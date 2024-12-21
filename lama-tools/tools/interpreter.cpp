#include "bytefile.h"
#include "error.h"
#include "interprete.h"
#include "verify.h"

#include <chrono>
#include <iostream>

template <typename F>
auto measure_time(F procedure) {
    auto begin = std::chrono::steady_clock::now();
    procedure();
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(end - begin);
}

int main(int argc, char *argv[]) {
    char *file_name = argv[1];
    bytefile *file = read_file(file_name);

    char *ip = entrypoint(file);
    ASSERT(ip != NULL, 1, "main symbol not found");

    auto verification_time = measure_time([=]() {
        verify_reachable_instructions(file, ip);
    });
    std::cerr << "Verification time: " << verification_time << std::endl;

    auto interpretation_time = measure_time([=]() {
        interprete(file_name, file, ip);
    });
    std::cerr << "Interpretation time: " << interpretation_time << std::endl;
}
