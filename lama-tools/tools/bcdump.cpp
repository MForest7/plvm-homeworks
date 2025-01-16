#include "bytefile.h"
#include "functors/print_inst.h"
#include "inst_reader.h"

#include <iomanip>
#include <iostream>

int main(int argc, const char *argv[]) {
    const bytefile *file = read_file(argv[1]);

    InstReader reader(file);
    for (const char *ip = file->code_ptr; ip < file->code_ptr + get_code_size(file);) {
        std::cout << "0x" << std::setw(8) << std::hex << ip - file->code_ptr << "\t" << std::dec;
        ip = reader.read_inst<PrinterFunctor, std::ostream &>(ip, std::cout);
        std::cout << std::endl;
    }
}
