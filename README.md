# ThreadPool

A simple c++ threadpool implemented with a work stealing queue.  
This implementation is used for a person game engine project so it makes certain concessions, primarily using `.init()` and `.deinit()` over constructors and destructors, and using only `void(*)()` type functions.  
This implementation also aims to keep latencies low and may not be the most energy efficient due to using a few strategic busy waits.

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
  ThreadPool thread_pool;
  thread_pool.init();

  // add work to the threadpool
  thread_pool.push(some_work);
  thread_pool.push(some_work);
  thread_pool.push(some_work);
  thread_pool.push(some_work);

  // begin working and wait for all threads to finish
  // runs some_work() printing "Hello there from another thread!" four times
  thread_pool.join();
  
  // deinit and clean up, not really necessary but it's more 'proper'
  thread_pool.deinit();
}
```

## Third Party
Uses taskflow work stealing queue: https://github.com/taskflow/work-stealing-queue
