#ifndef INST_READER_H
#define INST_READER_H

#include "bytefile.h"
#include "error.h"
#include "opcode.h"

#include <vector>

class InstReader {
public:
    InstReader(bytefile *file) {
        this->file = file;
    }

private:
    bytefile *file;
    char *ip;
    size_t line;

    inline void assert_can_read(int bytes) {
        ASSERT(file->code_ptr <= ip, 1,
               "ip is out of code section");
        ASSERT(ip + bytes <= (char *)file + file->size, 1,
               "ip is out of code section");
    }

    inline char read_byte() {
        assert_can_read(1);
        ip += sizeof(char);
        return *(ip - sizeof(char));
    }

    inline int read_int() {
        assert_can_read(4);
        ip += sizeof(int);
        return *(int *)(ip - sizeof(int));
    }

    inline char *read_string() {
        return get_string(file, read_int());
    }

    inline LocationEntry read_loc() {
        Location kind = (Location)read_byte();
        int index = read_int();
        return LocationEntry{kind, index};
    }

public:
    /**
     * Applies `(Functor{...args}` to instruction (and parameters) after `ip`.
     * Returns a pointer after the instruction.
     */
    template <template <unsigned char, typename...> typename Functor, typename... Args> //, template <unsigned char opcode, typename... Args> typename (Functor>
    char *read_inst(char *inst_ptr, Args... args) {
        ip = inst_ptr;
        unsigned char x = read_byte(),
                      h = (x & 0xF0) >> 4,
                      l = x & 0x0F;

        switch (x) {
        case Opcode_Const: {
            int value = read_int();
            (Functor<SINGLE(Opcode_Const), int>{args...})(value);
            break;
        }

        case Opcode_String: {
            char *value = read_string();
            (Functor<SINGLE(Opcode_String), char *>{args...})(value);
            break;
        }

        case Opcode_SExp: {
            char *sexp = read_string();
            int argc = read_int();
            (Functor<SINGLE(Opcode_SExp), char *, int>{args...})(sexp, argc);
            break;
        }

        case Opcode_StI: {
            (Functor<SINGLE(Opcode_StI)>{args...})();
            break;
        }

        case Opcode_StA: {
            (Functor<SINGLE(Opcode_StA)>{args...})();
            break;
        }

        case Opcode_Jmp: {
            int target = read_int();
            (Functor<SINGLE(Opcode_Jmp), int>{args...})(target);
            break;
        }

        case Opcode_End: {
            (Functor<SINGLE(Opcode_End)>{args...})();
            break;
        }

        case Opcode_Ret: {
            (Functor<SINGLE(Opcode_Ret)>{args...})();
            break;
        }

        case Opcode_Drop: {
            (Functor<SINGLE(Opcode_Drop)>{args...})();
            break;
        }

        case Opcode_Dup: {
            (Functor<SINGLE(Opcode_Dup)>{args...})();
            break;
        }

        case Opcode_Swap: {
            (Functor<SINGLE(Opcode_Swap)>{args...})();
            break;
        }

        case Opcode_Elem: {
            (Functor<SINGLE(Opcode_Elem)>{args...})();
            break;
        }

        case Opcode_CJmpZ: {
            int target = read_int();
            (Functor<SINGLE(Opcode_CJmpZ), int>{args...})(target);
            break;
        }

        case Opcode_CJmpNZ: {
            int target = read_int();
            (Functor<SINGLE(Opcode_CJmpNZ), int>{args...})(target);
            break;
        }

        case Opcode_Begin: {
            int argc = read_int();
            int locc = read_int();
            (Functor<SINGLE(Opcode_Begin), int, int>{args...})(argc, locc);
            break;
        }

        case Opcode_CBegin: {
            int argc = read_int();
            int locc = read_int();
            (Functor<SINGLE(Opcode_CBegin), int, int>{args...})(argc, locc);
            break;
        }

        case Opcode_Closure: {
            int entry = read_int();
            std::vector<LocationEntry> captured;
            {
                int n = read_int();
                for (int i = 0; i < n; i++) {
                    captured.push_back(read_loc());
                }
            }
            (Functor<SINGLE(Opcode_Closure), int, std::vector<LocationEntry>>{args...})(entry, captured);
            break;
        }

        case Opcode_CallC: {
            int argc = read_int();
            (Functor<SINGLE(Opcode_CallC), char *, int>{args...})(ip, argc);
            break;
        }

        case Opcode_Call: {
            int offset = read_int();
            int argc = read_int();
            (Functor<SINGLE(Opcode_Call), char *, int, int>{args...})(ip, offset, argc);
            break;
        }

        case Opcode_Tag: {
            char *tag = read_string();
            int argc = read_int();
            (Functor<SINGLE(Opcode_Tag), char *, int>{args...})(tag, argc);
            break;
        }

        case Opcode_Array: {
            int size = read_int();
            (Functor<SINGLE(Opcode_Array), int>{args...})(size);
            break;
        }

        case Opcode_Fail: {
            int line = read_int();
            int col = read_int();
            (Functor<SINGLE(Opcode_Fail), int, int>{args...})(line, col);
            break;
        }

        case Opcode_Line: {
            int line = read_int();
            (Functor<SINGLE(Opcode_Line), int>{args...})(line);
            break;
        }

        default:

#define CASE_BINOP(Binop_Code, op)                                 \
    case Binop_Code:                                               \
        (Functor<COMPOSED(HOpcode_Binop, Binop_Code)>{args...})(); \
        break;

            switch (h) {
            case HOpcode_Binop: {
                switch (l) {
                    BINOPS(CASE_BINOP)
                }
                break;
            }
#undef CASE_BINOP

#define CASE_LOCATION(hi, location, _)                               \
    case location:                                                   \
        (Functor<COMPOSED(hi, location), int>{args...})(read_int()); \
        break;

            case HOpcode_Ld: {
                switch (l) {
                    LOCATIONS(HOpcode_Ld, CASE_LOCATION)
                }
                break;
            }
            case HOpcode_LdA: {
                switch (l) {
                    LOCATIONS(HOpcode_LdA, CASE_LOCATION)
                }
                break;
            }
            case HOpcode_St: {
                switch (l) {
                    LOCATIONS(HOpcode_St, CASE_LOCATION)
                }
                break;
            }
#undef CASE_LOCATION

#define CASE_PATTERN(pattern, _)                               \
    case pattern:                                              \
        (Functor<COMPOSED(HOpcode_Patt, pattern)>{args...})(); \
        break;

            case HOpcode_Patt: {
                switch (l) {
                    PATTERNS(CASE_PATTERN)
                }
                break;
            }
#undef CASE_PATTERN

            case HOpcode_LCall: {
                switch (l) {
                case LCall_Lread: {
                    (Functor<COMPOSED(HOpcode_LCall, LCall_Lread)>{args...})();
                    break;
                }
                case LCall_Lwrite: {
                    (Functor<COMPOSED(HOpcode_LCall, LCall_Lwrite)>{args...})();
                    break;
                }
                case LCall_Llength: {
                    (Functor<COMPOSED(HOpcode_LCall, LCall_Llength)>{args...})();
                    break;
                }
                case LCall_Lstring: {
                    (Functor<COMPOSED(HOpcode_LCall, LCall_Lstring)>{args...})();
                    break;
                }
                case LCall_Barray: {
                    (Functor<COMPOSED(HOpcode_LCall, LCall_Barray), int>{args...})(read_int());
                    break;
                }
                default:
                    FAIL(1, "Unknown LCall");
                }
                break;
            }
            case HOpcode_Stop: {
                (Functor<COMPOSED(HOpcode_Stop, 0)>{args...})();
                break;
            }
            default:
                FAIL(1, "Unknown opcode %d\n", x);
            }
        }

        return ip;
    }
};

#endif // INST_READER_H
