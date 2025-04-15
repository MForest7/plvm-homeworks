#include "safe_read.h"
#include <gtest/gtest.h>
#include <numeric>
#include <random>
#include <sys/mman.h>

TEST(SafeReadByteTest, ReadValidByte) {
    uint8_t byte = 42;

    auto value = safe_read_byte(&byte);
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(42, value.value());
}

TEST(SafeReadByteTest, ReadNull) {
    auto value = safe_read_byte(nullptr);
    EXPECT_FALSE(value.has_value());
}

TEST(SafeReadByteTest, MultipleCorrectRead) {
    constexpr size_t N = 100;

    std::mt19937 mte;
    std::uniform_int_distribution<size_t> rnd(0, N - 1);

    std::vector<uint8_t> v(N);
    std::iota(v.begin(), v.end(), 0);

    constexpr size_t T = 100'000;
    for (size_t i = 0; i < T; i++) {
        size_t j = rnd(mte);
        auto value = safe_read_byte(v.data() + j);
        EXPECT_TRUE(value.has_value());
        EXPECT_EQ(j, value.value());
    }
}

TEST(SafeReadByteTest, MultipleIncorrectRead) {
    constexpr size_t N = 100;

    std::mt19937 mte;
    std::uniform_int_distribution<size_t> rnd(0, N - 1);

    auto ptr = reinterpret_cast<uint8_t *>(
        mmap(nullptr, N, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0));

    constexpr size_t T = 100'000;
    for (size_t i = 0; i < T; i++) {
        size_t j = rnd(mte);
        auto value = safe_read_byte(ptr + j);
        EXPECT_FALSE(value.has_value());
    }
}

TEST(SafeReadByteTest, MultipleNullRead) {
    constexpr size_t T = 100'000;
    for (size_t i = 0; i < T; i++) {
        auto value = safe_read_byte(nullptr);
        EXPECT_FALSE(value.has_value());
    }
}

TEST(SafeReadByteTest, MultipleMixedRead) {
    constexpr size_t N = 100;

    std::mt19937 mte;
    std::uniform_int_distribution<size_t> rnd(0, 2 * N - 1);

    auto ptr1 = reinterpret_cast<uint8_t *>(
        mmap(nullptr, N, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0));
    auto ptr2 = new uint8_t[N];

    constexpr size_t T = 100'000;
    for (size_t i = 0; i < T; i++) {
        size_t j = rnd(mte);
        if (j < N) {
            auto value = safe_read_byte(ptr1 + j);
            EXPECT_FALSE(value.has_value());
        } else {
            auto value = safe_read_byte(ptr2 + j);
            EXPECT_TRUE(value.has_value());
        }
    }
}
