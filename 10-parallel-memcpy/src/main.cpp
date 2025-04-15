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
                lock.unlock();

                while (true) {
                    lock.lock();
                    if (tasks.empty()) {
                        worker_cv.wait(lock, [this] { return destroy.test() || !tasks.empty(); });
                        if (destroy.test()) {
                            lock.unlock();
                            break;
                        }
                    }
                    auto task = tasks.front();
                    tasks.pop();

                    lock.unlock();

                    task();
                    receiver_cv.notify_one();
                }
            });
        }
    }

    std::vector<std::jthread> threads;
    std::queue<std::function<void()>> tasks;
    std::atomic_flag destroy;

    std::mutex worker_mutex;
    std::condition_variable worker_cv;

    std::condition_variable receiver_cv;

    void *parallelMemcpy(void *dst, const void *src, size_t size) {
        size_t block_size = std::max<size_t>(1024, (size + threads.size()) / (threads.size() + 1));
        size_t tasks_num = (size + block_size - 1) / block_size;

        std::unique_lock lock(worker_mutex);

        std::atomic_size_t waiting_for = tasks_num - 1;
        for (size_t i = 0; i + 1 < tasks_num; i++) {
            void *task_dst = reinterpret_cast<uint8_t *>(dst) + block_size * i;
            const void *task_src = reinterpret_cast<const uint8_t *>(src) + block_size * i;
            size_t task_size = std::min(size - i * block_size, block_size);
            tasks.push([task_dst, task_src, task_size, this, &waiting_for] {
                memcpy(task_dst, task_src, task_size);
                size_t rem = waiting_for.fetch_sub(1);
                if (rem - 1 == 0) {
                    receiver_cv.notify_one();
                }
            });
        }

        {
            void *task_dst = reinterpret_cast<uint8_t *>(dst) + block_size * (tasks_num - 1);
            const void *task_src = reinterpret_cast<const uint8_t *>(src) + block_size * (tasks_num - 1);
            size_t task_size = std::min(size - (tasks_num - 1) * block_size, block_size);
            memcpy(task_dst, task_src, task_size);
        }

        if (tasks_num > 1) {
            worker_cv.notify_all();
            receiver_cv.wait(lock, [&waiting_for] {
                size_t rem = waiting_for.load();
                return rem == 0;
            });
        }
        return dst;
    }

    ~ThreadPool() {
        destroy.test_and_set();
        worker_cv.notify_all();
    }
};

void run_with_n_threads(size_t threads_num) {
    ThreadPool thread_pool(threads_num);

    constexpr size_t N = 1 << 26;
    std::vector<int> src(N);
    for (size_t i = 0; i < N; i++) {
        src[i] = i;
    }

    std::vector<int> dst(N, 0);

    thread_pool.parallelMemcpy(dst.data(), src.data(), N * sizeof(int));
    assert(memcmp(src.data(), dst.data(), N * sizeof(int)) == 0);

    double total_time = 0;
    constexpr size_t laps = 10;
    for (size_t i = 0; i < laps; i++) {
        auto start = std::chrono::steady_clock::now();
        thread_pool.parallelMemcpy(dst.data(), src.data(), N * sizeof(int));
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
