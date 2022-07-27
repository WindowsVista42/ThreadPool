#pragma once

#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "wsq.hpp"

class ThreadPool {
  struct mutex;
  struct cvar;

  // Threads
  std::int64_t _thread_count;
  std::thread* _threads;

  std::mutex _work_start_mutex;
  std::condition_variable _work_start_cvar;

  std::mutex _work_done_mutex;
  std::condition_variable _work_done_cvar;

  std::atomic_int64_t _initialized_count;
  std::atomic_int64_t _working_count;
  std::atomic_int64_t _waiting_count;
  std::atomic_bool _done_spin_lock;
  std::atomic_bool _should_notify;
  std::atomic_bool _stop_working;

  using work = void (*)();
  WorkStealingQueue<work> _work_queue;

  static void thread_main(void* data) {
    ThreadPool* pool = (ThreadPool*)data;
    pool->_initialized_count.fetch_add(1);

    auto start_lock = std::unique_lock<std::mutex>(pool->_work_start_mutex, std::defer_lock);

    while(true) {
      start_lock.lock();
      pool->_waiting_count.fetch_add(1); // tell main thread we are waiting

      // sleep until we have work to do or we need to exit
      while(pool->_work_queue.empty() && !pool->_stop_working.load()) {
        pool->_work_start_cvar.wait(start_lock);
      }

      start_lock.unlock();
      pool->_waiting_count.fetch_sub(1); // tell main thread we are no longer waiting
                                                                    //
      // exit the thread
      if(pool->_stop_working.load()) {
        break;
      }

      // grab work while there is work remaining and run it
      auto work = pool->_work_queue.steal();
      if(work.has_value()) {
        pool->_working_count.fetch_add(1);

        work.value()();

        work = pool->_work_queue.steal();
        while(work.has_value()) {
          work.value()();
          work = pool->_work_queue.steal();
        }

        pool->_working_count.fetch_sub(1);
      }

      // if we are the last thread to finish, tell the main thread that
      // all threads have finished
      if(pool->_work_queue.empty() && pool->_working_count.load() == 0) {
        // spin lock until we have confirmation from the main thread that
        // it knows we are done working
        while(pool->_should_notify.load() && !pool->_done_spin_lock.load() && pool->_working_count.load() == 0 && pool->_work_queue.empty()) {
          pool->_work_done_cvar.notify_all();
        }
      }
    }

    pool->_initialized_count.fetch_sub(1);
  }

  // API
  public:
  ThreadPool() {}
  ~ThreadPool() {}

  // Init a thread pool with the specified (power of two) queue_size and thread_count
  void init(int64_t thread_count = std::thread::hardware_concurrency(), int64_t queue_size = 1024) {
    // init internals
    this->_initialized_count.store(0);
    this->_working_count.store(0);
    this->_waiting_count.store(0);
    this->_done_spin_lock.store(false);
    this->_should_notify.store(true);
    this->_stop_working.store(false);

    // create threads
    this->_thread_count = thread_count;
    this->_threads = new std::thread[this->_thread_count];
    for(std::int64_t i = 0; i < this->_thread_count; i += 1) {
      this->_threads[i] = std::thread(thread_main, this);
      this->_threads[i].detach();
    }

    // spin lock until all threads are created (required for synchronization)
    while(this->_waiting_count.load() != this->_thread_count) {}
  }

  // Deinit a thread pool, waiting for all threads to complete work
  void deinit() {
    this->join();
    // wait until all threads are waiting
    while(this->_waiting_count.load() != this->_thread_count) {}
    this->_stop_working.store(true);

    // spin lock until all threads are going to quit, and spam notify to
    // make sure they all get the message
    while(this->_initialized_count.load() != 0) {
      this->_work_start_cvar.notify_all();
    }

    delete[] this->_threads;
  }

  // Push work onto the threadpools queue
  void push(work w) {
    this->_work_queue.push(w);
  }

  // Query if the threadpool has finished the current batch of work
  bool finished() const {
    return this->_work_queue.empty() && this->_working_count.load() == 0;
  }

  // Begin working but dont wait on threads to complete (defer joining to a later stage)
  void start() {
    this->_should_notify.store(false);

    auto wq_size = this->_work_queue.size();
    if(wq_size < this->_thread_count) {
      for(std::int64_t i = 0; i < wq_size; i += 1) {
        this->_work_start_cvar.notify_one();
      }
    } else {
      this->_work_start_cvar.notify_all();
    }
  }

  // Begin working and wait until all threads have finished their work
  void join() {
    this->_should_notify.store(true);

    // tell threads to begin working
    auto wq_size = this->_work_queue.size();
    if(wq_size < this->_thread_count) {
      for(std::int64_t i = 0; i < wq_size; i += 1) {
        this->_work_start_cvar.notify_one();
      }
    } else {
      this->_work_start_cvar.notify_all();
    }

    // sleep main thread until work is done
    auto done_lock = std::unique_lock<std::mutex>(this->_work_done_mutex, std::defer_lock);
    done_lock.lock();

    while(!this->_work_queue.empty() || this->_working_count.load() != 0) {
      this->_work_done_cvar.wait(done_lock);
    }
    done_lock.unlock();

    // notify the notifying thread that we are awakened
    this->_done_spin_lock.store(true);

    // wait until all threads have gone back to the waiting state
    while(this->_waiting_count.load() != this->_thread_count) {}

    // reset previous spin lock state
    this->_done_spin_lock.store(false);
  }

  int64_t thread_count() const {
    return _thread_count;
  }
};
