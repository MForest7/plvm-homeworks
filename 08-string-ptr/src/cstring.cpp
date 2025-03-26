#include "cstring.h"
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>

CString null_string{nullptr};

CString::CString(const char *str) {
    if (str == nullptr) {
        ptr = reinterpret_cast<uintptr_t>(REF_COUNT_SATURATION);
        return;
    }

    size_t length_aligned = strlen(str);
    length_aligned += ((length_aligned + ALIGNMENT - 1) & REF_COUNTER_MASK);
    ptr = reinterpret_cast<uintptr_t>(aligned_alloc(ALIGNMENT, length_aligned));
    strcpy(reinterpret_cast<char *>(ptr), str);
}

CString::~CString() {
    if constexpr (DEBUG_MODE) {
        std::cerr << "free "
                  << static_cast<uintptr_t>(ptr & (~REF_COUNTER_MASK))
                  << " -> \"" << c_str() << "\"" << std::endl;
    }

    free(reinterpret_cast<void *>(ptr & (~REF_COUNTER_MASK)));
}

const char *CString::c_str() {
    return reinterpret_cast<const char *>(ptr & (~REF_COUNTER_MASK));
}

size_t CString::ref_count() {
    return static_cast<size_t>(ptr & REF_COUNTER_MASK);
}

void CString::inc_ref_count() {
    size_t ref_count = static_cast<size_t>(ptr & REF_COUNTER_MASK);
    if (ref_count == REF_COUNT_SATURATION) {
        return;
    }
    ++ref_count;
    ptr = (ptr & ~REF_COUNTER_MASK) | ref_count;
}

void CString::dec_ref_count() {
    size_t ref_count = static_cast<size_t>(ptr & REF_COUNTER_MASK);
    if (ref_count == 0) {
        throw std::logic_error("Cannot decrement if ref_count_is_zero");
    }
    if (ref_count == REF_COUNT_SATURATION) {
        return;
    }
    --ref_count;
    ptr = (ptr & ~REF_COUNTER_MASK) | ref_count;
}
