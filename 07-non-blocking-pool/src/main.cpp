#include <algorithm>
#include <atomic>
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

using namespace std;

struct MemPool {
    atomic<void *> begin;
    atomic<void *> end;
};

static constexpr size_t POOL_LIMIT = 256;

struct {
    MemPool known_pools[POOL_LIMIT];

    inline size_t add(void *begin, void *end) {
        for (size_t slot = 0; slot < POOL_LIMIT; slot++) {
            void *expected = nullptr;
            if (known_pools[slot].begin.compare_exchange_strong(expected, begin, memory_order_relaxed)) {
                known_pools[slot].end.store(end);
                return slot;
            }
        }
        return -1;
    }

    inline void remove(size_t slot) {
        known_pools[slot].begin.store(nullptr, memory_order_relaxed);
    }

} pool_registry;

static void (*segv_handler)(int signo, siginfo_t *info, void *extra);

static void alloc_handler(int signo, siginfo_t *info, void *extra) {
    auto failed_ptr = info->si_addr;
    for (size_t i = 0; i < POOL_LIMIT; i++) {
        void *begin = pool_registry.known_pools[i].begin.load(memory_order_relaxed);
        if (begin != nullptr && begin <= failed_ptr) {
            void *end = pool_registry.known_pools[i].end.load(memory_order_relaxed);
            if (begin <= failed_ptr && failed_ptr < end) {
                constexpr const char message[] = "Bad alloc in PoolAllocator: pool begins at 0x";
                write(STDERR_FILENO, message, sizeof(message) - 1);
                char hd[2 * sizeof(uintptr_t)];
                uintptr_t pool_ptr = reinterpret_cast<uintptr_t>(begin);
                for (size_t j = 0; j < 2 * sizeof(uintptr_t); j++) {
                    char next_digit = '0' + (pool_ptr & 15);
                    if (next_digit > '9') {
                        next_digit = 'a' + (pool_ptr & 15) - 10;
                    }
                    hd[sizeof(hd) - 1 - j] = next_digit;
                    pool_ptr >>= 4;
                }

                write(STDERR_FILENO, hd, sizeof(hd));
                write(STDERR_FILENO, "\n", 1);
                exit(3);
            }
        }
    }

    segv_handler(signo, info, extra);
}

template <typename Object>
class PoolAllocator {
    uint8_t *begin;
    uint8_t *end;
    size_t slot;
    uint8_t *first_free;

public:
    PoolAllocator(size_t capacity) {
        size_t page_size = sysconf(_SC_PAGE_SIZE);
        size_t protected_pages = (sizeof(Object) + page_size - 1) / page_size;
        size_t pool_size = page_size * protected_pages + capacity * sizeof(Object);

        begin = reinterpret_cast<uint8_t *>(
            mmap(nullptr, pool_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0));

        if (begin == MAP_FAILED) {
            throw std::bad_alloc();
        }

        mprotect(begin, page_size * protected_pages, PROT_NONE);

        end = begin + pool_size;
        first_free = end;

        slot = pool_registry.add(begin, end);
        if (slot == -1) {
            throw runtime_error("No free pool slot");
        }
    }

    Object *allocate(size_t n) {
        return reinterpret_cast<Object *>(first_free -= sizeof(Object));
    }

    void deallocate(Object *ptr, size_t n) {}

    ~PoolAllocator() {
        pool_registry.remove(slot);
        munmap(begin, (size_t)end - (size_t)begin);
    }
};

template <typename Object>
class LockedPoolAllocator {
    uint8_t *begin;
    uint8_t *end;
    size_t slot;
    uint8_t *first_free;
    mutex lock;

public:
    LockedPoolAllocator(size_t capacity) {
        size_t page_size = sysconf(_SC_PAGE_SIZE);
        size_t protected_pages = (sizeof(Object) + page_size - 1) / page_size;
        size_t pool_size = page_size * protected_pages + capacity * sizeof(Object);
        pool_size = (pool_size + page_size - 1) & (~(page_size - 1));

        begin = reinterpret_cast<uint8_t *>(
            mmap(nullptr, pool_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0));

        if (begin == MAP_FAILED) {
            throw std::bad_alloc();
        }

        mprotect(begin, page_size * protected_pages, PROT_NONE);

        end = begin + pool_size;
        first_free = end;

        slot = pool_registry.add(begin, end);
        if (slot == -1) {
            throw runtime_error("No free pool slot");
        }
    }

    Object *allocate(size_t n) {
        lock.lock();
        Object *ptr = reinterpret_cast<Object *>(first_free -= sizeof(Object));
        lock.unlock();
        return ptr;
    }

    void deallocate(Object *ptr, size_t n) {}

    ~LockedPoolAllocator() {
        pool_registry.remove(slot);
        munmap(begin, (size_t)end - (size_t)begin);
    }
};

template <typename Object>
class LockFreePoolAllocator {
    uint8_t *begin;
    uint8_t *end;
    size_t slot;
    atomic_uint64_t first_free;

public:
    LockFreePoolAllocator(size_t capacity) {
        size_t page_size = sysconf(_SC_PAGE_SIZE);
        size_t protected_pages = (sizeof(Object) + page_size - 1) / page_size;
        size_t pool_size = page_size * protected_pages + capacity * sizeof(Object);

        begin = reinterpret_cast<uint8_t *>(
            mmap(nullptr, pool_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0));

        if (begin == MAP_FAILED) {
            throw std::bad_alloc();
        }

        mprotect(begin, page_size * protected_pages, PROT_NONE);

        end = begin + pool_size;
        first_free = reinterpret_cast<uint64_t>(end);

        slot = pool_registry.add(begin, end);
        if (slot == -1) {
            throw runtime_error("No free pool slot");
        }
    }

    Object *allocate(size_t n) {
        Object *begin = reinterpret_cast<Object *>(
            first_free.fetch_sub(sizeof(Object), memory_order_relaxed));
        return begin - 1;
    }

    void deallocate(Object *ptr, size_t n) {}

    ~LockFreePoolAllocator() {
        pool_registry.remove(slot);
        munmap(begin, (size_t)end - (size_t)begin);
    }
};

static void get_usage(struct rusage &usage) {
    if (getrusage(RUSAGE_SELF, &usage)) {
        perror("Cannot get usage");
        exit(EXIT_SUCCESS);
    }
}

struct Node {
    Node *next;
    unsigned node_id;
};

template <template <typename> typename Allocator = std::allocator>
static inline Node *create_list(unsigned n, Allocator<Node> &allocator) {
    Node *list = nullptr;
    for (unsigned i = 0; i < n; i++) {
        Node *node = allocator.allocate(1);
        *node = Node{list, i};
        list = node;
    }
    return list;
}

template <template <typename> typename Allocator = std::allocator>
static inline void delete_list(Node *list, Allocator<Node> &allocator) {
    while (list) {
        Node *node = list;
        list = list->next;
        allocator.deallocate(node, 1);
    }
}

constexpr int threads_num = 16;

template <template <typename> typename Allocator, typename... Args>
static inline void test(unsigned n, bool local, Args... args) {
    struct rusage start, finish;
    get_usage(start);

    if (local) {
        vector<thread> threads;
        for (int i = 0; i < threads_num; i++) {
            threads.emplace_back([n, args...] {
                Allocator<Node> allocator(args...);
                auto list = create_list(n, allocator);
                delete_list(list, allocator);
            });
        }
        for (int i = 0; i < threads_num; i++) {
            threads[i].join();
        }
    } else {
        Allocator<Node> allocator(args...);
        vector<thread> threads;
        for (int i = 0; i < threads_num; i++) {
            threads.emplace_back([n, &allocator] {
                auto list = create_list(n, allocator);
                delete_list(list, allocator);
            });
        }
        for (int i = 0; i < threads_num; i++) {
            threads[i].join();
        }
    }

    get_usage(finish);

    struct timeval diff;
    timersub(&finish.ru_utime, &start.ru_utime, &diff);
    uint64_t time_used = diff.tv_sec * 1000000 + diff.tv_usec;
    cout << "Time used:       " << time_used << " usec\n";

    uint64_t mem_used = (finish.ru_maxrss - start.ru_idrss) * 1024;
    cout << "Memory used:     " << mem_used << " bytes\n";

    auto mem_required = threads_num * n * sizeof(Node);
    cout << "Memory required: " << mem_required << " bytes \n";

    auto overhead = (mem_used - min(mem_required, mem_used)) * double(100) / mem_used;
    cout << "Overhead: " << std::fixed << std::setw(4) << std::setprecision(1)
         << overhead << "%\n";
}

constexpr int nodes_num = 10000000;

int main(const int argc, const char *argv[]) {
    struct sigaction action;
    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = alloc_handler;

    struct sigaction segv_action;
    sigaction(SIGSEGV, &action, &segv_action);
    segv_handler = segv_action.sa_sigaction;

    if (argc < 2) {
        std::cerr << "Expected use: ./non-blocking-pool <allocator>" << std::endl;
        std::cerr << "Available allocators:" << std::endl;
        std::cerr << "    std::allocator LockedPoolAllocator LockFreePoolAllocator PoolAllocator" << std::endl;
        exit(1);
    }

    if (string(argv[1]) == "std::allocator") {
        test<std::allocator>(nodes_num, false);
    } else if (string(argv[1]) == "LockedPoolAllocator") {
        test<LockedPoolAllocator>(nodes_num, false, threads_num * nodes_num);
    } else if (string(argv[1]) == "LockFreePoolAllocator") {
        test<LockFreePoolAllocator>(nodes_num, false, threads_num * nodes_num);
    } else if (string(argv[1]) == "PoolAllocator") {
        test<PoolAllocator>(nodes_num, true, nodes_num);
    } else {
        std::cerr << "Unknown allocator: " << argv[1] << std::endl;
        std::cerr << "Available allocators:" << std::endl;
        std::cerr << "    std::allocator LockedPoolAllocator LockFreePoolAllocator PoolAllocator" << std::endl;
        exit(1);
    }
    return EXIT_SUCCESS;
}
