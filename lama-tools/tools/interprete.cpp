#include "interprete.h"
#include "../runtime/runtime_common.h"
#include "bytefile.h"
#include "error.h"
#include "inst_reader.h"
#include "opcode.h"

#include <iostream>
#include <vector>

#define VSTACK_SIZE 65536
#define CSTACK_SIZE 65536

namespace {

extern "C" size_t *__gc_stack_top;
extern "C" size_t *__gc_stack_bottom;

static size_t __vstack_globals_count;
static size_t __vstack[VSTACK_SIZE];

static inline size_t vstack_load(size_t *ptr) {
    ASSERT(ptr < __gc_stack_bottom, 1, "Virtual stack underflow");
    ASSERT(ptr > __gc_stack_top, 1, "Location is out of stack");
    return *ptr;
}

static inline void vstack_init() {
    __gc_stack_bottom = __vstack + VSTACK_SIZE;
    __gc_stack_top = __gc_stack_bottom - 1;
}

static inline void vstack_alloc_globals(size_t count) {
    ASSERT(VSTACK_SIZE >= count, 1, "Cannot allocate memory for globals");
    __gc_stack_top -= count;
    __vstack_globals_count = count;
}

static inline void vstack_push(size_t value) {
    ASSERT(__gc_stack_top > __vstack, 1, "Virtual stack overflow");
    *(__gc_stack_top--) = value;
}

static inline size_t vstack_pop() {
    ASSERT(__gc_stack_top + 1 != __gc_stack_bottom, 1, "Virtual stack underflow");
    return *(++__gc_stack_top);
}

static inline size_t vstack_top() {
    ASSERT(__gc_stack_top + 1 < __gc_stack_bottom, 1, "Virtual stack underflow");
    return *(__gc_stack_top + 1);
}

static inline size_t vstack_kth_from_end(size_t index) {
    ASSERT(__gc_stack_top + 1 + index < __gc_stack_bottom, 1, "Virtual stack underflow");
    return *(__gc_stack_top + 1 + index);
}

static inline size_t vstack_size() {
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

enum IpAdvance {
    Normal,
    Jump
};

struct {
    char *file_name;
    bytefile *file;
    IpAdvance advance;
    char *jump_target;
} interpreter;

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

static inline void cstack_init() {
    __cstack_top = __cstack + CSTACK_SIZE - 1;
    __cstack_bottom = __cstack + CSTACK_SIZE;

    // init main frame
    __cstack_top->base = __gc_stack_top;
    __cstack_top->return_ip = NULL;
}

static inline frame *cstack_call(char *return_ip, size_t args_count, bool is_closure) {
    ASSERT(__cstack_top > __cstack, 1, "Call stack overflow");
    ASSERT(__gc_stack_top + args_count < __gc_stack_bottom, 1, "Virtual stack underflow");

    frame *top_frame = --__cstack_top;
    top_frame->return_ip = return_ip;
    top_frame->base = __gc_stack_top;
    top_frame->is_closure = is_closure;
    top_frame->args_count = args_count;
    return top_frame;
}

static inline void cstack_alloc(size_t locals_count) {
    ASSERT(__gc_stack_top - locals_count > __vstack, 1, "Virtual stack overflow");
    __gc_stack_top -= locals_count;
    __cstack_top->locals_count = locals_count;
}

/**
 * returns: return ip
 */
static inline char *cstack_end() {
    size_t return_value = vstack_pop();
    ASSERT(__cstack_top != __cstack_bottom, 1, "Call stack underflow");
    ASSERT(__cstack_top->base + __cstack_top->args_count <= __gc_stack_bottom, 1, "Virtual stack underflow");
    __gc_stack_top = __cstack_top->base + __cstack_top->args_count + __cstack_top->is_closure;
    vstack_push(return_value);
    return (__cstack_top++)->return_ip;
}

static inline size_t *loc(size_t location, int index) {
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
               index, closure_size);
        ptr = closure_content + 1 + index;
        break;
    default:
        FAIL(1, "Unexpected location type %d\n", location);
    }
    return ptr;
}

extern "C" int Lread();
extern "C" int Lwrite(int n);
extern "C" int Llength(void *p);
extern "C" void *Lstring(void *p);

extern "C" void *Bstring(void *p);
extern "C" int LtagHash(char *s);
extern "C" void *Bsta(void *v, int i, void *x);
extern "C" void *Belem(void *p, int i);
extern "C" int Btag(void *d, int t, int n);
extern "C" int Barray_patt(void *d, int n);

extern "C" int Bstring_patt(void *x, void *y);
extern "C" int Bclosure_tag_patt(void *x);
extern "C" int Bboxed_patt(void *x);
extern "C" int Bunboxed_patt(void *x);
extern "C" int Barray_tag_patt(void *x);
extern "C" int Bstring_tag_patt(void *x);
extern "C" int Bsexp_tag_patt(void *x);

extern "C" void Bmatch_failure(void *v, char *fname, int line, int col);

extern "C" void *alloc_array(int);
extern "C" void *alloc_sexp(int);
extern "C" void *alloc_closure(int);

extern "C" void __init();
extern "C" void __shutdown();

static inline void *Barray(int n) {
    data *r = (data *)alloc_array(n);

    for (int i = n - 1; i >= 0; i--) {
        ((int *)r->contents)[i] = vstack_pop();
    }

    return r->contents;
}

static inline void *BSexp(int n, int tag) {
    int fields_cnt = n;
    data *r = (data *)alloc_sexp(fields_cnt);
    ((sexp *)r)->tag = 0;

    for (int i = n; i > 0; i--) {
        ((int *)r->contents)[i] = vstack_pop();
    }

    ((sexp *)r)->tag = tag;

    return (int *)r->contents;
}

static inline void *Bclosure(int n, void *entry) {
    data *r = (data *)alloc_closure(n + 1);
    ((void **)r->contents)[0] = entry;

    for (int i = n; i >= 1; --i) {
        ((int *)r->contents)[i] = vstack_pop();
    }

    return r->contents;
}

template <unsigned char opcode, typename... Args>
struct InterpreterFunctor {
    void operator()(Args... args) {
        ::std::cout << "Interpreter for opcode " << (int)opcode << " not implemented" << ::std::endl;
    }
};

template <>
struct InterpreterFunctor<Opcode_Const, int> {
    void operator()(int value) {
        vstack_push(BOX(value));
    }
};

template <>
struct InterpreterFunctor<Opcode_String, char *> {
    void operator()(char *ptr) {
        vstack_push((size_t)Bstring((void *)ptr));
    }
};

template <>
struct InterpreterFunctor<Opcode_SExp, char *, int> {
    void operator()(char *tag, int n) {
        vstack_push((size_t)BSexp(n, UNBOX(LtagHash(tag))));
    }
};

template <>
struct InterpreterFunctor<Opcode_StI> {
    void operator()() {
        size_t v = vstack_pop();
        *(size_t *)vstack_pop() = v;
        vstack_push(v);
    }
};

template <>
struct InterpreterFunctor<Opcode_StA> {
    void operator()() {
        size_t *v = (size_t *)vstack_pop();
        size_t i = vstack_pop();
        size_t *x = (size_t *)vstack_pop();
        size_t *ptr = (size_t *)Bsta(v, i, x);
        vstack_push((size_t)ptr);
    }
};

template <>
struct InterpreterFunctor<Opcode_Jmp, int> {
    void operator()(int target) {
        char *dst = interpreter.file->code_ptr + target;
        interpreter.advance = Jump;
        interpreter.jump_target = dst;
    }
};

template <>
struct InterpreterFunctor<Opcode_End> {
    void operator()() {
        interpreter.advance = Jump;
        interpreter.jump_target = cstack_end();
    }
};

template <>
struct InterpreterFunctor<Opcode_Ret> {
    void operator()() {
        interpreter.advance = Jump;
        interpreter.jump_target = cstack_end();
    }
};

template <>
struct InterpreterFunctor<Opcode_Drop> {
    void operator()() {
        vstack_pop();
    }
};

template <>
struct InterpreterFunctor<Opcode_Dup> {
    void operator()() {
        vstack_push(vstack_top());
    }
};

template <>
struct InterpreterFunctor<Opcode_Swap> {
    void operator()() {
        size_t fst = vstack_pop();
        size_t snd = vstack_pop();
        vstack_push(fst);
        vstack_push(snd);
    }
};

template <>
struct InterpreterFunctor<Opcode_Elem> {
    void operator()() {
        size_t i = vstack_pop();
        size_t *p = (size_t *)vstack_pop();
        vstack_push((size_t)Belem(p, i));
    }
};

template <>
struct InterpreterFunctor<Opcode_CJmpZ, int> {
    void operator()(int target) {
        char *dst = interpreter.file->code_ptr + target;
        if (UNBOX(vstack_pop()) == 0) {
            interpreter.advance = Jump;
            interpreter.jump_target = dst;
        }
    }
};

template <>
struct InterpreterFunctor<Opcode_CJmpNZ, int> {
    void operator()(int target) {
        char *dst = interpreter.file->code_ptr + target;
        if (UNBOX(vstack_pop()) != 0) {
            interpreter.advance = Jump;
            interpreter.jump_target = dst;
        }
    }
};

template <>
struct InterpreterFunctor<Opcode_Begin, int, int> {
    void operator()(int args_count, int locals_count) {
        cstack_alloc(locals_count);
    }
};

template <>
struct InterpreterFunctor<Opcode_CBegin, int, int> {
    void operator()(int args_count, int locals_count) {
        cstack_alloc(locals_count);
    }
};

template <>
struct InterpreterFunctor<Opcode_Closure, int, ::std::vector<LocationEntry>> {
    void operator()(int offset, ::std::vector<LocationEntry> locations) {
        for (const auto &location : locations) {
            vstack_push(*loc(location.kind, location.index));
        }
        vstack_push((size_t)Bclosure(locations.size(), (void *)offset));
    }
};

template <>
struct InterpreterFunctor<Opcode_CallC, char *, int> {
    void operator()(char *return_ip, int args_count) {
        char *entry = interpreter.file->code_ptr + *(int *)vstack_kth_from_end(args_count);
        cstack_call(return_ip, args_count, true);
        interpreter.advance = Jump;
        interpreter.jump_target = entry;
    }
};

template <>
struct InterpreterFunctor<Opcode_Call, char *, int, int> {
    void operator()(char *return_ip, int offset, int args_count) {
        char *dst = interpreter.file->code_ptr + offset;
        cstack_call(return_ip, args_count, false);
        interpreter.advance = Jump;
        interpreter.jump_target = dst;
    }
};

template <>
struct InterpreterFunctor<Opcode_Tag, char *, int> {
    void operator()(char *s, int args) {
        size_t tag = Btag((void *)vstack_pop(), LtagHash((char *)s), BOX(args));
        vstack_push(tag);
    }
};

template <>
struct InterpreterFunctor<Opcode_Array, int> {
    void operator()(int n) {
        size_t arr = (size_t)Barray_patt((void *)vstack_pop(), BOX(n));
        vstack_push(arr);
    }
};

template <>
struct InterpreterFunctor<Opcode_Fail, int, int> {
    void operator()(int line, int col) {
        Bmatch_failure((void *)vstack_pop(), interpreter.file_name, line, col);
    }
};

template <>
struct InterpreterFunctor<Opcode_Line, int> {
    void operator()(int line) {}
};

template <unsigned char opcode>
    requires((opcode >> 4) == HOpcode_Binop)
struct InterpreterFunctor<opcode> {
    void operator()() {
        int rhv = UNBOX(vstack_pop());
        int lhv = UNBOX(vstack_pop());
        int res;

#define CASE_BINOP(Binop_Name, op) \
    case Binop_Name:               \
        res = lhv op rhv;          \
        break;

        switch (opcode & 0x0F) {
            BINOPS(CASE_BINOP)
        }
        vstack_push(BOX(res));
    }
};

template <unsigned char opcode>
    requires((opcode >> 4) == HOpcode_Ld)
struct InterpreterFunctor<opcode, int> {
    void operator()(int index) {
        size_t *addr = loc(opcode & 0x0F, index);
        vstack_push(*addr);
    }
};

template <unsigned char opcode>
    requires((opcode >> 4) == HOpcode_LdA)
struct InterpreterFunctor<opcode, int> {
    void operator()(int index) {
        size_t *addr = loc(opcode & 0x0F, index);
        vstack_push((size_t)addr);
        vstack_push((size_t)addr);
    }
};

template <unsigned char opcode>
    requires((opcode >> 4) == HOpcode_St)
struct InterpreterFunctor<opcode, int> {
    void operator()(int index) {
        *loc(opcode & 0x0F, index) = vstack_top();
    }
};

template <unsigned char opcode>
    requires((opcode >> 4) == HOpcode_Patt)
struct InterpreterFunctor<opcode> {
    void operator()() {
        void *x = (void *)vstack_pop();
        switch (opcode & 0x0F) {
        case Pattern_String: {
            void *y = (void *)vstack_pop();
            vstack_push(Bstring_patt(x, y));
            break;
        }
        case Pattern_StringTag: {
            vstack_push(Bstring_tag_patt(x));
            break;
        }
        case Pattern_ArrayTag: {
            vstack_push(Barray_tag_patt(x));
            break;
        }
        case Pattern_SExpTag: {
            vstack_push(Bsexp_tag_patt(x));
            break;
        }
        case Pattern_Boxed: {
            vstack_push(Bboxed_patt(x));
            break;
        }
        case Pattern_Unboxed: {
            vstack_push(Bunboxed_patt(x));
            break;
        }
        case Pattern_ClosureTag: {
            vstack_push(Bclosure_tag_patt(x));
            break;
        }
        default: {
            FAIL(1, "Unexpected pattern: %d\n", (int)(opcode & 0x0F));
        }
        }
    }
};

template <unsigned char opcode>
    requires((opcode >> 4) == HOpcode_LCall && (opcode & 0x0F) != LCall_Barray)
struct InterpreterFunctor<opcode> {
    void operator()() {
        switch (opcode & 0x0F) {
        case LCall_Lread:
            vstack_push(Lread());
            break;
        case LCall_Lwrite:
            vstack_push(Lwrite(vstack_pop()));
            break;
        case LCall_Llength:
            vstack_push(Llength((void *)vstack_pop()));
            break;
        case LCall_Lstring:
            vstack_push((size_t)Lstring((void *)vstack_pop()));
            break;
        default:
            FAIL(1, "Unknown LCall");
        }
    }
};

template <>
struct InterpreterFunctor<COMPOSED(HOpcode_LCall, LCall_Barray), int> {
    void operator()(int n) {
        vstack_push((size_t)Barray(n));
    }
};

template <unsigned char opcode>
    requires((opcode >> 4) == HOpcode_Stop)
struct InterpreterFunctor<opcode> {
    void operator()() {
        __shutdown();
        exit(0);
    }
};

} // namespace

void interprete(char *file_name, bytefile *file, char *ip) {
    size_t line = 0;

    __init();
    vstack_init();
    vstack_alloc_globals(file->global_area_size);
    cstack_init();

    InstReader reader(file);

    interpreter.file_name = file_name;
    interpreter.file = file;

    do {
        if (ip == NULL) {
            __shutdown();
            break;
        }

#ifdef DEBUG_MODE
        dump_stack();
#endif // DEBUG_MODE
        CERR("Inst 0x%08x %d\n", (ip - file->code_ptr), (int)*ip);

        ip = reader.read_inst<InterpreterFunctor>(ip);
        if (interpreter.advance == Jump) [[unlikely]] {
            ip = interpreter.jump_target;
            interpreter.advance = Normal;
        }
    } while (1);
}
