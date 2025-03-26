#pragma once

#include "cstring.h"

struct StringPtr {
    StringPtr();
    StringPtr(const char *str);
    StringPtr(const StringPtr &other);
    StringPtr &operator=(const StringPtr &other);
    StringPtr &operator=(StringPtr &&other);
    StringPtr &operator=(const char *str);
    StringPtr(StringPtr &&other);
    ~StringPtr();

    const char *str();

    size_t ref_count();

private:
    CString *cstr;
};
