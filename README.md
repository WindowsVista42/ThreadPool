# ThreadPool

A simple thread pool implemented with a work stealing queue.  
This thread pool is intended to be the backbone any higher-order concurrency an application might need.

## Requirements
C++ 17 and a recent C++ compiler.

## Basic Usage
Include threadpool.hpp, threadpool.cpp and wsq.hpp into your build system.

```c++
#include <iostream>
#include "threadpool.hpp"

void some_work() {
  std::cout << "Hello there from another thread!\n";
}

int main() {
  // create the thread pool with 4 threads and a work queue that can hold 1024 items
  ThreadPool* thread_pool = create_thread_pool(4, 1024);

  // push work onto the threadpool
  thread_pool_push(thread_pool, some_work);
  thread_pool_push(thread_pool, some_work);
  thread_pool_push(thread_pool, some_work);
  thread_pool_push(thread_pool, some_work);

  // begin working and wait for all threads to finish
  // runs some_work() printing "Hello there from another thread!" four times
  thread_pool_join(thread_pool);
  
  // deinit and clean up
  // not strictly necessary since a threadpool is likely going to live for the entire lifetime of the program
  destroy_thread_pool(thread_pool);
}
```
A more detailed usage can be found in [examples/test.cpp](examples/test.cpp).

## Third Party
Taskflow work stealing queue: https://github.com/taskflow/work-stealing-queue
