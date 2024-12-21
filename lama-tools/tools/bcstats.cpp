#include "bytefile.h"
#include "functors/default.h"
#include "functors/print_inst.h"
#include "functors/successors.h"
#include "inst_reader.h"

#include <algorithm>
#include <iostream>
#include <queue>
#include <string.h>
#include <vector>

inline std::vector<bool> mark_reachable_instructions(bytefile *file, char *entry) {
    std::queue<char *> q;
    q.push(entry);
    std::vector<bool> visited(get_code_size(file));
    visited[entry - file->code_ptr] = true;
    InstReader reader(file);

    while (!q.empty()) {
        char *ip = q.front();
        q.pop();

        std::vector<char *> successors;
        char *next = reader.read_inst<DefaultFunctor>(ip);
        reader.read_inst<SuccessorsFunctor, char *, char *, std::vector<char *> *>(ip, file->code_ptr, next, &successors);
        for (const char *s : successors) {
            if (!visited[s - file->code_ptr]) {
                visited[s - file->code_ptr] = true;
                q.push((char *)s);
            }
        }
    }

    return visited;
}

inline void mark_jumps(bytefile *file, char *entry, std::vector<bool> *jump, std::vector<bool> *label) {
    char *code_begin = file->code_ptr;
    char *code_end = file->code_ptr + get_code_size(file);

    jump->assign(code_end - code_begin, false);
    label->assign(code_end - code_begin, false);
    InstReader reader(file);

    for (char *ip = code_begin; ip != code_end;) {
        unsigned char opcode = *ip;
        switch (opcode) {
        case Opcode_Jmp:
        case Opcode_CJmpNZ:
        case Opcode_CJmpZ:
        case Opcode_Call:
        case Opcode_CallC:
        case Opcode_Fail:
        case COMPOSED(HOpcode_Stop, 0):
            jump->at(ip - code_begin) = true;
            break;
        default:
            break;
        }

        std::vector<char *> successors;
        char *next = reader.read_inst<DefaultFunctor>(ip);
        reader.read_inst<SuccessorsFunctor, char *, char *, std::vector<char *> *>(ip, file->code_ptr, next, &successors);
        for (const char *s : successors) {
            if (next != s) {
                label->at(s - code_begin) = true;
            }
        }
        ip = (char *)next;
    }
}

struct Idiom {
    char *begin;
    char *end;
};

int compare_idioms(const Idiom &fst, const Idiom &snd) {
    for (size_t i = 0; fst.begin + i != fst.end && snd.begin + i != snd.end; i++) {
        if (fst.begin[i] != snd.begin[i])
            return (int)(fst.begin[i]) - (int)(snd.begin[i]);
    }
    return (int)(fst.end - fst.begin) - (int)(snd.end - snd.begin);
}

struct ShortIdiomGroup {
    Idiom idiom;
    size_t count;
};

struct IdiomGroup {
    const Idiom *idiom;
    size_t count;
};

int main(int argc, char *argv[]) {
    bytefile *file = read_file(argv[1]);

    std::vector<ShortIdiomGroup> one_byte_idioms(1 << 8, {nullptr, 0});
    std::vector<ShortIdiomGroup> two_byte_idioms(1 << 16, {nullptr, 0});
    std::vector<Idiom> idioms;

    char *entry = entrypoint(file);
    if (entry == nullptr) {
        std::cerr << "Error: main not found" << std::endl;
        return 1;
    }

    std::vector<bool> reachable = mark_reachable_instructions(file, entry);
    std::vector<bool> jump, label;
    mark_jumps(file, entry, &jump, &label);

    InstReader reader(file);
    char *code_begin = file->code_ptr;
    char *code_end = file->code_ptr + get_code_size(file);

    for (char *ip = code_begin, *prev = nullptr; ip != code_end;) {
        if (!reachable[ip - code_begin]) {
            prev = nullptr;
            ip = reader.read_inst<DefaultFunctor>(ip);
            continue;
        }

        char *next = reader.read_inst<DefaultFunctor>(ip);
        if (prev != nullptr && !label[ip - code_begin]) {
            if (next - prev == 2) {
                int index = ((int)(*prev) << 8) | (int)(*ip);
                two_byte_idioms[index].idiom = {prev, next};
                two_byte_idioms[index].count++;
            } else {
                idioms.push_back(Idiom{prev, next});
            }
        }
        if (next - ip == 1) {
            int index = *(unsigned char *)ip;
            one_byte_idioms[index].idiom = {ip, next};
            one_byte_idioms[index].count++;
        } else {
            idioms.push_back(Idiom{ip, next});
        }

        prev = jump[ip - code_begin] ? nullptr : ip;
        ip = next;
    }

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

    for (const ShortIdiomGroup &sg : one_byte_idioms) {
        if (sg.idiom.begin != nullptr) {
            idiom_groups.push_back({&sg.idiom, sg.count});
        }
    }

    for (const ShortIdiomGroup &sg : two_byte_idioms) {
        if (sg.idiom.begin != nullptr) {
            idiom_groups.push_back({&sg.idiom, sg.count});
        }
    }

    std::sort(idiom_groups.begin(), idiom_groups.end(), [](const IdiomGroup &fst, const IdiomGroup &snd) {
        return fst.count > snd.count;
    });

    size_t index = 0;
    for (const auto &[idiom, count] : idiom_groups) {
        std::cout << "#" << ++index << ": " << count << " times";
        for (char *ip = idiom->begin; ip != idiom->end;) {
            std::cout << "\n\t";
            ip = reader.read_inst<PrinterFunctor, std::ostream &>(ip, std::cout);
        }
        std::cout << "\n";
    }
}
