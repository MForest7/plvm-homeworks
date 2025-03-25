#include <atomic>
#include <iomanip>
#include <iostream>
#include <new>
#include <queue>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>

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
    size_t register_index;
};

static constexpr size_t POOL_LIMIT = 100;
MemPool *known_pools[POOL_LIMIT];
size_t first_free_pool_slot;
queue<size_t> free_pool_slots;

enum PoolRegisterStatus {
    IDLE,
    UPDATING,
    BAD_ALLOC
};

atomic_size_t pool_registry_status;
atomic_flag bad_alloc_happened;

static inline void pool_registry_spinlock(PoolRegisterStatus lock_status) {
    size_t expected;
    do {
        expected = IDLE;
    } while (pool_registry_status.compare_exchange_strong(expected, lock_status));
}

static inline void pool_registry_unlock() {
    pool_registry_status.store(IDLE);
}

static inline void pool_registry_add(MemPool *pool) {
    pool_registry_spinlock(UPDATING);

    pool->register_index = first_free_pool_slot;
    if (!free_pool_slots.empty()) {
        pool->register_index = free_pool_slots.front();
        free_pool_slots.pop();
    } else if (first_free_pool_slot == POOL_LIMIT) {
        throw runtime_error("No free slots in pool registry");
    } else {
        first_free_pool_slot++;
    }

    known_pools[pool->register_index] = pool;

    pool_registry_unlock();
}

static inline void pool_registry_remove(MemPool *pool) {
    pool_registry_spinlock(UPDATING);

    free_pool_slots.push(pool->register_index);
    known_pools[pool->register_index] = nullptr;

    pool_registry_unlock();
}

void segv_handler(int signo, siginfo_t *info, void *extra) {
    bad_alloc_happened.test_and_set();
    pool_registry_spinlock(BAD_ALLOC);

    auto failed_ptr = info->si_addr;
    for (size_t i = 0; i < POOL_LIMIT; i++) {
        const MemPool &entry = *known_pools[i];
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
    MemPool pool;
    void *first_free;

public:
    PoolAllocator(size_t capacity) {
        pool.pool = this;
        pool.kind = SIMPLE;

        size_t page_size = sysconf(_SC_PAGE_SIZE);
        size_t protected_pages = (sizeof(Object) + page_size - 1) / page_size;
        size_t pool_size = page_size * protected_pages + capacity * sizeof(Object);

        pool.begin = mmap(nullptr, pool_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);

        if (pool.begin == MAP_FAILED) {
            throw std::bad_alloc();
        }

        mprotect(pool.begin, page_size * protected_pages, PROT_NONE);

        pool.end = pool.begin + pool_size;
        first_free = pool.end;

        pool_registry_add(&pool);
    }

    Object *allocate(size_t n) {
        return reinterpret_cast<Object *>(first_free -= sizeof(Object));
    }

    void deallocate(Object *ptr, size_t n) {}

    ~PoolAllocator() {
        pool_registry_remove(&pool);
        munmap(pool.begin, (size_t)pool.end - (size_t)pool.begin);
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

template <template <typename> typename Allocator>
static inline void test(unsigned n, Allocator<Node> *allocator, bool auto_delete) {
    struct rusage start, finish;
    get_usage(start);

    auto list = create_list(n, allocator);
    if (!auto_delete) {
        delete_list(list, allocator);
    }

    delete allocator;

    get_usage(finish);

    struct timeval diff;
    timersub(&finish.ru_utime, &start.ru_utime, &diff);
    uint64_t time_used = diff.tv_sec * 1000000 + diff.tv_usec;
    cout << "Time used:       " << time_used << " usec\n";

    uint64_t mem_used = (finish.ru_maxrss - start.ru_maxrss) * 1024;
    cout << "Memory used:     " << mem_used << " bytes\n";

    auto mem_required = n * sizeof(Node);
    cout << "Memory required: " << mem_required << " bytes \n";

    auto overhead = (mem_used - mem_required) * double(100) / mem_used;
    cout << "Overhead: " << std::fixed << std::setw(4) << std::setprecision(1)
         << overhead << "%\n";
}

int main(const int argc, const char *argv[]) {
    struct sigaction action;
    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = segv_handler;
    sigaction(SIGSEGV, &action, NULL);

    cout << "With std::allocator:" << endl;
    test(10000000, new std::allocator<Node>{}, /* auto_delete */ false);

    cout << "With PoolAllocator:" << endl;
    test(10000000, new PoolAllocator<Node>(10000000), /* auto_delete */ true);
    return EXIT_SUCCESS;
}
