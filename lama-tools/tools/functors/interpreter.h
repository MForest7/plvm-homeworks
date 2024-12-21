#ifndef INTERPRETER_FUNCTOR_H
#define INTERPRETER_FUNCTOR_H

#include "../../runtime/runtime_common.h"
#include "../opcode.h"
#include "../runtime_externs.h"
#include "../stack.h"
#include <iostream>

char __interpreter_ip;

template <unsigned char opcode, typename... Args>
struct InterpreterFunctor {
    inline void operator()(Args... args) {
        std::cout << "Interpreter for opcode " << (int)opcode << " not implemented" << std::endl;
    }
};

template <>
struct InterpreterFunctor<Opcode_Const, int> {
    inline void operator()(int value) {
        vstack_push(BOX(value));
    }
};

template <>
struct InterpreterFunctor<Opcode_String, char *> {
    inline void operator()(char *ptr) {
        vstack_push((size_t)Bstring((void *)ptr));
    }
};

template <>
struct InterpreterFunctor<Opcode_SExp, char *, int> {
    inline void operator()(char *tag, int n) {
        vstack_push((size_t)BSexp(n, UNBOX(LtagHash(tag))));
    }
};

template <>
struct InterpreterFunctor<Opcode_StI> {
    inline void operator()(char *tag, int n) {
        size_t v = vstack_pop();
        *(size_t *)vstack_pop() = v;
        vstack_push(v);
    }
};

template <>
struct InterpreterFunctor<Opcode_StA> {
    inline void operator()(char *tag, int n) {
        size_t *v = (size_t *)vstack_pop();
        size_t i = vstack_pop();
        size_t *x = (size_t *)vstack_pop();
        size_t *ptr = (size_t *)Bsta(v, i, x);
        vstack_push((size_t)ptr);
    }
};

template <>
struct InterpreterFunctor<Opcode_Jmp> {
    inline void operator()(char *tag, int n) {
        size_t v = vstack_pop();
        *(size_t *)vstack_pop() = v;
        vstack_push(v);
    }
};

#endif // INTERPRETER_FUNCTOR_H
