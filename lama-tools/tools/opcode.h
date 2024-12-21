#ifndef OPCODE_H
#define OPCODE_H

enum HOpcode {
    HOpcode_Binop = 0,
    HOpcode_Ld = 2,
    HOpcode_LdA = 3,
    HOpcode_St = 4,
    HOpcode_Patt = 6,
    HOpcode_LCall = 7,
    HOpcode_Stop = 15,
};

enum Binop {
    Binop_Add = 1,
    Binop_Sub = 2,
    Binop_Mul = 3,
    Binop_Div = 4,
    Binop_Rem = 5,
    Binop_LessThan = 6,
    Binop_LessEqual = 7,
    Binop_GreaterThan = 8,
    Binop_GreaterEqual = 9,
    Binop_Equal = 10,
    Binop_NotEqual = 11,
    Binop_And = 12,
    Binop_Or = 13,
};

enum Opcode {
    Opcode_Const = 16,
    Opcode_String = 17,
    Opcode_SExp = 18,
    Opcode_StI = 19,
    Opcode_StA = 20,
    Opcode_Jmp = 21,
    Opcode_End = 22,
    Opcode_Ret = 23,
    Opcode_Drop = 24,
    Opcode_Dup = 25,
    Opcode_Swap = 26,
    Opcode_Elem = 27,

    Opcode_CJmpZ = 80,
    Opcode_CJmpNZ = 81,
    Opcode_Begin = 82,
    Opcode_CBegin = 83,
    Opcode_Closure = 84,
    Opcode_CallC = 85,
    Opcode_Call = 86,
    Opcode_Tag = 87,
    Opcode_Array = 88,
    Opcode_Fail = 89,
    Opcode_Line = 90,
};

enum LCall {
    LCall_Lread = 0,
    LCall_Lwrite = 1,
    LCall_Llength = 2,
    LCall_Lstring = 3,
    LCall_Barray = 4,
};

enum Location {
    Location_Global = 0,
    Location_Local = 1,
    Location_Arg = 2,
    Location_Captured = 3,
};

enum Pattern {
    Pattern_String = 0,
    Pattern_StringTag = 1,
    Pattern_ArrayTag = 2,
    Pattern_SExpTag = 3,
    Pattern_Boxed = 4,
    Pattern_Unboxed = 5,
    Pattern_ClosureTag = 6
};

#define BINOPS(MACRO)             \
    MACRO(Binop_Add, +)           \
    MACRO(Binop_Sub, -)           \
    MACRO(Binop_Mul, *)           \
    MACRO(Binop_Div, /)           \
    MACRO(Binop_Rem, %)           \
    MACRO(Binop_LessThan, <)      \
    MACRO(Binop_LessEqual, <=)    \
    MACRO(Binop_GreaterThan, >)   \
    MACRO(Binop_GreaterEqual, >=) \
    MACRO(Binop_Equal, ==)        \
    MACRO(Binop_NotEqual, !=)     \
    MACRO(Binop_And, &&)          \
    MACRO(Binop_Or, ||)

#define LOCATIONS(hi, MACRO)        \
    MACRO(hi, Location_Global, "G") \
    MACRO(hi, Location_Local, "L")  \
    MACRO(hi, Location_Arg, "A")    \
    MACRO(hi, Location_Captured, "C")

struct LocationEntry {
    Location kind;
    int index;
};

#define LCALLS(MACRO)               \
    MACRO(LCall_Lread, "Lread")     \
    MACRO(LCall_Lwrite, "Lwrite")   \
    MACRO(LCall_Llength, "Llength") \
    MACRO(LCall_Lstring, "Lstring") \
    MACRO(LCall_Barray, "Barray")

#define PATTERNS(MACRO)                 \
    MACRO(Pattern_String, "=str")       \
    MACRO(Pattern_StringTag, "#string") \
    MACRO(Pattern_ArrayTag, "#array")   \
    MACRO(Pattern_SExpTag, "#sexp")     \
    MACRO(Pattern_Boxed, "#ref")        \
    MACRO(Pattern_Unboxed, "#val")      \
    MACRO(Pattern_ClosureTag, "#fun")

#define SINGLE(code) ((unsigned char)(code))
#define COMPOSED(hi, lo) ((unsigned char)(((hi) << 4) | (lo)))

#endif // OPCODE_H
