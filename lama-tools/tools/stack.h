#ifndef STACK_H
#define STACK_H

#include <cstddef>

#define VSTACK_SIZE 65536
#define CSTACK_SIZE 65536

extern "C" size_t *__gc_stack_top;
extern "C" size_t *__gc_stack_bottom;

size_t __vstack_globals_count;
size_t __vstack[VSTACK_SIZE];

inline size_t vstack_load(size_t *ptr);

inline void vstack_init();

inline void vstack_alloc_globals(size_t count);

inline void vstack_push(size_t value);

inline size_t vstack_pop();

inline size_t vstack_top();

inline size_t vstack_kth_from_end(size_t index);

inline size_t vstack_size();

void dump_stack();

typedef struct frame {
    char *return_ip;
    bool is_closure;
    size_t *base;
    size_t args_count;
    size_t locals_count;
} frame;

frame __cstack[CSTACK_SIZE];
frame *__cstack_top;
frame *__cstack_bottom;

inline void cstack_init();

inline frame *cstack_call(char *return_ip, size_t args_count, bool is_closure);

inline void cstack_alloc(size_t locals_count);
/**
 * returns: return ip
 */
inline char *cstack_end();

inline size_t *loc(size_t location, int index);

#endif // STACK_H
