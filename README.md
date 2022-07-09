# ThreadPool

A simple c++ threadpool implemented with a work stealing queue.

## Basic Usage
Copy threadpool.hpp and wsq.hpp to your source folder.

```
#include <iostream>
#include "threadpool.hpp"

void some_work() {
  std::cout << "Hello there from another thread!\n";
}

int main() {
  ThreadPool thread_pool = ThreadPool(); // by default uses the number of threads on your system
  
  thread_pool.push(some_work);
  thread_pool.push(some_work);
  thread_pool.push(some_work);
  thread_pool.push(some_work);

  thread_pool.join();
}
```

## Third Party
Uses taskflow work stealing queue: https://github.com/taskflow/work-stealing-queue
