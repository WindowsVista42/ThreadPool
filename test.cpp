#include "threadpool.hpp"
#include <algorithm>

static void test_function() {
  printf("doing work!\n");
}

static void test_function2() {
  Sleep(1000);
  printf("thing12\n");
}

static std::atomic_int64_t num(0);
static void test_short_function() {
  //num.fetch_add(1);
  //for(int i = 0; i < 5; i ++) {
  //  num.fetch_add(rand() % 10);
  //  num.fetch_add(rand() % 10);
  //  num.fetch_add(rand() % 10);
  //  num.fetch_add(rand() % 10);
  //  num.fetch_add(rand() % 10);
  //  num.fetch_add(rand() % 10);
  //  num.fetch_add(rand() % 10);
  //  num.fetch_add(rand() % 10);
  //  num.fetch_add(rand() % 10);
  //  num.fetch_add(rand() % 10);
  //  num.fetch_add(rand() % 10);
  //  num.fetch_add(rand() % 10);
  //  num.fetch_add(rand() % 10);
  //  num.fetch_add(rand() % 10);
  //  num.fetch_add(rand() % 10);
  //  num.fetch_add(rand() % 10);
  //  num.fetch_add(rand() % 10);
  //}
  return;
}

static DWORD WINAPI print_num(PVOID) {
  while(true) {
    //printf("num: %llu\n", num);
    //printf("num: %llu\n", num.load());
    Sleep(1000);
  }
}

void print_time_info(std::vector<double>& times) {
  double largest_time = 0.0;
  double avg_time = 0.0;
  double smallest_time = 0.0;
  long x = 0;

  long count = times.size();

  std::vector<double> valid;

  for(auto& time: times) {
    if(time > 0.0005) {
      continue;
    }
    if(time > largest_time) {
      largest_time = time;
    }
    if(time < smallest_time) {
      smallest_time = time;
    }
    avg_time += time;
    x += 1;
    valid.push_back(time);
  }

  avg_time /= (double)count;

  std::sort(valid.begin(), valid.end(), std::greater<double>{});

  double p1_high_avg_time = 0.0;
  int p1 = (int)(valid.size() * 0.01);
  for(int i = 0; i < p1; i +=1) {
    p1_high_avg_time += valid[i];
  }

  p1_high_avg_time /= (double)p1;

  double p01_high_avg_time = 0.0;
  int p01 = (int)(valid.size() * 0.001);
  for(int i = 0; i < p01; i +=1) {
    p01_high_avg_time += valid[i];
  }

  p01_high_avg_time /= (double)p01;

  double std_dev = 0.0;
  int subc = 0;

  for(int i = 0; i < valid.size(); i += 1) {
    if(valid[i] > 0.0005) {
      subc += 1;
    } else {
      std_dev += (valid[i] - avg_time) * (valid[i] - avg_time);
    }
  }

  std_dev /= (double)(valid.size() - subc);
  std_dev = sqrt(std_dev);

  printf("largest_time: %.16lf\n", largest_time);
  printf("smallest_time: %.16lf\n", smallest_time);
  printf("avg_time: %.16lf\n", avg_time);
  printf("p1_high_avg_time: %.16lf\n", p1_high_avg_time);
  printf("p01_high_avg_time: %.16lf\n", p01_high_avg_time);
  printf("std_dev: %.16lf\n", std_dev);
  printf("\n");

  valid.clear();
}

int main() {
  auto THREAD_COUNT = std::thread::hardware_concurrency() * 0.8;
  ThreadPool thread_pool = ThreadPool(THREAD_COUNT);

  HANDLE thread_print_num = CreateThread(0, 0, print_num, 0, 0, 0);

  std::vector<double> times;

  for(int i = 0; i < 10000000; i += 1) {
    auto t0 = std::chrono::high_resolution_clock::now();
    for(int i = 0; i < 4; i += 1) {
      thread_pool.add_work(test_short_function);
    }

    thread_pool.join();
    auto t1 = std::chrono::high_resolution_clock::now();
    times.push_back(std::chrono::duration<double>(t1 - t0).count());
  }

  printf("done\n");
  print_time_info(times);
}
