#include "bytefile.h"
#include "opcode.h"

#include <algorithm>
#include <array>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <queue>
#include <sstream>
#include <vector>

#include <cstdlib>

#define FAIL(code, ...)                                                          \
    do {                                                                         \
        static_assert((code), "Expected non-zero integer literal");              \
        fprintf(stderr, "Failed at line %d with code %d\n\t", __LINE__, (code)); \
        fprintf(stderr, __VA_ARGS__);                                            \
        fprintf(stderr, "\n");                                                   \
        exit(code);                                                              \
    } while (0)

struct Inst {
    char *begin;
    char *end;
    bool end_block;
    std::vector<char *> successors;
    std::string str;
};

class InstReader {
public:
    InstReader(bytefile *file) {
        this->file = file;
    }

    bytefile *file;

private:
    char *ip;

    char read_byte() {
        return *ip++;
    }

    size_t read_int() {
        ip += sizeof(size_t);
        return *(size_t *)(ip - sizeof(size_t));
    }

    char *read_string() {
        size_t pos = read_int();
        return get_string(file, pos);
    }

    static constexpr const char *ops[] = {"+", "-", "*", "/", "%", "<", "<=", ">", ">=", "==", "!=", "&&", "!!"};
    static constexpr const char *pats[] = {"=str", "#string", "#array", "#sexp", "#ref", "#val", "#fun"};
    static constexpr const char *lds[] = {"LD", "LDA", "ST"};
    static constexpr const char *locs[] = {"G", "L", "A", "C"};

public:
    Inst read_inst(char *inst_ptr) {
        bool end_block = false;
        char *begin = ip = inst_ptr;
        std::vector<char *> successors;

        std::stringstream dump;

        char x = read_byte(),
             h = (x & 0xF0) >> 4,
             l = x & 0x0F;

        switch (x) {
        case Opcode_Const: {
            dump << "CONST\t" << read_int();
            break;
        }

        case Opcode_String: {
            dump << "STRING\t" << read_string();
            break;
        }

        case Opcode_SExp: {
            dump << "SEXP\t" << read_string() << " " << read_int();
            break;
        }

        case Opcode_StI: {
            dump << "STI";
            break;
        }

        case Opcode_StA: {
            dump << "STA";
            break;
        }

        case Opcode_Jmp: {
            size_t jmp = read_int();
            dump << "JMP\t0x" << std::hex << jmp << std::dec;
            successors.push_back(file->code_ptr + jmp);
            end_block = true;
            break;
        }

        case Opcode_End: {
            dump << "END";
            end_block = true;
            break;
        }

        case Opcode_Ret: {
            dump << "RET";
            end_block = true;
            break;
        }

        case Opcode_Drop: {
            dump << "DROP";
            break;
        }

        case Opcode_Dup: {
            dump << "DUP";
            break;
        }

        case Opcode_Swap: {
            dump << "SWAP";
            break;
        }

        case Opcode_Elem: {
            dump << "ELEM";
            break;
        }

        case Opcode_CJmpZ: {
            size_t jmp = read_int();
            dump << "CJMPz\t0x" << std::hex << jmp << std::dec;
            successors.push_back(file->code_ptr + jmp);
            successors.push_back(ip);
            end_block = true;
            break;
        }

        case Opcode_CJmpNZ: {
            size_t jmp = read_int();
            dump << "CJMPnz\t0x" << std::hex << jmp << std::dec;
            successors.push_back(file->code_ptr + jmp);
            successors.push_back(ip);
            end_block = true;
            break;
        }

        case Opcode_Begin: {
            size_t args_count = read_int();
            size_t locals_count = read_int();
            dump << "BEGIN\t" << args_count << " " << locals_count;
            break;
        }

        case Opcode_CBegin: {
            size_t args_count = read_int();
            size_t locals_count = read_int();
            dump << "CBEGIN\t" << args_count << " " << locals_count;
            break;
        }

        case Opcode_Closure: {
            char *entry = (char *)read_int();
            dump << "CLOSURE\t0x" << std::hex << (size_t)entry << std::dec;
            {
                int n = read_int();
                for (int i = 0; i < n; i++) {
                    dump << locs[read_byte()] << "(" << read_int() << ")";
                }
            };
            successors.push_back(entry);
            break;
        }

        case Opcode_CallC: {
            dump << "CALLC\t" << read_int();
            break;
        }

        case Opcode_Call: {
            size_t call_addr = read_int();
            size_t args_count = read_int();
            dump << "CALL\t0x" << std::hex << call_addr << std::dec << " " << args_count;
            successors.push_back(file->code_ptr + call_addr);
            break;
        }

        case Opcode_Tag: {
            dump << "TAG\t" << read_string() << " " << read_int();
            break;
        }

        case Opcode_Array: {
            dump << "ARRAY\t" << read_int();
            break;
        }

        case Opcode_Fail: {
            dump << "FAIL\t" << read_int() << " " << read_int();
            end_block = true;
            break;
        }

        case Opcode_Line: {
            dump << "LINE\t" << read_int();
            break;
        }

        default:
            switch (h) {
            case HOpcode_Binop: {
                dump << "BINOP\t" << ops[l - 1];
                break;
            }
            case HOpcode_Ld:
            case HOpcode_LdA:
            case HOpcode_St: {
                dump << lds[h - 2] << "\t";
                dump << locs[l] << "(" << read_int() << ")";
                break;
            }
            case HOpcode_Patt: {
                dump << "PATT\t" << pats[l];
                break;
            }
            case HOpcode_LCall: {
                switch (l) {
                case LCall_Lread: {
                    dump << "CALL\tLread";
                    break;
                }
                case LCall_Lwrite: {
                    dump << "CALL\tLwrite";
                    break;
                }
                case LCall_Llength: {
                    dump << "CALL\tLlength";
                    break;
                }
                case LCall_Lstring: {
                    dump << "CALL\tLstring";
                    break;
                }
                case LCall_Barray: {
                    dump << "CALL\tBarray\t" << read_int();
                    break;
                }
                default:
                    FAIL(1, "Unknown LCall");
                }
                break;
            }
            case HOpcode_Stop: {
                dump << "STOP";
                break;
            }
            default:
                FAIL(1, "Unknown opcode %d\n", x);
            }
        }

        return Inst{
            .begin = begin,
            .end = ip,
            .end_block = end_block,
            .successors = successors,
            .str = dump.str()};
    }
};

template <typename F>
void traverse(bytefile *file, F process_block) {
    std::queue<char *> q;
    std::vector<bool> visited(64, false);

    q.push(file->code_ptr);
    visited[0] = true;

    InstReader reader(file);

    while (!q.empty()) {
        char *ip = q.front();
        q.pop();

        process_block(ip, reader);

        do {
            auto inst = reader.read_inst(ip);
            ip = inst.end;

            for (char *next : inst.successors) {
                size_t offset = next - file->code_ptr;
                if (offset >= visited.size()) {
                    visited.resize(visited.size() * 2, false);
                }
                if (!visited[offset]) {
                    visited[offset] = true;
                    q.push(next);
                }
            }

            if (inst.end_block)
                break;
        } while (true);
    }
}

struct Idiom {
    char *begin;
    char *end;
};

struct IdiomGroup {
    const Idiom *idiom;
    size_t count;
};

int compare_idioms(const Idiom &fst, const Idiom &snd) {
    for (size_t i = 0; fst.begin + i != fst.end && snd.begin + i != snd.end; i++) {
        if (fst.begin[i] != snd.begin[i])
            return (int)(fst.begin[i]) - (int)(snd.begin[i]);
    }
    return (int)(fst.end - fst.begin) - (int)(snd.end - snd.begin);
}

int main(int argc, char *argv[]) {
    bytefile *file = read_file(argv[1]);

    std::vector<Idiom> idioms;
    traverse(file, [&idioms](char *ip, InstReader &reader) {
        char *prev_ip = nullptr;
        std::string prev_dump;

        while (true) {
            auto inst = reader.read_inst(ip);

            if (prev_ip != nullptr) {
                idioms.push_back({prev_ip, inst.end});
            }
            idioms.push_back({inst.begin, inst.end});

            prev_ip = ip;
            ip = inst.end;

            if (inst.end_block)
                break;
        }
    });

    std::sort(idioms.begin(), idioms.end(), [](const Idiom &fst, const Idiom &snd) {
        return compare_idioms(fst, snd) < 0;
    });

    std::vector<IdiomGroup> idiom_groups;
    for (const Idiom &idiom : idioms) {
        if (idiom_groups.empty() || compare_idioms(idiom, *(idiom_groups.back().idiom)) != 0) {
            idiom_groups.push_back({&idiom, 1});
        } else {
            idiom_groups.back().count++;
        }
    }

    std::sort(idiom_groups.begin(), idiom_groups.end(), [](const IdiomGroup &fst, const IdiomGroup &snd) {
        return fst.count > snd.count;
    });

    InstReader reader(file);
    size_t index = 0;
    for (const auto &[idiom, count] : idiom_groups) {
        std::cout << "#" << ++index << ": " << count << " times";
        for (char *ip = idiom->begin; ip != idiom->end;) {
            auto inst = reader.read_inst(ip);
            ip = inst.end;
            std::cout << "\n\t" << inst.str;
        }
        std::cout << "\n";
    }
}
