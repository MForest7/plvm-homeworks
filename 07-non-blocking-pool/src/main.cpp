#include <algorithm>
#include <atomic>
#include <iomanip>
#include <iostream>
#include <list>
#include <mutex>
#include <new>
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

enum PoolKind {
    SIMPLE,
    LOCKED,
    LOCK_FREE
};

struct MemPool {
    void *begin;
    void *end;
    PoolKind kind;
    void *pool;
};

list<MemPool> known_pools;

void segv_handler(int signo, siginfo_t *info, void *extra) {
    auto failed_ptr = info->si_addr;
    for (const MemPool &entry : known_pools) {
        if (entry.begin <= failed_ptr && failed_ptr < entry.end) {
            switch (entry.kind) {
            case SIMPLE:
                write(2, "Bad alloc in PoolAllocator\n", 28);
                break;
            case LOCKED:
                write(2, "Bad alloc in LockedPoolAllocator\n", 34);
                break;
            case LOCK_FREE:
                write(2, "Bad alloc in LockFreePoolAllocator\n", 36);
                break;
            }
            exit(3);
        }
    }
    exit(3);
}

template <typename Object>
class PoolAllocator {
    void *allocate_begin;
    void *allocate_end;
    void *first_free;

public:
    PoolAllocator(size_t capacity) {
        size_t page_size = sysconf(_SC_PAGE_SIZE);
        size_t protected_pages = (sizeof(Object) + page_size - 1) / page_size;
        size_t pool_size = page_size * protected_pages + capacity * sizeof(Object);

        allocate_end = mmap(nullptr, pool_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);

        if (allocate_end == MAP_FAILED) {
            throw std::bad_alloc();
        }

        mprotect(allocate_end, page_size * protected_pages, PROT_NONE);

        allocate_begin = allocate_end + pool_size;
        known_pools.emplace_back(allocate_end, allocate_begin, SIMPLE, this);
        first_free = allocate_begin;
    }

    Object *allocate(size_t n) {
        return reinterpret_cast<Object *>(first_free -= sizeof(Object));
    }

    void deallocate(Object *ptr, size_t n) {}

    ~PoolAllocator() {
        auto register_entry = find_if(known_pools.begin(), known_pools.end(), [this](const MemPool &entry) { return entry.pool == this; });
        known_pools.erase(register_entry);
        munmap(allocate_end, (size_t)allocate_begin - (size_t)allocate_end);
    }
};

template <typename Object>
class LockedPoolAllocator {
    void *allocate_begin;
    void *allocate_end;
    void *first_free;
    mutex lock;

public:
    LockedPoolAllocator(size_t capacity) {
        size_t page_size = sysconf(_SC_PAGE_SIZE);
        size_t protected_pages = (sizeof(Object) + page_size - 1) / page_size;
        size_t pool_size = page_size * protected_pages + capacity * sizeof(Object);

        allocate_end = mmap(nullptr, pool_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);

        if (allocate_end == MAP_FAILED) {
            throw std::bad_alloc();
        }

        mprotect(allocate_end, page_size * protected_pages, PROT_NONE);

        allocate_begin = allocate_end + pool_size;
        known_pools.emplace_back(allocate_end, allocate_begin, LOCKED, this);
        first_free = allocate_begin;
    }

    Object *allocate(size_t n) {
        lock.lock();
        Object *ptr = reinterpret_cast<Object *>(first_free -= sizeof(Object));
        lock.unlock();
        return ptr;
    }

    void deallocate(Object *ptr, size_t n) {}

    ~LockedPoolAllocator() {
        auto register_entry = find_if(known_pools.begin(), known_pools.end(), [this](const MemPool &entry) { return entry.pool == this; });
        known_pools.erase(register_entry);
        munmap(allocate_end, (size_t)allocate_begin - (size_t)allocate_end);
    }
};

template <typename Object>
class LockFreePoolAllocator {
    void *allocate_begin;
    void *allocate_end;
    atomic_uint64_t first_free;

public:
    LockFreePoolAllocator(size_t capacity) {
        size_t page_size = sysconf(_SC_PAGE_SIZE);
        size_t protected_pages = (sizeof(Object) + page_size - 1) / page_size;
        size_t pool_size = page_size * protected_pages + capacity * sizeof(Object);

        allocate_end = mmap(nullptr, pool_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);

        if (allocate_end == MAP_FAILED) {
            throw std::bad_alloc();
        }

        mprotect(allocate_end, page_size * protected_pages, PROT_NONE);

        allocate_begin = allocate_end + pool_size;
        known_pools.emplace_back(allocate_end, allocate_begin, LOCK_FREE, this);
        first_free = reinterpret_cast<uint64_t>(allocate_begin);
    }

    Object *allocate(size_t n) {
        Object *begin = reinterpret_cast<Object *>(first_free.fetch_sub(sizeof(Object)));
        return begin - 1;
    }

    void deallocate(Object *ptr, size_t n) {}

    ~LockFreePoolAllocator() {
        auto register_entry = find_if(known_pools.begin(), known_pools.end(), [this](const MemPool &entry) { return entry.pool == this; });
        known_pools.erase(register_entry);
        munmap(allocate_end, (size_t)allocate_begin - (size_t)allocate_end);
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
static inline Node *create_list(unsigned n, Allocator<Node> *allocator) {
    Node *list = nullptr;
    for (unsigned i = 0; i < n; i++) {
        Node *node = allocator->allocate(1);
        *node = Node{list, i};
        list = node;
    }
    return list;
}

template <template <typename> typename Allocator = std::allocator>
static inline void delete_list(Node *list, Allocator<Node> *allocator) {
    while (list) {
        Node *node = list;
        list = list->next;
        allocator->deallocate(node, 1);
    }
}

constexpr int threads_num = 16;

template <template <typename> typename Allocator>
static inline void run_with_global_allocator(unsigned n, Allocator<Node> *allocator) {
    vector<jthread> threads;
    for (int i = 0; i < threads_num; i++) {
        threads.emplace_back([n, allocator] {
            auto list = create_list(n, allocator);
            delete_list(list, allocator);
            return;
        });
    }
    threads.clear();
}

static inline void run_with_local_allocators(unsigned n) {
    vector<jthread> threads;
    for (int i = 0; i < threads_num; i++) {
        threads.emplace_back([n] {
            PoolAllocator<Node> allocator(n);
            auto list = create_list(n, &allocator);
            delete_list(list, &allocator);
            return;
        });
    }
    threads.clear();
}

template <template <typename> typename Allocator>
static inline void test(unsigned n, Allocator<Node> *allocator) {
    struct rusage start, finish;
    get_usage(start);

    if (allocator != nullptr) {
        run_with_global_allocator<Allocator>(n, allocator);
        delete allocator;
    } else {
        run_with_local_allocators(n);
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
    action.sa_sigaction = segv_handler;
    sigaction(SIGSEGV, &action, NULL);

    if (argc < 2) {
        std::cerr << "Expected use: ./non-blocking-pool <allocator>" << std::endl;
        std::cerr << "Available allocators:" << std::endl;
        std::cerr << "    std::allocator LockedPoolAllocator LockFreePoolAllocator PoolAllocator" << std::endl;
        exit(1);
    }

    if (string(argv[1]) == "std::allocator") {
        test(nodes_num, new std::allocator<Node>{});
    } else if (string(argv[1]) == "LockedPoolAllocator") {
        test(nodes_num, new LockedPoolAllocator<Node>(threads_num * nodes_num));
    } else if (string(argv[1]) == "LockFreePoolAllocator") {
        test(nodes_num, new LockFreePoolAllocator<Node>(threads_num * nodes_num));
    } else if (string(argv[1]) == "PoolAllocator") {
        test<PoolAllocator>(nodes_num, nullptr);
    } else {
        std::cerr << "Unknown allocator: " << argv[1] << std::endl;
        std::cerr << "Available allocators:" << std::endl;
        std::cerr << "    std::allocator LockedPoolAllocator LockFreePoolAllocator PoolAllocator" << std::endl;
        exit(1);
    }
    return EXIT_SUCCESS;
}
