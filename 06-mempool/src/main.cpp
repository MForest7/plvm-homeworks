#include <iomanip>
#include <iostream>
#include <new>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>

using namespace std;

void segv_handler(int signo, siginfo_t *info, void *extra) {
    throw std::bad_alloc();
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

        first_free = allocate_begin = allocate_end + pool_size;
    }

    Object *allocate(size_t n) {
        return reinterpret_cast<Object *>(first_free -= sizeof(Object));
    }

    void deallocate(Object *ptr, size_t n) {}

    ~PoolAllocator() {
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

template <template <typename> typename Allocator>
static inline void test(unsigned n, Allocator<Node> *allocator) {
    struct rusage start, finish;
    get_usage(start);

    auto list = create_list(n, allocator);
    delete_list(list, allocator);

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
    test(10000000, new std::allocator<Node>{});

    cout << "With PoolAllocator:" << endl;
    test(10000000, new PoolAllocator<Node>(10000000));
    return EXIT_SUCCESS;
}
