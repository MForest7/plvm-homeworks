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

    std::mutex receiver_mutex;
    std::condition_variable receiver_cv;

    void *parallelMemcpy(void *dst, const void *src, size_t size) {
        size_t block_size = std::max<size_t>(1024, (size + threads.size() - 1) / threads.size());
        size_t tasks_num = (size + block_size - 1) / block_size;

        std::unique_lock lock(worker_mutex);

        std::atomic_size_t waiting_for = tasks_num;
        for (size_t i = 0; i < tasks_num; i++) {
            void *task_dst = dst + block_size * i;
            const void *task_src = src + block_size * i;
            size_t task_size = std::min(size - i * block_size, block_size);
            tasks.push([task_dst, task_src, task_size, i, this, &waiting_for] {
                memcpy(task_dst, task_src, task_size);
                size_t rem = waiting_for.fetch_sub(1);
                {
                    std::unique_lock lock(receiver_mutex);
                    std::cout << "task " << i << " ends, remaining " << rem - 1 << std::endl;
                }
            });
        }

        worker_cv.notify_all();
        /*{
            std::cout << "wait..." << std::endl;
        }*/
        receiver_cv.wait(lock, [&waiting_for] {
            size_t rem = waiting_for.load();
            // std::cout << "remaining " << rem << std::endl;
            return rem == 0;
        });
        // std::cout << "return!!!" << std::endl;
        return dst;
    }

    ~ThreadPool() {
        destroy.test_and_set();
        worker_cv.notify_all();
    }
};

void *parallelMemcpy(void *dst, const void *src, size_t size, size_t threads_num) {
    std::vector<std::jthread> threads;
    size_t block_size = std::max<size_t>(1024, (size + threads_num) / threads_num);
    size_t tasks_num = (size + block_size - 1) / block_size;

    for (size_t i = 0; i < tasks_num; i++) {
        void *task_dst = dst + block_size * i;
        const void *task_src = src + block_size * i;
        size_t task_size = std::min(size - i * block_size, block_size);
        threads.emplace_back([task_dst, task_src, task_size] {
            memcpy(task_dst, task_src, task_size);
        });
    }
    return dst;
}

int main(const int argc, const char *argv[]) {
    if (argc < 1) {
        std::cerr << "Expected usage: ./build/parallel-memcpy <threads_num>" << std::endl;
        return 1;
    }
    size_t threads_num = atoi(argv[1]);

    ThreadPool thread_pool(atoi(argv[1]));

    const size_t N = 1 << 28;
    std::vector<int> src(N);
    for (size_t i = 0; i < N; i++) {
        src[i] = i;
    }

    std::vector<int> dst(N, 0);

    /*auto start = std::chrono::steady_clock::now();
    parallelMemcpy(dst.data(), src.data(), N * sizeof(int), threads_num);
    auto end = std::chrono::steady_clock::now();
    double time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    assert(memcmp(src.data(), dst.data(), N * sizeof(int)) == 0);
    std::cout << time << " us" << std::endl

    return 0;*/

    double total_time = 0;
    size_t laps = 100;
    for (size_t i = 0; i < laps; i++) {
        auto start = std::chrono::steady_clock::now();
        parallelMemcpy(dst.data(), src.data(), N * sizeof(int), threads_num);
        auto end = std::chrono::steady_clock::now();
        total_time += std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    }

    std::cout << total_time / laps << " us" << std::endl;
}
