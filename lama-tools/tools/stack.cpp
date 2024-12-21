#include "stack.h"
#include "../runtime/runtime_common.h"
#include "error.h"
#include "opcode.h"

#define VSTACK_SIZE 65536
#define CSTACK_SIZE 65536

extern "C" size_t *__gc_stack_top;
extern "C" size_t *__gc_stack_bottom;

inline size_t vstack_load(size_t *ptr) {
    ASSERT(ptr < __gc_stack_bottom, "Virtual stack underflow");
    ASSERT(ptr > __gc_stack_top, "Location is out of stack");
    return *ptr;
}

inline void vstack_init() {
    __gc_stack_bottom = __vstack + VSTACK_SIZE;
    __gc_stack_top = __gc_stack_bottom - 1;
}

inline void vstack_alloc_globals(size_t count) {
    ASSERT(VSTACK_SIZE >= count, 1, "Cannot allocate memory for globals");
    __gc_stack_top -= count;
    __vstack_globals_count = count;
}

inline void vstack_push(size_t value) {
    ASSERT(__gc_stack_top > __vstack, 1, "Virtual stack overflow");
    *(__gc_stack_top--) = value;
}

inline size_t vstack_pop() {
    ASSERT(__gc_stack_top + 1 != __gc_stack_bottom, 1, "Virtual stack underflow");
    return *(++__gc_stack_top);
}

inline size_t vstack_top() {
    ASSERT(__gc_stack_top + 1 < __gc_stack_bottom, 1, "Virtual stack underflow");
    return *(__gc_stack_top + 1);
}

inline size_t vstack_kth_from_end(size_t index) {
    ASSERT(__gc_stack_top + 1 + index < __gc_stack_bottom, 1, "Virtual stack underflow");
    return *(__gc_stack_top + 1 + index);
}

inline size_t vstack_size() {
    return __gc_stack_bottom - __gc_stack_top - 1;
}

void dump_stack() {
    fprintf(stderr, "vstack: ");
    for (size_t *ptr = __gc_stack_bottom - 1; ptr > __gc_stack_top; ptr--) {
        if (UNBOXED(*ptr))
            fprintf(stderr, "%d ", UNBOX(*ptr));
        else
            fprintf(stderr, "0x%x ", *ptr);
    }
    fprintf(stderr, "\n");
}

inline void cstack_init() {
    __cstack_top = __cstack + CSTACK_SIZE - 1;
    __cstack_bottom = __cstack + CSTACK_SIZE;

    // init main frame
    __cstack_top->base = __gc_stack_top;
    __cstack_top->return_ip = NULL;
}

inline frame *cstack_call(char *return_ip, size_t args_count, bool is_closure) {
    ASSERT(__cstack_top > __cstack, 1, "Call stack overflow");
    ASSERT(__gc_stack_top + args_count < __gc_stack_bottom, 1, "Virtual stack underflow");

    frame *top_frame = --__cstack_top;
    top_frame->return_ip = return_ip;
    top_frame->base = __gc_stack_top;
    top_frame->is_closure = is_closure;
    top_frame->args_count = args_count;
    return top_frame;
}

inline void cstack_alloc(size_t locals_count) {
    ASSERT(__gc_stack_top - locals_count > __vstack, 1, "Virtual stack overflow");
    __gc_stack_top -= locals_count;
    __cstack_top->locals_count = locals_count;
}

/**
 * returns: return ip
 */
inline char *cstack_end() {
    size_t return_value = vstack_pop();
    ASSERT(__cstack_top != __cstack_bottom, 1, "Call stack underflow");
    ASSERT(__cstack_top->base + __cstack_top->args_count <= __gc_stack_bottom, 1, "Virtual stack underflow");
    __gc_stack_top = __cstack_top->base + __cstack_top->args_count + __cstack_top->is_closure;
    vstack_push(return_value);
    return (__cstack_top++)->return_ip;
}

inline size_t *loc(size_t location, int index) {
    size_t *ptr = NULL;
    size_t *closure_content;
    size_t closure_size;
    switch (location) {
    case Location_Global:
        ASSERT(index < __vstack_globals_count, 1,
               "Memory access failed: G(%d) is out of section (%d globals)",
               index, __vstack_globals_count);
        ptr = __gc_stack_bottom - 1 - index;
        break;
    case Location_Local:
        ASSERT(index < __cstack_top->locals_count, 1,
               "Memory access failed: L(%d) is out of frame (%d locals)",
               index, __cstack_top->locals_count);
        ptr = __cstack_top->base - index;
        break;
    case Location_Arg:
        ASSERT(index < __cstack_top->args_count, 1,
               "Memory access failed: A(%d) is out of frame (%d args)",
               index, __cstack_top->args_count);
        ptr = __cstack_top->base + (__cstack_top->args_count - index);
        break;
    case Location_Captured:
        ASSERT(__cstack_top->is_closure, 1,
               "Memory access failed: C(%d) is invalid out of closure",
               index);
        closure_content = *(size_t **)(__cstack_top->base + __cstack_top->args_count + 1);
        ASSERT(index < closure_size, 1,
               "Memory access failed: C(%d) is out of captured (%d captured)",
               closure_size);
        ptr = closure_content + 1 + index;
        break;
    default:
        FAIL(1, "Unexpected location type %d\n", location);
    }
    return ptr;
}
