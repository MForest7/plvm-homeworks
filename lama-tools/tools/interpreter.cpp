#include "bytefile.h"
#include "error.h"
#include "interprete.h"
#include "verify.h"

#include <chrono>
#include <iostream>

namespace {

template <typename F>
auto measure_time(F procedure) {
    auto begin = std::chrono::steady_clock::now();
    procedure();
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(end - begin);
}

} // namespace

int main(int argc, const char *argv[]) {
    const char *file_name = argv[1];
    const bytefile *file = read_file(file_name);

    auto entrypoints = get_entrypoints(file);

    auto verification_time = measure_time([=]() {
        verify_reachable_instructions(file, entrypoints);
    });
    std::cerr << "Verification time: " << verification_time << std::endl;

    const char *ip = nullptr;
    for (int i = 0; i < file->public_symbols_number; i++) {
        if (strcmp(get_public_name(file, i), "main") == 0) {
            ip = file->code_ptr + get_public_offset(file, i);
            break;
        }
    }
    ASSERT(ip != nullptr, 1, "main symbol not found");

    auto execution_time = measure_time([=]() {
        interprete(file_name, file, ip);
    });
    std::cerr << "Execution time: " << execution_time << std::endl;
}
