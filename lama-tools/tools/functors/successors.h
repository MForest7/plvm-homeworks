#ifndef FUNCTOR_SUCCESSORS_H
#define FUNCTOR_SUCCESSORS_H

#include "../opcode.h"
#include <cstdint>
#include <cstdlib>

#include <vector>

template <unsigned char opcode, typename... Args>
struct SuccessorsFunctor {
    const char *code_ptr;
    const char *next;
    std::vector<const char *> *successors;

    void operator()(Args... args) {
        *successors = {next};
    }
};

template <>
struct SuccessorsFunctor<Opcode_Jmp, int> {
    const char *code_ptr;
    const char *next;
    std::vector<const char *> *successors;

    void operator()(int target) {
        *successors = {code_ptr + target};
    }
};

template <>
struct SuccessorsFunctor<Opcode_CJmpNZ, int> {
    const char *code_ptr;
    const char *next;
    std::vector<const char *> *successors;

    void operator()(int target) {
        *successors = {code_ptr + target, next};
    }
};

template <>
struct SuccessorsFunctor<Opcode_CJmpZ, int> {
    const char *code_ptr;
    const char *next;
    std::vector<const char *> *successors;

    void operator()(int target) {
        *successors = {next, code_ptr + target};
    }
};

template <>
struct SuccessorsFunctor<Opcode_Call, char *, int, int> {
    const char *code_ptr;
    const char *next;
    std::vector<const char *> *successors;

    void operator()(char *, int target, int) {
        *successors = {code_ptr + target, next};
    }
};

template <>
struct SuccessorsFunctor<Opcode_Closure, int, std::vector<LocationEntry>> {
    const char *code_ptr;
    const char *next;
    std::vector<const char *> *successors;

    void operator()(int offset, std::vector<LocationEntry>) {
        *successors = {code_ptr + offset, next};
    }
};

template <unsigned char opcode, typename... Args>
    requires(opcode == SINGLE(Opcode_Ret) || opcode == SINGLE(Opcode_End) || opcode == SINGLE(Opcode_Fail) || opcode == COMPOSED(HOpcode_Stop, 0))
struct SuccessorsFunctor<opcode, Args...> {
    const char *code_ptr;
    const char *next;
    std::vector<const char *> *successors;

    void operator()(Args...) {
        *successors = {};
    }
};

#endif // FUNCTOR_SUCCESSORS_H
