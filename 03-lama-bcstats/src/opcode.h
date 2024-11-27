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
    Binop_Add = 0,
    Binop_Sub = 1,
    Binop_Mul = 2,
    Binop_Div = 3,
    Binop_Rem = 4,
    Binop_LessThan = 5,
    Binop_LessEqual = 6,
    Binop_GreaterThan = 7,
    Binop_GreaterEqual = 8,
    Binop_Equal = 9,
    Binop_NotEqual = 10,
    Binop_And = 11,
    Binop_Or = 12,
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

#endif // OPCODE_H