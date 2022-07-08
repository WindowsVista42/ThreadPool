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
  num.fetch_add(1);
}

static DWORD WINAPI print_num(PVOID) {
  while(true) {
    printf("num: %llu\n", num.load());
    Sleep(1000);
  }
}

void print_time_info(std::vector<double>& times) {
  double largest_time = 0.0;
  double avg_time = 0.0;
  double smallest_time = 0.0;
  int x = 0;

  int abn_num = 0;

  for(auto& time: times) {
    if(time > largest_time) {
      largest_time = time;
      if(largest_time > 0.0005) {
        printf("Abnormal time at: %d, %lf s\n", x, time);
        abn_num += 1;
      }
    }
    if(time < smallest_time) {
      smallest_time = time;
    }
    avg_time += time;
    x += 1;
  }

  avg_time /= (double)times.size();

  std::sort(times.begin(), times.end(), std::greater<double>{});

  double p1_high_avg_time = 0.0;
  int p1 = (int)(times.size() * 0.01);
  for(int i = 0; i < p1; i +=1) {
    p1_high_avg_time += times[i];
  }

  p1_high_avg_time /= (double)p1;

  double p01_high_avg_time = 0.0;
  int p01 = (int)(times.size() * 0.001);
  for(int i = 0; i < p01; i +=1) {
    p01_high_avg_time += times[i];
  }

  p01_high_avg_time /= (double)p01;

  double std_dev = 0.0;
  int subc = 0;

  for(int i = 0; i < times.size(); i += 1) {
    if(times[i] > 0.0005) {
      subc += 1;
    } else {
      std_dev += (times[i] - avg_time) * (times[i] - avg_time);
    }
  }

  std_dev /= (double)(times.size() - subc);
  std_dev = sqrt(std_dev);

  printf("largest_time: %lf\n", largest_time);
  printf("smallest_time: %lf\n", smallest_time);
  printf("avg_time: %lf\n", avg_time);
  printf("p1_high_avg_time: %lf\n", p1_high_avg_time);
  printf("p01_high_avg_time: %lf\n", p01_high_avg_time);
  printf("std_dev: %lf\n", std_dev);
  printf("number abnormal: %d\n", abn_num);
  printf("\n");

  times.clear();
}

int main() {
  ThreadPool thread_pool = ThreadPool(std::thread::hardware_concurrency() * 0.8);

  HANDLE thread_print_num = CreateThread(0, 0, print_num, 0, 0, 0);

  std::vector<double> times;

  for(int i = 0; i < 20000000; i += 1) {
    auto t0 = std::chrono::high_resolution_clock::now();
    thread_pool.add_work(test_short_function);
    thread_pool.begin();
    thread_pool.add_work(test_short_function);
    thread_pool.add_work(test_short_function);
    thread_pool.add_work(test_short_function);
    thread_pool.join();
    auto t1 = std::chrono::high_resolution_clock::now();
    times.push_back(std::chrono::duration<double>(t1 - t0).count());
  }

  printf("done\n");
  print_time_info(times);
}
