#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <list>
#include <mutex>
#include <new>
#include <queue>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <thread>
#include <unistd.h>
#include <vector>

class ThreadPool {
public:
    ThreadPool(size_t threads_num) {
        for (size_t i = 0; i < threads_num; i++) {
            threads.emplace_back([this, i] {
                std::unique_lock lock(worker_mutex);

                while (true) {
                    worker_cv.wait(lock);
                    if (destroy.test()) {
                        lock.unlock();
                        break;
                    }
                    if (i >= alive_threads) {
                        continue;
                    }
                    lock.unlock();

                    memcpy(thread_dst[i], thread_src[i], thread_size[i]);

                    lock.lock();
                    waiting_for.fetch_sub(1);
                    receiver_cv.notify_one();
                }
            });
        }

        thread_dst.resize(threads_num, nullptr);
        thread_src.resize(threads_num, nullptr);
        thread_size.resize(threads_num, 0);
    }

    std::vector<std::jthread> threads;
    std::queue<std::function<void()>> tasks;
    std::atomic_flag destroy;

    std::mutex worker_mutex;
    std::condition_variable worker_cv;

    std::condition_variable receiver_cv;
    std::atomic_size_t alive_threads;
    std::atomic_size_t waiting_for;

    std::vector<void *> thread_dst;
    std::vector<const void *> thread_src;
    std::vector<size_t> thread_size;

    void *parallelMemcpy(void *dst, const void *src, size_t size, size_t threads_num) {
        threads_num = std::min(threads.size(), threads_num);

        alive_threads.store(threads_num);
        size_t block_size = std::max<size_t>(1024, (size + threads_num) / (threads_num + 1));
        size_t tasks_num = (size + block_size - 1) / block_size;

        std::unique_lock lock(worker_mutex);

        waiting_for = tasks_num - 1;

        for (size_t i = 0; i + 1 < tasks_num; i++) {
            thread_dst[i] = reinterpret_cast<uint8_t *>(dst) + block_size * i;
            thread_src[i] = reinterpret_cast<const uint8_t *>(src) + block_size * i;
            thread_size[i] = std::min(size - i * block_size, block_size);
        }

        if (tasks_num > 1) {
            worker_cv.notify_all();
        }

        lock.unlock();
        {
            void *task_dst = reinterpret_cast<uint8_t *>(dst) + block_size * (tasks_num - 1);
            const void *task_src = reinterpret_cast<const uint8_t *>(src) + block_size * (tasks_num - 1);
            size_t task_size = std::min(size - (tasks_num - 1) * block_size, block_size);
            memcpy(task_dst, task_src, task_size);
        }
        lock.lock();

        while (tasks_num > 1 && waiting_for.load()) {
            receiver_cv.wait(lock);
        }
        return dst;
    }

    ~ThreadPool() {
        destroy.test_and_set();
        worker_cv.notify_all();
    }
};

constexpr size_t MAX_THREADS = 8;
ThreadPool thread_pool(MAX_THREADS);

void run_with_n_threads(size_t threads_num) {
    constexpr size_t N = 1 << 26;
    std::vector<int> src(N);
    for (size_t i = 0; i < N; i++) {
        src[i] = i;
    }

    std::vector<int> dst(N, 0);

    thread_pool.parallelMemcpy(dst.data(), src.data(), N * sizeof(int), threads_num);
    assert(memcmp(src.data(), dst.data(), N * sizeof(int)) == 0);

    double total_time = 0;
    constexpr size_t laps = 10;
    for (size_t i = 0; i < laps; i++) {
        auto start = std::chrono::steady_clock::now();
        thread_pool.parallelMemcpy(dst.data(), src.data(), N * sizeof(int), threads_num);
        auto end = std::chrono::steady_clock::now();
        total_time += std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    }

    std::cout << threads_num << " threads: " << total_time / laps << " us" << std::endl;
}

int main(const int argc, const char *argv[]) {
    for (size_t threads_num = 0; threads_num <= 8; threads_num++) {
        run_with_n_threads(threads_num);
    }
}
