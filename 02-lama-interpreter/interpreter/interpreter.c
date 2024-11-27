#include "../runtime/gc.h"
#include "../runtime/runtime.h"
#include "../runtime/runtime_common.h"
#include "bytefile.h"
#include "opcode.h"

#define FAIL(code, ...)                                                          \
    do {                                                                         \
        static_assert((code), "Expected non-zero integer literal");              \
        fprintf(stderr, "Failed at line %d with code %d\n\t", __LINE__, (code)); \
        fprintf(stderr, __VA_ARGS__);                                            \
        fprintf(stderr, "\n");                                                   \
        exit(code);                                                              \
    } while (0)

// #define DEBUG_MODE

#ifdef DEBUG_MODE
#define SAFE_MODE
#define CERR(...) fprintf(stderr, __VA_ARGS__);
#else
#define CERR(...)
#endif // DEBUG_MODE

#ifdef SAFE_MODE
#define ASSERT(condition, code, ...)   \
    do {                               \
        if (!(condition))              \
            FAIL((code), __VA_ARGS__); \
    } while (0)
#else
#define ASSERT(condition, code, ...)
#endif // SAFE_MODE

#define VSTACK_SIZE 65536
#define CSTACK_SIZE 65536

extern size_t *__gc_stack_top;
extern size_t *__gc_stack_bottom;

size_t __vstack[VSTACK_SIZE];

void vstack_init() {
    __gc_stack_bottom = __vstack + VSTACK_SIZE;
    __gc_stack_top = __gc_stack_bottom - 1;
}

void vstack_alloc_globals(size_t count) {
    ASSERT(VSTACK_SIZE >= count, 1, "Cannot allocate memory for globals");
    __gc_stack_top -= count;
}

void vstack_push(size_t value) {
    ASSERT(__gc_stack_top > __vstack, 1, "Virtual stack overflow");
    *(__gc_stack_top--) = value;
}

size_t vstack_pop() {
    ASSERT(__gc_stack_top + 1 != __gc_stack_bottom, 1, "Virtual stack underflow");
    return *(++__gc_stack_top);
}

size_t vstack_top() {
    ASSERT(__gc_stack_top + 1 < __gc_stack_bottom, 1, "Virtual stack underflow");
    return *(__gc_stack_top + 1);
}

size_t vstack_kth_from_end(size_t index) {
    ASSERT(__gc_stack_top + 1 + index < __gc_stack_bottom, 1, "Virtual stack underflow");
    return *(__gc_stack_top + 1 + index);
}

size_t vstack_size() {
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

void cstack_init() {
    __cstack_top = __cstack + CSTACK_SIZE - 1;
    __cstack_bottom = __cstack + CSTACK_SIZE;

    // init main frame
    __cstack_top->base = __gc_stack_top;
    __cstack_top->return_ip = NULL;
}

frame *cstack_call(char *return_ip, size_t args_count, bool is_closure) {
    ASSERT(__cstack_top > __cstack, 1, "Call stack overflow");
    ASSERT(__gc_stack_top + args_count < __gc_stack_bottom, 1, "Virtual stack underflow");

    frame *top_frame = --__cstack_top;
    top_frame->return_ip = return_ip;
    top_frame->base = __gc_stack_top;
    top_frame->is_closure = is_closure;
    top_frame->args_count = args_count;
    return top_frame;
}

void *cstack_alloc(size_t locals_count) {
    ASSERT(__gc_stack_top - locals_count > __vstack, 1, "Virtual stack overflow");
    __gc_stack_top -= locals_count;
    __cstack_top->locals_count = locals_count;
}

/**
 * returns: return ip
 */
char *cstack_end() {
    size_t return_value = vstack_pop();
    ASSERT(__cstack_top != __cstack_bottom, 1, "Call stack underflow");
    ASSERT(__cstack_top->base + __cstack_top->args_count <= __gc_stack_bottom, 1, "Virtual stack underflow");
    __gc_stack_top = __cstack_top->base + __cstack_top->args_count + __cstack_top->is_closure;
    vstack_push(return_value);
    return (__cstack_top++)->return_ip;
}

size_t *loc(size_t location, int index) {
    switch (location) {
    case Location_Global:
        return __gc_stack_bottom - 1 - index;
    case Location_Local:
        return __cstack_top->base - index;
    case Location_Arg:
        return __cstack_top->base + (__cstack_top->args_count - index);
    case Location_Captured:
        return *(size_t **)(__cstack_top->base + __cstack_top->args_count + 1) + 1 + index;
    }
    FAIL(1, "Unexpected location type %d\n", location);
}

extern int Lread();
extern int Lwrite(int n);
extern int Llength(void *p);
extern void *Lstring(void *p);

extern void *Bstring(void *p);
extern int LtagHash(char *s);
extern void *Bsta(void *v, int i, void *x);
extern void *Belem(void *p, int i);
extern int Btag(void *d, int t, int n);
extern int Barray_patt(void *d, int n);

extern int Bstring_patt(void *x, void *y);
extern int Bclosure_tag_patt(void *x);
extern int Bboxed_patt(void *x);
extern int Bunboxed_patt(void *x);
extern int Barray_tag_patt(void *x);
extern int Bstring_tag_patt(void *x);
extern int Bsexp_tag_patt(void *x);

extern void Bmatch_failure(void *v, char *fname, int line, int col);

static void *Barray(int n) {
    data *r = (data *)alloc_array(n);

    for (int i = n - 1; i >= 0; i--) {
        ((int *)r->contents)[i] = vstack_pop();
    }

    return r->contents;
}

static void *BSexp(int n, int tag) {
    int fields_cnt = n;
    data *r = (data *)alloc_sexp(fields_cnt);
    ((sexp *)r)->tag = 0;

    for (int i = n; i > 0; i--) {
        ((int *)r->contents)[i] = vstack_pop();
    }

    ((sexp *)r)->tag = tag;

    return (int *)r->contents;
}

static void *Bclosure(int n, void *entry) {
    data *r = (data *)alloc_closure(n + 1);
    ((void **)r->contents)[0] = entry;

    for (int i = n; i >= 1; --i) {
        ((int *)r->contents)[i] = vstack_pop();
    }

    return r->contents;
}

int main(int argc, char *argv[]) {
    bytefile *file = read_file(argv[1]);

    char *ip = file->code_ptr;

#define INT (ip += sizeof(int), *(int *)(ip - sizeof(int)))
#define PTR (ip += sizeof(char *), *(char **)(ip - sizeof(char *)))
#define BYTE *ip++
#define STRING get_string(file, INT)

    __init();
    vstack_init();
    vstack_alloc_globals(file->global_area_size);
    cstack_init();

    do {
        if (ip == NULL) {
            __shutdown();
            exit(0);
        }

#ifdef DEBUG_MODE
        dump_stack();
#endif // DEBUG_MODE
        CERR("Inst 0x%08x %d\n", (ip - file->code_ptr), (int)*ip);

        char x = BYTE,
             h = (x & 0xF0) >> 4,
             l = x & 0x0F;

        switch (x) {
        case Opcode_Const: {
            vstack_push(BOX(INT));
            break;
        }

        case Opcode_String: {
            const char *p = STRING;
            size_t str = (size_t)Bstring((void *)p);
            vstack_push(str);
            break;
        }

        case Opcode_SExp: {
            char *s = STRING;
            size_t n = INT;
            vstack_push((size_t)BSexp(n, UNBOX(LtagHash(s))));
            break;
        }

        case Opcode_StI: {
            size_t v = vstack_pop();
            *(size_t *)vstack_pop() = v;
            vstack_push(v);
            break;
        }

        case Opcode_StA: {
            size_t *v = (size_t *)vstack_pop();
            size_t i = vstack_pop();
            size_t *x = (size_t *)vstack_pop();
            size_t *ptr = Bsta(v, i, x);
            vstack_push((size_t)ptr);
            break;
        }

        case Opcode_Jmp: {
            ip = file->code_ptr + INT;
            break;
        }

        case Opcode_End:
        case Opcode_Ret: {
            ip = cstack_end();
            break;
        }

        case Opcode_Drop: {
            vstack_pop();
            break;
        }

        case Opcode_Dup: {
            vstack_push(vstack_top());
            break;
        }

        case Opcode_Swap: {
            size_t fst = vstack_pop();
            size_t snd = vstack_pop();
            vstack_push(fst);
            vstack_push(snd);
            break;
        }

        case Opcode_Elem: {
            size_t i = vstack_pop();
            size_t *p = (size_t *)vstack_pop();
            vstack_push((size_t)Belem(p, i));
            break;
        }

        case Opcode_CJmpZ: {
            char *dst = file->code_ptr + INT;
            if (UNBOX(vstack_pop()) == 0) {
                ip = dst;
            }
            break;
        }

        case Opcode_CJmpNZ: {
            char *dst = file->code_ptr + INT;
            if (UNBOX(vstack_pop()) != 0) {
                ip = dst;
            }
            break;
        }

        case Opcode_Begin:
        case Opcode_CBegin: {
            size_t args_count = INT;
            size_t locals_count = INT;
            cstack_alloc(locals_count);
            break;
        }

        case Opcode_Closure: {
            char *entry = PTR;
            size_t n = INT;
            for (size_t i = 0; i < n; i++) {
                char location = BYTE;
                size_t index = INT;
                vstack_push(*loc(location, index));
            }
            vstack_push((size_t)Bclosure(n, (void *)entry));
            break;
        }

        case Opcode_CallC: {
            size_t args_count = INT;
            char *entry = file->code_ptr + *(int *)vstack_kth_from_end(args_count);
            cstack_call(ip, args_count, true);
            ip = entry;
            break;
        }

        case Opcode_Call: {
            char *dst = file->code_ptr + INT;
            size_t args_count = INT;
            cstack_call(ip, args_count, false);
            ip = dst;
            break;
        }

        case Opcode_Tag: {
            const char *s = STRING;
            size_t args = INT;
            size_t tag = Btag((void *)vstack_pop(), LtagHash((void *)s), BOX(args));
            vstack_push(tag);
            break;
        }

        case Opcode_Array: {
            size_t n = INT;
            size_t arr = (size_t)Barray_patt((void *)vstack_pop(), BOX(n));
            vstack_push(arr);
            break;
        }

        case Opcode_Fail: {
            size_t line = INT;
            size_t col = INT;
            Bmatch_failure((void *)vstack_pop(), argv[1], line, col);
            break;
        }

        case Opcode_Line: {
            size_t line = INT;
            break;
        }

        default:
            switch (h) {
            case HOpcode_Binop: {
                int rhv = UNBOX(vstack_pop());
                int lhv = UNBOX(vstack_pop());
                int res;
                switch (l) {
                case Binop_Add:
                    res = lhv + rhv;
                    break;
                case Binop_Sub:
                    res = lhv - rhv;
                    break;
                case Binop_Mul:
                    res = lhv * rhv;
                    break;
                case Binop_Div:
                    res = lhv / rhv;
                    break;
                case Binop_Rem:
                    res = lhv % rhv;
                    break;
                case Binop_LessThan:
                    res = lhv < rhv;
                    break;
                case Binop_LessEqual:
                    res = lhv <= rhv;
                    break;
                case Binop_GreaterThan:
                    res = lhv > rhv;
                    break;
                case Binop_GreaterEqual:
                    res = lhv >= rhv;
                    break;
                case Binop_Equal:
                    res = lhv == rhv;
                    break;
                case Binop_NotEqual:
                    res = lhv != rhv;
                    break;
                case Binop_And:
                    res = lhv && rhv;
                    break;
                case Binop_Or:
                    res = lhv || rhv;
                    break;
                }
                vstack_push(BOX(res));
                break;
            }
            case HOpcode_Ld: {
                size_t *addr = loc(l, INT);
                vstack_push(*addr);
                break;
            }
            case HOpcode_LdA: {
                size_t *addr = loc(l, INT);
                vstack_push((size_t)addr);
                vstack_push((size_t)addr);
                break;
            }
            case HOpcode_St: {
                *loc(l, INT) = vstack_top();
                break;
            }
            case HOpcode_Patt: {
                void *x = (void *)vstack_pop();
                switch (l) {
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
                    FAIL(1, "Unexpected pattern: %d\n", (int)l);
                }
                }
                break;
            }
            case HOpcode_LCall:
                switch (l) {
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
                case LCall_Barray:
                    vstack_push((size_t)Barray(INT));
                    break;
                default:
                    FAIL(1, "Unknown LCall");
                }
                break;
            case HOpcode_Stop:
                __shutdown();
                exit(0);
            default:
                FAIL(1, "Unknown opcode %d\n", x);
            }
        }
    } while (1);
}
