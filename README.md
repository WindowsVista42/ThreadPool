# ThreadPool

A simple c++ threadpool implemented with a work stealing queue.

## Requirements
C++ 17 and a recent c++ compiler.

## Basic Usage
Copy or include threadpool.hpp and wsq.hpp into your working directory.

```
#include <iostream>
#include "threadpool.hpp"

void some_work() {
  std::cout << "Hello there from another thread!\n";
}

int main() {
  // create the thread pool with a work queue size of 1024 and 
  // std::thread::hardware_concurrency() number of threads
  ThreadPool thread_pool = ThreadPool();

  // add work to the threadpool
  thread_pool.push(some_work);
  thread_pool.push(some_work);
  thread_pool.push(some_work);
  thread_pool.push(some_work);

  // begin working and wait for all threads to finish
  thread_pool.join();
}
```

## Third Party
Uses taskflow work stealing queue: https://github.com/taskflow/work-stealing-queue
