#include "runtime_externs.h"
#include "../runtime/runtime_common.h"
#include "stack.h"

inline void *Barray(int n) {
    data *r = (data *)alloc_array(n);

    for (int i = n - 1; i >= 0; i--) {
        ((int *)r->contents)[i] = vstack_pop();
    }

    return r->contents;
}

inline void *BSexp(int n, int tag) {
    int fields_cnt = n;
    data *r = (data *)alloc_sexp(fields_cnt);
    ((sexp *)r)->tag = 0;

    for (int i = n; i > 0; i--) {
        ((int *)r->contents)[i] = vstack_pop();
    }

    ((sexp *)r)->tag = tag;

    return (int *)r->contents;
}

inline void *Bclosure(int n, void *entry) {
    data *r = (data *)alloc_closure(n + 1);
    ((void **)r->contents)[0] = entry;

    for (int i = n; i >= 1; --i) {
        ((int *)r->contents)[i] = vstack_pop();
    }

    return r->contents;
}
