#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "wsq.hpp"

#include "threadpool.hpp"

struct ThreadPool {
  std::int64_t thread_count;
  std::thread* threads;

  std::mutex work_start_mutex;
  std::condition_variable work_start_cvar;

  std::mutex work_done_mutex;
  std::condition_variable work_done_cvar;

  std::atomic_int64_t initialized_count;
  std::atomic_int64_t working_count;
  std::atomic_int64_t waiting_count;
  std::atomic_bool done_spin_lock;
  std::atomic_bool should_notify;
  std::atomic_bool stop_working;

  WorkStealingQueue<VoidFunctionPtr> work_queue;

  ThreadPool(int64_t queue_size) :
    work_queue(queue_size)
  {};

  ~ThreadPool() {}
};

static void thread_main(void* data) {
  ThreadPool* pool = (ThreadPool*)data;
  pool->initialized_count.fetch_add(1);

  auto start_lock = std::unique_lock<std::mutex>(pool->work_start_mutex, std::defer_lock);

  while(true) {
    start_lock.lock();
    pool->waiting_count.fetch_add(1); // tell main thread we are waiting

    // sleep until we have work to do or we need to exit
    while(pool->work_queue.empty() && !pool->stop_working.load()) {
      pool->work_start_cvar.wait(start_lock);
    }

    start_lock.unlock();
    pool->waiting_count.fetch_sub(1); // tell main thread we are no longer waiting
                                                                  //
    // exit the thread
    if(pool->stop_working.load()) {
      break;
    }

    // grab work while there is work remaining and run it
    auto work = pool->work_queue.steal();
    if(work.has_value()) {
      pool->working_count.fetch_add(1);

      work.value()();

      work = pool->work_queue.steal();
      while(work.has_value()) {
        work.value()();
        work = pool->work_queue.steal();
      }

      pool->working_count.fetch_sub(1);
    }

    // if we are the last thread to finish, tell the main thread that
    // all threads have finished
    if(pool->work_queue.empty() && pool->working_count.load() == 0) {
      // spin lock until we have confirmation from the main thread that
      // it knows we are done working
      while(pool->should_notify.load() && !pool->done_spin_lock.load() && pool->working_count.load() == 0 && pool->work_queue.empty()) {
        pool->work_done_cvar.notify_all();
      }
    }
  }

  pool->initialized_count.fetch_sub(1);
}

ThreadPool* create_thread_pool(int thread_count, int queue_size) {
  ThreadPool* thread_pool = new ThreadPool(queue_size);

  // init internals
  thread_pool->initialized_count.store(0);
  thread_pool->working_count.store(0);
  thread_pool->waiting_count.store(0);
  thread_pool->done_spin_lock.store(false);
  thread_pool->should_notify.store(true);
  thread_pool->stop_working.store(false);

  // create threads
  thread_pool->thread_count = thread_count;
  thread_pool->threads = new std::thread[thread_pool->thread_count];
  for(std::int64_t i = 0; i < thread_pool->thread_count; i += 1) {
    thread_pool->threads[i] = std::thread(thread_main, thread_pool);
    thread_pool->threads[i].detach();
  }

  // spin lock until all threads are created (required for synchronization)
  while(thread_pool->waiting_count.load() != thread_pool->thread_count) {}

  return thread_pool;
}

void destroy_thread_pool(ThreadPool* thread_pool_ptr) {
  ThreadPool* thread_pool = (ThreadPool*)thread_pool_ptr;

  thread_pool_join(thread_pool);

  // wait until all threads are waiting
  while(thread_pool->waiting_count.load() != thread_pool->thread_count) {}
  thread_pool->stop_working.store(true);

  // spin lock until all threads are going to quit, and spam notify to
  // make sure they all get the message
  while(thread_pool->initialized_count.load() != 0) {
    thread_pool->work_start_cvar.notify_all();
  }

  delete[] thread_pool->threads;
  delete thread_pool;
}

void thread_pool_push(ThreadPool* thread_pool_ptr, VoidFunctionPtr work_function) {
  ThreadPool* thread_pool = (ThreadPool*)thread_pool_ptr;

  thread_pool->work_queue.push(work_function);
}

bool thread_pool_is_finished(ThreadPool* thread_pool_ptr) {
  ThreadPool* thread_pool = (ThreadPool*)thread_pool_ptr;

  return thread_pool->work_queue.empty() && thread_pool->working_count.load() == 0;
}

void thread_pool_start(ThreadPool* thread_pool_ptr) {
  ThreadPool* thread_pool = (ThreadPool*)thread_pool_ptr;

  thread_pool->should_notify.store(false);

  std::int64_t wq_size = thread_pool->work_queue.size();
  if(wq_size < thread_pool->thread_count) {
    for(std::int64_t i = 0; i < wq_size; i += 1) {
      thread_pool->work_start_cvar.notify_one();
    }
  } else {
    thread_pool->work_start_cvar.notify_all();
  }
}

void thread_pool_join(ThreadPool* thread_pool_ptr) {
  ThreadPool* thread_pool = (ThreadPool*)thread_pool_ptr;

  // wait until all threads have gone back to the waiting state
  //while(thread_pool->_waiting_count.load() != thread_pool->_thread_count) {}

  thread_pool->should_notify.store(true);

  // tell threads to begin working
  std::int64_t wq_size = thread_pool->work_queue.size();
  if(wq_size < thread_pool->thread_count) {
    for(std::int64_t i = 0; i < wq_size; i += 1) {
      thread_pool->work_start_cvar.notify_one();
    }
  } else {
    thread_pool->work_start_cvar.notify_all();
  }

  // sleep main thread until work is done
  auto done_lock = std::unique_lock<std::mutex>(thread_pool->work_done_mutex, std::defer_lock);
  done_lock.lock();

  while(!thread_pool->work_queue.empty() || thread_pool->working_count.load() != 0) {
    thread_pool->work_done_cvar.wait(done_lock);
  }
  done_lock.unlock();

  // notify the notifying thread that we are awakened
  thread_pool->done_spin_lock.store(true);

  // wait until all threads have gone back to the waiting state
  while(thread_pool->waiting_count.load() != thread_pool->thread_count) {}

  // reset previous spin lock state
  thread_pool->done_spin_lock.store(false);
}

int thread_pool_thread_count(ThreadPool* thread_pool_ptr) {
  return ((ThreadPool*)thread_pool_ptr)->thread_count;
}
