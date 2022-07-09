#pragma once

#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

#if defined(_WIN32) || defined(_WIN64)
  #define __THREADPOOL_USE_WINDOWS__
  #include <Windows.h>
#endif

#include "wsq.hpp"

class ThreadPool {
  struct mutex;
  struct cvar;

#ifdef __THREADPOOL_USE_WINDOWS__
  struct mutex {
    CRITICAL_SECTION _value;

    mutex() {
      InitializeCriticalSection(&this->_value);
    }

    void lock() {
      EnterCriticalSection(&this->_value);
    }

    void unlock() {
      LeaveCriticalSection(&this->_value);
    }
  };

  struct cvar {
    CONDITION_VARIABLE _value;

    cvar() {
      InitializeConditionVariable(&this->_value);
    }

    void sleep(mutex* m) {
      SleepConditionVariableCS(&this->_value, (CRITICAL_SECTION*)m, INFINITE);
    }

    void wake_one() {
      WakeConditionVariable(&this->_value);
    }

    void wake_all() {
      WakeAllConditionVariable(&this->_value);
    }
  };

  struct thread {
    HANDLE _value;

    thread() {}
    void create(DWORD (*thread_main)(PVOID), void* data) {
      this->_value = CreateThread(0, 0, thread_main, data, 0, 0);
    }
    void terminate() {
      TerminateThread(this->_value, 0);
    }
  };
#endif

  // Threads
  std::int64_t _thread_count;
  thread* _threads;

  // Synchronization
  mutex _work_start_mutex;
  cvar _work_start_cvar;

  mutex _work_done_mutex;
  cvar _work_done_cvar;

  std::atomic_int64_t _working_count;
  std::atomic_int64_t _waiting_count;
  std::atomic_bool _done_spin_lock;
  std::atomic_bool _should_notify;

  using work = void (*)();
  WorkStealingQueue<work> _work_queue;

#ifdef __THREADPOOL_USE_WINDOWS__
  static DWORD WINAPI thread_main(PVOID data) {
    ThreadPool* pool = (ThreadPool*)data;
    printf("thread here!\n");

    while(true) {
      pool->_work_start_mutex.lock();
      pool->_waiting_count.fetch_add(1, std::memory_order_relaxed); // tell main thread we are waiting

      // sleep until we have work to do
      while(pool->_work_queue.empty()) {
        pool->_work_start_cvar.sleep(&pool->_work_start_mutex);
      }

      pool->_work_start_mutex.unlock();
      pool->_waiting_count.fetch_sub(1, std::memory_order_relaxed); // tell main thread we are no longer waiting

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
      if(pool->_work_queue.empty() && pool->_working_count.load(std::memory_order_relaxed) == 0) {
        // spin lock until we have confirmation from the main thread that
        // it knows we are done working
        while(pool->_should_notify.load(std::memory_order_relaxed) && !pool->_done_spin_lock.load(std::memory_order_relaxed)) {
          pool->_work_done_cvar.wake_all();
        }
      }
    }

    return 0;
  }
#endif

  static DWORD WINAPI debug(PVOID data) {
    ThreadPool* pool = (ThreadPool*)data;

    while(true) {
      printf("_thread_count: %lld\n", pool->_thread_count);
      printf("_working_count: %lld\n", pool->_working_count.load());
      printf("_waiting_count: %lld\n", pool->_waiting_count.load());
      printf("_done_spin_lock: %d\n", pool->_done_spin_lock.load() ? 1 : 0);
      printf("_should_notify: %d\n", pool->_should_notify.load() ? 1 : 0);

      Sleep(500);
    }
  }

  static inline thread dbg_thread;

  // API
  public:
  ThreadPool(int64_t queue_size = 1024, int64_t thread_count = std::thread::hardware_concurrency()) :
    _work_queue(queue_size)
  {
    this->_work_done_mutex = mutex();
    this->_work_start_cvar = cvar();

    this->_work_done_mutex = mutex();
    this->_work_done_cvar = cvar();

    this->_working_count.store(0);
    this->_waiting_count.store(0);
    this->_done_spin_lock.store(false);
    this->_should_notify.store(true);

    // create threads
    this->_thread_count = thread_count;
    this->_threads = (thread*)malloc(sizeof(thread) * this->_thread_count);
    for(std::int64_t i = 0; i < this->_thread_count; i += 1) {
      this->_threads[i].create(thread_main, this);
    }

    //dbg_thread.create(debug, this);

    // spin lock until all threads are created (required for synchronization)
    while(this->_waiting_count.load() != this->_thread_count) {}
    //  printf("here %lld!\n", this->_waiting_count.load());
    //}
  }

  ~ThreadPool() {
    for(std::int64_t i = 0; i < this->_thread_count; i += 1) {
      this->_threads[i].terminate();
    }

    free(this->_threads);
  }

  using work_id = std::int64_t;
  work_id push(work w) {
    this->_work_queue.push(w);
    return 0;
  }

  bool finished() {
    return this->_work_queue.empty() && this->_working_count.load() == 0;
  }

  void join() {
    this->_should_notify.store(true, std::memory_order_relaxed);

    // tell threads to begin working
    auto wq_size = this->_work_queue.size();
    if(wq_size < this->_thread_count) {
      for(std::int64_t i = 0; i < wq_size; i += 1) {
        this->_work_start_cvar.wake_one();
      }
    } else {
      this->_work_start_cvar.wake_all();
    }

    // sleep main thread until work is done
    this->_work_done_mutex.lock();
    while(!this->_work_queue.empty() && this->_working_count.load(std::memory_order_relaxed) != 0) {
      this->_work_done_cvar.sleep(&this->_work_done_mutex);
    }
    this->_work_done_mutex.unlock();

    // notify the notifying thread that we are awakened
    this->_done_spin_lock.store(true, std::memory_order_relaxed);

    // wait until all threads have gone back to the waiting state
    while(this->_waiting_count.load(std::memory_order_relaxed) != this->_thread_count) {}

    // reset previous spin lock state
    this->_done_spin_lock.store(false, std::memory_order_relaxed);
  }

  void start() {
    this->_should_notify.store(false);

    auto wq_size = this->_work_queue.size();
    if(wq_size < this->_thread_count) {
      for(std::int64_t i = 0; i < wq_size; i += 1) {
        this->_work_start_cvar.wake_one();
      }
    } else {
      this->_work_start_cvar.wake_all();
    }
  }

  // add some kind of ---thin--- way to
  // query if some *specific* work in the queue has finished
};
