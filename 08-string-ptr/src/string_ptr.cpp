#include "string_ptr.h"
#include <utility>

StringPtr::StringPtr() {
    cstr = &null_string;
}

StringPtr::StringPtr(const char *str) {
    cstr = new CString(str);
}

StringPtr::StringPtr(const StringPtr &other) {
    cstr = other.cstr;
    cstr->inc_ref_count();
}

StringPtr &StringPtr::operator=(const StringPtr &other) {
    if (this == &other) {
        return *this;
    }

    StringPtr sptr(other);
    std::swap(this->cstr, sptr.cstr);
    return *this;
}

StringPtr &StringPtr::operator=(StringPtr &&other) {
    if (this == &other) {
        return *this;
    }

    if (cstr->ref_count() == 0) {
        delete cstr;
    } else {
        cstr->dec_ref_count();
    }

    cstr = other.cstr;
    other.cstr = &null_string;

    return *this;
}

StringPtr &StringPtr::operator=(const char *str) {
    StringPtr sptr(str);
    std::swap(this->cstr, sptr.cstr);
    return *this;
}

StringPtr::StringPtr(StringPtr &&other) {
    this->cstr = other.cstr;
    other.cstr = &null_string;
}

StringPtr::~StringPtr() {
    if (cstr->ref_count() == 0) {
        delete cstr;
    } else {
        cstr->dec_ref_count();
    }
}

const char *StringPtr::str() {
    return cstr->c_str();
}

size_t StringPtr::ref_count() {
    return 1 + cstr->ref_count();
}
