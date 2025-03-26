#pragma once

#include <cstddef>
#include <cstdint>

#define DEBUG_MODE 1

struct CString {
private:
    uintptr_t ptr;

public:
    CString(const char *str);
    CString(const CString &other) = delete;
    CString(CString &&other) = delete;
    ~CString();

    const char *c_str();
    size_t ref_count();
    void inc_ref_count();
    void dec_ref_count();

    static constexpr size_t REF_COUNTER_LENGTH = 1;
    static constexpr size_t ALIGNMENT = (1 << REF_COUNTER_LENGTH);
    static constexpr size_t REF_COUNTER_MASK = (1 << REF_COUNTER_LENGTH) - 1;
    static constexpr size_t REF_COUNT_SATURATION = (1 << REF_COUNTER_LENGTH) - 1;
};

extern CString null_string;
