#ifndef FUNCTOR_PRINT_INST_H
#define FUNCTOR_PRINT_INST_H

#include "../error.h"
#include "../opcode.h"

#include <ostream>
#include <vector>

template <unsigned char opcode>
inline const char *opcode_to_string() {
    constexpr unsigned char hi = (opcode & 0xF0) >> 4;
    constexpr unsigned char lo = opcode & 0x0F;

#define BINOP_TO_STRING(opcode, op) \
    if constexpr (lo == opcode) {   \
        return "BINOP " #op;        \
    }

    if constexpr (hi == HOpcode_Binop) {
        BINOPS(BINOP_TO_STRING)
    }
#undef BINOP_TO_STRING

#define HOPCODE_TO_STRING(code, str) \
    if constexpr (hi == code) {      \
        return str;                  \
    }

    HOPCODE_TO_STRING(HOpcode_Ld, "LD")
    HOPCODE_TO_STRING(HOpcode_LdA, "LDA")
    HOPCODE_TO_STRING(HOpcode_St, "ST")
    HOPCODE_TO_STRING(HOpcode_Stop, "STOP")
#undef HOPCODE_TO_STRING

#define PATTERN_TO_STRING(code, str) \
    if constexpr (lo == code) {      \
        return "PATT " str;          \
    }

    if constexpr (hi == HOpcode_Patt) {
        PATTERNS(PATTERN_TO_STRING)
    }
#undef PATTERN_TO_STRING

#define LCALL_TO_STRING(code, str) \
    if constexpr (lo == code) {    \
        return "LCALL " str;       \
    }

    if constexpr (hi == HOpcode_LCall) {
        LCALLS(LCALL_TO_STRING)
    }
#undef LCALL_TO_STRING

#define OPCODE_TO_STRING(code, str) \
    if constexpr (opcode == code) { \
        return str;                 \
    }

    OPCODE_TO_STRING(Opcode_Const, "CONST")
    OPCODE_TO_STRING(Opcode_String, "STRING")
    OPCODE_TO_STRING(Opcode_SExp, "SEXP")
    OPCODE_TO_STRING(Opcode_StI, "STI")
    OPCODE_TO_STRING(Opcode_StA, "STA")
    OPCODE_TO_STRING(Opcode_Jmp, "JMP")
    OPCODE_TO_STRING(Opcode_End, "END")
    OPCODE_TO_STRING(Opcode_Ret, "RET")
    OPCODE_TO_STRING(Opcode_Drop, "DROP")
    OPCODE_TO_STRING(Opcode_Dup, "DUP")
    OPCODE_TO_STRING(Opcode_Swap, "SWAP")
    OPCODE_TO_STRING(Opcode_Elem, "ELEM")
    OPCODE_TO_STRING(Opcode_CJmpZ, "CJMPz")
    OPCODE_TO_STRING(Opcode_CJmpNZ, "CJMPnz")
    OPCODE_TO_STRING(Opcode_Begin, "BEGIN")
    OPCODE_TO_STRING(Opcode_CBegin, "CBEGIN")
    OPCODE_TO_STRING(Opcode_Closure, "CLOSURE")
    OPCODE_TO_STRING(Opcode_CallC, "CALLC")
    OPCODE_TO_STRING(Opcode_Call, "CALL")
    OPCODE_TO_STRING(Opcode_Tag, "TAG")
    OPCODE_TO_STRING(Opcode_Array, "ARRAY")
    OPCODE_TO_STRING(Opcode_Fail, "FAIL")
    OPCODE_TO_STRING(Opcode_Line, "LINE")
#undef OPCODE_TO_STRING

    FAIL(1, "Unexpected opcode: %d", (int)opcode);
}

template <unsigned char opcode, typename... Args>
struct PrinterFunctor {
    std::ostream &out;
    inline void operator()(Args... args) {
        out << opcode_to_string<opcode>() << "\t";
        ((out << std::forward<Args>(args) << " "), ...);
    }
};

#define PRINT_LOCATION(hi, location, str)                     \
    template <>                                               \
    struct PrinterFunctor<COMPOSED(hi, location), int> {      \
        std::ostream &out;                                    \
        inline void operator()(int index) {                   \
            out << opcode_to_string<COMPOSED(hi, location)>() \
                << "\t" str "(" << index << ")";              \
        }                                                     \
    };

LOCATIONS(HOpcode_Ld, PRINT_LOCATION)
LOCATIONS(HOpcode_LdA, PRINT_LOCATION)
LOCATIONS(HOpcode_St, PRINT_LOCATION)
#undef PRINT_LOCATION

#define PRINT_OFFSET(opcode)                                  \
    template <>                                               \
    struct PrinterFunctor<SINGLE(opcode), int> {              \
        std::ostream &out;                                    \
        inline void operator()(int offset) {                  \
            out << opcode_to_string<SINGLE(opcode)>() << "\t" \
                << "0x" << std::hex << offset << std::dec;    \
        }                                                     \
    };

PRINT_OFFSET(Opcode_Jmp)
PRINT_OFFSET(Opcode_CJmpNZ)
PRINT_OFFSET(Opcode_CJmpZ)
#undef PRINT_OFFSET

template <>
struct PrinterFunctor<SINGLE(Opcode_Call), const char *, int, int> {
    std::ostream &out;
    inline void operator()(const char *, int offset, int argc) {
        out << opcode_to_string<Opcode_Call>() << "\t"
            << "0x" << std::hex << offset << std::dec << " "
            << argc;
    }
};

template <>
struct PrinterFunctor<SINGLE(Opcode_CallC), const char *, int, int> {
    std::ostream &out;
    inline void operator()(const char *, int argc) {
        out << opcode_to_string<Opcode_CallC>() << "\t" << argc;
    }
};

#define PRINT_PATTERN(pattern, str)                                            \
    template <>                                                                \
    struct PrinterFunctor<COMPOSED(HOpcode_Patt, pattern)> {                   \
        std::ostream &out;                                                     \
        inline void operator()() {                                             \
            out << opcode_to_string<COMPOSED(HOpcode_Patt, pattern)>() << "\t" \
                << str;                                                        \
        }                                                                      \
    };

PATTERNS(PRINT_PATTERN)
#undef PRINT_PATTERN

#define PRINT_LCALL(lcall, str)                                        \
    template <>                                                        \
    struct PrinterFunctor<COMPOSED(HOpcode_LCall, lcall)> {            \
        std::ostream &out;                                             \
        inline void operator()() {                                     \
            out << opcode_to_string<COMPOSED(HOpcode_LCall, lcall)>(); \
        }                                                              \
    };

LCALLS(PRINT_LCALL)
#undef PRINT_LCALL

template <>
struct PrinterFunctor<SINGLE(Opcode_Closure), int, std::vector<LocationEntry>> {
    static constexpr const char *locations[] = {"G", "L", "A", "C"};

    std::ostream &out;
    inline void operator()(int offset, std::vector<LocationEntry> args) {
        out << opcode_to_string<SINGLE(Opcode_Closure)>() << " "
            << std::hex << offset << std::dec << " ";
        for (const auto &entry : args) {
            out << locations[entry.kind] << "(" << entry.index << ")" << "\t";
        }
    }
};

#endif // FUNCTOR_PRINT_INST_H
