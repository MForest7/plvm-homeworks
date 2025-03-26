#include "string_ptr.h"
#include <gtest/gtest.h>
#include <utility>

TEST(CStringTest, AllocDealloc) {
    CString cstr("Hello");

    EXPECT_STREQ("Hello", cstr.c_str());
}

TEST(CStringTest, EmptyString) {
    CString cstr("");

    EXPECT_STREQ("", cstr.c_str());
}

TEST(CStringTest, RefCount) {
    CString cstr("Hello");

    EXPECT_EQ(0, cstr.ref_count());

    cstr.inc_ref_count();
    EXPECT_EQ(1, cstr.ref_count());

    cstr.inc_ref_count();
    EXPECT_EQ(1, cstr.ref_count());

    cstr.dec_ref_count();
    EXPECT_EQ(1, cstr.ref_count());
}

TEST(StringPtrTest, CreateNull) {
    StringPtr s1;

    EXPECT_EQ(nullptr, s1.str());
    EXPECT_EQ(2, s1.ref_count());
}

TEST(StringPtrTest, CreateFromString) {
    StringPtr s1("");
    StringPtr s2("hello");

    EXPECT_STREQ("", s1.str());
    EXPECT_EQ(1, s1.ref_count());
    EXPECT_STREQ("hello", s2.str());
    EXPECT_EQ(1, s2.ref_count());
}

TEST(StringPtrTest, CreateFromStringPtr) {
    StringPtr s1("abacaba");
    StringPtr s2(s1);

    EXPECT_STREQ(s1.str(), s2.str());
    EXPECT_EQ(2, s1.ref_count());
    EXPECT_EQ(2, s2.ref_count());
}

TEST(StringPtrTest, MoveFromStringPtr) {
    StringPtr s1("abacaba");
    StringPtr s2(std::move(s1));

    EXPECT_STREQ("abacaba", s2.str());
    EXPECT_STREQ(nullptr, s1.str());

    EXPECT_EQ(1, s2.ref_count());
    EXPECT_EQ(2, s1.ref_count());
}

TEST(StringPtrTest, AssignFromString) {
    StringPtr s1;
    s1 = "ababb";

    EXPECT_STREQ("ababb", s1.str());
    EXPECT_EQ(1, s1.ref_count());
}

TEST(StringPtrTest, AssignFromStringPtr) {
    StringPtr s1;
    {
        StringPtr s2 = "abacaba";
        s1 = s2;
    }
    EXPECT_STREQ("abacaba", s1.str());
    EXPECT_EQ(2, s1.ref_count());
}

TEST(StringPtrTest, AssignFromSelf) {
    StringPtr s1 = "abacaba";
    s1 = s1;

    EXPECT_STREQ("abacaba", s1.str());
    EXPECT_EQ(1, s1.ref_count());
}

TEST(StringPtrTest, MoveFromSelf) {
    StringPtr s1 = "abacaba";
    s1 = std::move(s1);

    EXPECT_STREQ("abacaba", s1.str());
    EXPECT_EQ(1, s1.ref_count());
}

TEST(StringPtrTest, AssignMoveFromStringPtr) {
    StringPtr s1;
    {
        StringPtr s2 = "abacaba";
        s1 = std::move(s2);
        EXPECT_EQ(nullptr, s2.str());
        EXPECT_EQ(2, s2.ref_count());
    }
    EXPECT_STREQ("abacaba", s1.str());
    EXPECT_EQ(1, s1.ref_count());
}

TEST(StringPtrTest, BubbleSort) {
    constexpr size_t n = 4;
    StringPtr a[] = {"abacaba", "", "hello", "a"};
    const char *expected[] = {"", "a", "abacaba", "hello"};

    for (size_t i = 0; i < n; i++) {
        for (size_t j = 1; j < n; j++) {
            if (strcmp(a[j - 1].str(), a[j].str()) > 0) {
                std::swap(a[j - 1], a[j]);
            }
        }
    }

    for (size_t i = 0; i < n; i++) {
        EXPECT_EQ(1, a[i].ref_count());
        EXPECT_STREQ(expected[i], a[i].str());
    }
}
