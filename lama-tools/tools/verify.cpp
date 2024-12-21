#include "verify.h"
#include "bytefile.h"
#include "error.h"
#include "functors/default.h"
#include "functors/print_inst.h"
#include "functors/stack_depth.h"
#include "functors/successors.h"
#include "inst_reader.h"

#include <algorithm>
#include <iostream>
#include <queue>
#include <string.h>
#include <unordered_map>
#include <vector>

namespace {

struct ClosureEntry {
    char *offset;
    int captured;
};

/**
 * Set parameter `capture` to a non-negative integer to verify closure.
 * Returns minimal possible number of arguments in the virtual stack before the call.
 */
int verify_function(bytefile *file, char *begin, int captured = -1) {
    std::vector<int> jumps;
    StackLayout layout{
        .globals = file->global_area_size,
        .locals = 0,
        .args = 0,
        .captured = captured,
        .jumps = &jumps,
        .is_closure = (captured >= 0),
    };

    InstReader reader(file);

    int size = 2;
    for (char *ip = begin; *ip != Opcode_End;) {
        ip = reader.read_inst<DefaultFunctor>(ip);
        size = ip - begin + 1;
        if (*ip == Opcode_Begin || *ip == Opcode_CBegin) {
            FAIL(1, "Nested begin at offset 0x%.8x", ip - file->code_ptr);
        }
    }

    std::vector<bool> visited(size, false);
    visited[0] = true;

    std::queue<std::pair<char *, StackLayout>> q;
    q.push({begin, layout});

    while (!q.empty()) {
        char *ip = q.front().first;
        StackLayout layout = q.front().second;
        q.pop();

        char *next = reader.read_inst<StackDepthFunctor>(ip, &layout);

        if (layout.locals < 0) {
            FAIL(1, "Stack underflow at offset 0x%.8x", ip - file->code_ptr);
        }

        std::vector<char *> successors;
        reader.read_inst<SuccessorsFunctor>(ip, file->code_ptr, next, &successors);
        for (const char *s : successors) {
            if (begin <= s && s < begin + size) {
                if (!visited[s - begin]) {
                    visited[s - begin] = true;
                    q.push({(char *)s, layout});
                }
            }
        }
    }

    for (int jump : jumps) {
        char *target = file->code_ptr + jump;
        if (target < begin || target > begin + size) {
            FAIL(1, "Jump out of the function body (to 0x%.8x, function body is 0x%.8x..0x%.8x",
                 target - file->code_ptr, begin - file->code_ptr, begin + size - file->code_ptr);
        }
    }

    return layout.args;
}

template <unsigned char opcode, typename... Args>
struct GetCallOffset {
    int *offset;
    void operator()(Args...) {}
};

template <>
struct GetCallOffset<Opcode_Call, char *, int, int> {
    int *offset;
    void operator()(char *, int call_offset, int) {
        *offset = call_offset;
    }
};

void verify_calls(bytefile *file, char *begin, const std::unordered_map<int, int> &min_args_count) {
    std::vector<int> jumps;
    StackLayout layout{
        .globals = file->global_area_size,
        .locals = 0,
        .args = 0,
        .captured = 0,
        .jumps = &jumps,
        .is_closure = false,
    };

    InstReader reader(file);

    int size = 1;
    for (char *ip = begin; *ip != Opcode_End;) {
        ip = reader.read_inst<DefaultFunctor>(ip);
        size = ip - begin;
    }

    std::vector<bool> visited(size, false);
    visited[0] = true;

    std::queue<std::pair<char *, StackLayout>> q;
    q.push({begin, layout});

    while (!q.empty()) {
        char *ip = q.front().first;
        StackLayout layout = q.front().second;
        q.pop();

        if (*ip == Opcode_Call) {
            int offset;
            reader.read_inst<GetCallOffset>(ip, &offset);
            if (layout.locals < min_args_count.at((int)(file->code_ptr + offset))) {
                FAIL(1, "Stack underflow at offset 0x%.8x", (unsigned int)ip);
            }
        }

        char *next = reader.read_inst<StackDepthFunctor>(ip, &layout);
        std::vector<char *> successors;
        reader.read_inst<SuccessorsFunctor>(ip, file->code_ptr, next, &successors);
        for (const char *s : successors) {
            if (begin <= s && s < begin + size) {
                if (!visited[s - begin]) {
                    visited[s - begin] = true;
                    q.push({(char *)s, layout});
                }
            }
        }
    }
}

template <unsigned char opcode, typename... Args>
struct PushCallOffset {
    std::vector<char *> *begins;
    std::vector<ClosureEntry> *cbegins;
    char *code_ptr;

    void operator()(Args...) {}
};

template <>
struct PushCallOffset<Opcode_Call, char *, int, int> {
    std::vector<char *> *begins;
    std::vector<ClosureEntry> *cbegins;
    char *code_ptr;

    void operator()(char *, int offset, int args) {
        char *begin = code_ptr + offset;
        if (*begin != Opcode_Begin) {
            FAIL(1, "Call offset 0x%.8x points to opcode %d, %d expected",
                 (unsigned int)begin, *begin, Opcode_Begin);
        }
        begins->push_back(begin);
    }
};

template <>
struct PushCallOffset<Opcode_Closure, int, std::vector<LocationEntry>> {
    std::vector<char *> *begins;
    std::vector<ClosureEntry> *cbegins;
    char *code_ptr;

    void operator()(int offset, std::vector<LocationEntry> capture) {
        char *begin = code_ptr + offset;
        if (*begin != Opcode_CBegin && *begin != Opcode_Begin) {
            FAIL(1, "Closure offset 0x%.8x points to opcode %d, %d or %d expected",
                 (unsigned int)begin, *begin, Opcode_Begin, Opcode_CBegin);
        }
        cbegins->push_back({begin, (int)capture.size()});
    }
};

} // namespace

void verify_reachable_instructions(bytefile *file, char *entry) {
    std::queue<char *> q;
    q.push(entry);
    std::vector<bool> visited(get_code_size(file));
    visited[entry - file->code_ptr] = true;
    InstReader reader(file);

    std::vector<char *> begins;
    std::vector<ClosureEntry> cbegins;
    begins.push_back(entry);

    while (!q.empty()) {
        char *ip = q.front();
        q.pop();

        std::vector<char *> successors;
        char *next = reader.read_inst<PushCallOffset>(ip, &begins, &cbegins, file->code_ptr);
        reader.read_inst<SuccessorsFunctor>(ip, file->code_ptr, next, &successors);
        for (const char *s : successors) {
            if (!visited[s - file->code_ptr]) {
                visited[s - file->code_ptr] = true;
                q.push((char *)s);
            }
        }
    }

    std::unordered_map<int, int> min_args_count;
    for (auto begin : begins) {
        min_args_count[(int)begin] = verify_function(file, begin);
    }
    for (const ClosureEntry &cl : cbegins) {
        min_args_count[(int)cl.offset] = verify_function(file, cl.offset, cl.captured);
    }

    for (char *begin : begins) {
        verify_calls(file, begin, min_args_count);
    }
    for (const ClosureEntry &cl : cbegins) {
        verify_calls(file, cl.offset, min_args_count);
    }
}
