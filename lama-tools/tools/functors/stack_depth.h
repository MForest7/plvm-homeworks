#ifndef FUNCTOR_STACK_DEPTH_H
#define FUNCTOR_STACK_DEPTH_H

#include "../error.h"
#include "../opcode.h"
#include <iostream>
#include <vector>

enum StackError {
    NoUnderflow = 0,
    GlobalUnderflow = 1,
    LocalUnderflow = 2,
    ArgUnderflow = 4,
    CaptureUnderflow = 8,
    CaptureOutOfClosure = 16
};

struct StackLayout {
    int globals;
    int locals;
    int args;
    int captured;
    std::vector<int> *jumps;
    bool is_closure;
};

inline void load_location(StackLayout *layout, const LocationEntry &location) {
    switch (location.kind) {
    case Location_Global:
        if (location.index > layout->globals) {
            FAIL(1, "Memory access failed: G(%d) is out of section (%d globals)",
                 location.index, layout->globals);
        }
        break;
    case Location_Local:
        if (location.index > layout->locals) {
            FAIL(1, "Memory access failed: L(%d) is out of frame (%d locals)",
                 location.index, layout->locals);
        }
        break;
    case Location_Arg:
        layout->args = std::max(layout->args, 1 + location.index);
        break;
    case Location_Captured:
        if (!layout->is_closure) {
            FAIL(1, "Memory access failed: C(%d) is invalid out of closure",
                 location.index);
        } else {
            layout->captured = std::max(layout->captured, 1 + location.index);
        }
        break;
    }
}

template <unsigned char opcode, typename... Args>
struct StackDepthFunctor {
    StackLayout *layout;
    inline void operator()(Args...) {}
};

template <unsigned char opcode, typename... Args>
    requires((opcode >> 4) == HOpcode_Binop)
struct StackDepthFunctor<opcode, Args...> {
    StackLayout *layout;
    inline void operator()(Args...) {
        layout->locals -= 1;
    }
};

template <unsigned char opcode, typename... Args>
    requires(opcode == Opcode_Const || opcode == Opcode_String || opcode == Opcode_SExp || opcode == Opcode_Dup)
struct StackDepthFunctor<opcode, Args...> {
    StackLayout *layout;
    inline void operator()(Args...) {
        layout->locals += 1;
    }
};

template <unsigned char opcode>
    requires((opcode >> 4) == HOpcode_Ld)
struct StackDepthFunctor<opcode, int> {
    StackLayout *layout;
    inline void operator()(int index) {
        load_location(layout, {(Location)(opcode & 0x0f), index});
        layout->locals += 1;
    }
};

template <unsigned char opcode>
    requires((opcode >> 4) == HOpcode_LdA)
struct StackDepthFunctor<opcode, int> {
    StackLayout *layout;
    inline void operator()(int index) {
        load_location(layout, {(Location)(opcode & 0x0f), index});
        layout->locals += 2;
    }
};

template <unsigned char opcode>
    requires((opcode >> 4) == HOpcode_St)
struct StackDepthFunctor<opcode, int> {
    StackLayout *layout;
    inline void operator()(int index) {
        load_location(layout, {(Location)(opcode & 0x0f), index});
    }
};

template <>
struct StackDepthFunctor<Opcode_StI> {
    StackLayout *layout;
    inline void operator()() {
        layout->locals -= 1;
    }
};

template <>
struct StackDepthFunctor<Opcode_StA> {
    StackLayout *layout;
    inline void operator()() {
        layout->locals -= 2;
    }
};

template <>
struct StackDepthFunctor<Opcode_Drop> {
    StackLayout *layout;
    inline void operator()() {
        layout->locals -= 1;
    }
};

template <>
struct StackDepthFunctor<Opcode_Elem> {
    StackLayout *layout;
    inline void operator()() {
        layout->locals -= 1;
    }
};

template <>
struct StackDepthFunctor<Opcode_Jmp, int> {
    StackLayout *layout;
    inline void operator()(int target) {
        layout->jumps->push_back(target);
    }
};

template <>
struct StackDepthFunctor<Opcode_CJmpZ, int> {
    StackLayout *layout;
    inline void operator()(int target) {
        layout->jumps->push_back(target);
        layout->locals -= 1;
    }
};

template <>
struct StackDepthFunctor<Opcode_CJmpNZ, int> {
    StackLayout *layout;
    inline void operator()(int target) {
        layout->jumps->push_back(target);
        layout->locals -= 1;
    }
};

template <>
struct StackDepthFunctor<Opcode_Begin, int, int> {
    StackLayout *layout;
    inline void operator()(int args_count, int locals_count) {
        layout->args = args_count;
        layout->is_closure = false;
        layout->locals = locals_count;
        layout->captured = 0;
    }
};

template <>
struct StackDepthFunctor<Opcode_CBegin, int, int> {
    StackLayout *layout;
    inline void operator()(int args_count, int locals_count) {
        layout->args = args_count;
        layout->is_closure = true;
        layout->locals = locals_count;
        layout->captured = 0;
    }
};

template <>
struct StackDepthFunctor<Opcode_Closure, int, std::vector<LocationEntry>> {
    StackLayout *layout;
    inline void operator()(int, std::vector<LocationEntry> capture) {
        for (const LocationEntry &loc : capture) {
            load_location(layout, loc);
        }
        layout->locals += 1;
    }
};

template <>
struct StackDepthFunctor<Opcode_Line, int> {
    StackLayout *layout;
    inline void operator()(int) {}
};

template <>
struct StackDepthFunctor<Opcode_CallC, char *, int> {
    StackLayout *layout;
    inline void operator()(char *, int) {}
};

template <>
struct StackDepthFunctor<Opcode_Call, char *, int, int> {
    StackLayout *layout;
    inline void operator()(char *, int, int) {
        layout->locals += 1;
    }
};

template <>
struct StackDepthFunctor<Opcode_Tag, char *, int> {
    StackLayout *layout;
    inline void operator()(char *, int) {}
};

template <>
struct StackDepthFunctor<Opcode_Array, int> {
    StackLayout *layout;
    inline void operator()(int) {}
};

template <>
struct StackDepthFunctor<Opcode_End> {
    StackLayout *layout;
    inline void operator()() {}
};

template <>
struct StackDepthFunctor<Opcode_Fail> {
    StackLayout *layout;
    inline void operator()() {
        layout->locals -= 1;
    }
};

template <>
struct StackDepthFunctor<COMPOSED(HOpcode_Patt, Pattern_String)> {
    StackLayout *layout;
    inline void operator()() {
        layout->locals -= 1;
    }
};

template <unsigned char pattern>
    requires((pattern >> 4) == HOpcode_Patt && (pattern & 0x0F) != Pattern_String)
struct StackDepthFunctor<pattern> {
    StackLayout *layout;
    inline void operator()() {}
};

template <>
struct StackDepthFunctor<COMPOSED(HOpcode_LCall, LCall_Lread)> {
    StackLayout *layout;
    inline void operator()() {
        layout->locals += 1;
    }
};

template <>
struct StackDepthFunctor<COMPOSED(HOpcode_LCall, LCall_Barray), int> {
    StackLayout *layout;
    inline void operator()(int) {
        layout->locals += 1;
    }
};

template <unsigned char lcall>
    requires((lcall >> 4) == HOpcode_LCall && (lcall & 0x0F) != LCall_Lread && (lcall & 0x0F) != LCall_Barray)
struct StackDepthFunctor<lcall> {
    StackLayout *layout;
    inline void operator()() {}
};

#endif // FUNCTOR_STACK_DEPTH_H
