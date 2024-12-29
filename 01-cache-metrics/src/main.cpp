#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

static constexpr uint32_t PAGE_SIZE = 4 * 1024;

constexpr size_t ARRAY_SIZE = (1 << 20);
alignas(PAGE_SIZE) ptrdiff_t jump[ARRAY_SIZE];

#define REPEAT_4(op) op op op op
#define REPEAT_16(op) REPEAT_4(REPEAT_4(op))
#define REPEAT_256(op) REPEAT_16(REPEAT_16(op))
#define REPEAT_1024(op) REPEAT_4(REPEAT_256(op))

volatile ptrdiff_t *save;

#define STEP cursor = *reinterpret_cast<ptrdiff_t **>(cursor);

double lap() {
    ptrdiff_t *cursor = jump;
    REPEAT_256(STEP)
    auto start_time = std::chrono::steady_clock::now();
    REPEAT_1024(STEP)
    auto end_time = std::chrono::steady_clock::now();

    save = cursor;

    return (double)(end_time - start_time).count();
}

double run_with_stride(ptrdiff_t stride, ptrdiff_t spots) {
    ptrdiff_t size = std::min((ptrdiff_t)ARRAY_SIZE, stride * spots);
    for (ptrdiff_t *i = jump; i < jump + size; i++) {
        if (i - stride >= jump)
            *i = (ptrdiff_t)(i - stride);
        else
            *i = (ptrdiff_t)(i + size - stride);
    }
    return lap();
}

constexpr size_t MAX_SPOTS = 20;
constexpr size_t MAX_STRIDE_I = 20;

template <typename T>
using table_t = std::array<std::array<T, MAX_STRIDE_I>, MAX_SPOTS>;

table_t<bool> run() {
    table_t<double> table;

    for (int spots = 1; spots < MAX_SPOTS; spots++) {
        table[spots].fill(0);
        for (int stride_i = 0; stride_i < MAX_STRIDE_I; stride_i++) {
            ptrdiff_t stride = 1 << stride_i;
            if (spots * stride > ARRAY_SIZE)
                continue;
            table[spots][stride_i] = run_with_stride(stride, spots);
        }
    }

    constexpr double threshold = 2.0;

    table_t<bool> or_table;
    for (int spots = 2; spots < MAX_SPOTS; spots++) {
        for (int stride_i = 1; stride_i < MAX_STRIDE_I; stride_i++) {
            bool step1 = (table[spots][stride_i] > threshold * table[spots][stride_i - 1]);
            bool step2 = (table[spots][stride_i] > threshold * table[spots - 1][stride_i]);
            or_table[spots][stride_i] = (step1 || step2);
        }
    }

    return or_table;
}

double run_with_two_strides(ptrdiff_t small_stride, ptrdiff_t big_stride, ptrdiff_t spots) {
    ptrdiff_t *cursor = jump;
    ptrdiff_t size = std::min((ptrdiff_t)ARRAY_SIZE, big_stride * spots);
    for (ptrdiff_t *i = jump; i < jump + size; i += big_stride) {
        if (i + big_stride < jump + size)
            *(i + small_stride) = (ptrdiff_t)(i + small_stride + big_stride - small_stride);
        else
            *(i + small_stride) = (ptrdiff_t)(i + small_stride + big_stride - small_stride - size);
        *i = (ptrdiff_t)(i + small_stride);
    }
    return lap();
}

int main() {
    table_t<int> stat;
    for (int r = 0; r < MAX_SPOTS; r++) {
        for (int c = 0; c < MAX_STRIDE_I; c++) {
            stat[r][c] = 0;
        }
    }

    const int runs = 100;

    for (int i = 0; i < runs; i++) {
        auto cur = run();
        for (int r = 2; r < MAX_SPOTS; r++) {
            for (int c = 1; c < MAX_STRIDE_I; c++) {
                stat[r][c] += cur[r][c];
            }
        }
    }

    struct Candidate {
        int r, c;
        size_t assoc, cap;
    };

    std::vector<Candidate> candidates;

    const double threshold = 0.8;
    size_t capacity = std::numeric_limits<size_t>::max();
    size_t associativity = std::numeric_limits<size_t>::max();

    std::cout << "#####  ";
    for (int c = 1; c < MAX_STRIDE_I; c++) {
        std::cout << std::setw(5) << c;
    }
    std::cout << std::endl;

    for (int r = 2; r < MAX_SPOTS; r++) {
        std::cout << std::setw(5) << r << ": ";
        for (int c = 1; c < MAX_STRIDE_I; c++) {
            std::cout << std::setw(5) << stat[r][c];
            if (stat[r][c] > runs * threshold) {
                // std::cout << "candidate " << r << " " << c << " ";
                size_t candidate_associativity = r - 1;
                size_t candidate_capacity = (1 << c) * candidate_associativity * sizeof(ptrdiff_t);
                candidates.push_back(Candidate{r, c, candidate_associativity, candidate_capacity});
                // std::cout << candidate_capacity << " " << candidate_associativity << std::endl;
                if (std::pair{candidate_capacity, candidate_associativity} < std::pair{capacity, associativity}) {
                    capacity = candidate_capacity;
                    associativity = candidate_associativity;
                }
            }
        }
        std::cout << std::endl;
    }

    std::cout << "Candidates: ";
    for (auto &c : candidates) {
        std::cout << " r=" << c.r << " c=" << c.c << " assoc=" << c.assoc << " cap=" << c.cap << std::endl;
    }

    std::vector<int> cache_line_stat(8, 0);
    const double time_threshold = 1.2;
    ptrdiff_t big_stride = capacity / associativity / sizeof(ptrdiff_t);
    for (int i = 0; i < runs; i++) {
        std::vector<double> time(8, 0);
        for (int small_stride_i = 0; (1 << small_stride_i) <= 256 / sizeof(ptrdiff_t); small_stride_i++) {
            time[small_stride_i] = run_with_two_strides(1 << small_stride_i, big_stride, associativity + 1);
        }
        for (int i = 1; i < 8; i++) {
            cache_line_stat[i] += (time[i] > time_threshold * time[i - 1]);
        }
    }

    int cache_line_size_i = max_element(cache_line_stat.begin(), cache_line_stat.end()) - cache_line_stat.begin();
    int cache_line_size = (1 << cache_line_size_i) * sizeof(ptrdiff_t);

    std::cout << "     Capacity: " << capacity << std::endl;
    std::cout << "Associativity: " << associativity << std::endl;
    std::cout << "   Block size: " << cache_line_size << std::endl;
}
