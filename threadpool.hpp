#pragma once

using VoidFunctionPtr = void (*)();
struct ThreadPool;

// Create a thread pool with the specified thread_count and specified (power of two) queue_size
ThreadPool* create_thread_pool(int thread_count, int queue_size = 1024);

// Destroy a thread pool, waiting for all threads to complete work
void destroy_thread_pool(ThreadPool* thread_pool);

// Push a work function into the threadpools queue
void thread_pool_push(ThreadPool* thread_pool, VoidFunctionPtr work_function_ptr);

// Query if the threadpool has finished the current batch of work
bool thread_pool_is_finished(ThreadPool* thread_pool);

// Tell the threadpool to begin working but dont wait on threads to complete (defer joining to a later stage)
void thread_pool_start(ThreadPool* thread_pool);

// Tell the threadpool to begin working and wait until all threads have finished their work
// If the threadpool is already working then this function waits until all the threads have finished their work
void thread_pool_join(ThreadPool* thread_pool);

// Returns the number of threads committed to this threadpool
int thread_pool_thread_count(ThreadPool* thread_pool);
