#include "bytefile.h"

#include <cstdlib>

#define FAIL(code, ...)                                                          \
    do {                                                                         \
        static_assert((code), "Expected non-zero integer literal");              \
        fprintf(stderr, "Failed at line %d with code %d\n\t", __LINE__, (code)); \
        fprintf(stderr, __VA_ARGS__);                                            \
        fprintf(stderr, "\n");                                                   \
        exit(code);                                                              \
    } while (0)

bytefile *read_file(char *fname) {
    FILE *f = fopen(fname, "rb");
    long size;
    bytefile *file;

    if (f == 0) {
        FAIL(255, "%s\n", strerror(errno));
    }

    if (fseek(f, 0, SEEK_END) == -1) {
        FAIL(255, "%s\n", strerror(errno));
    }

    file = (bytefile *)malloc(sizeof(int) * 4 + (size = ftell(f)));

    if (file == 0) {
        FAIL(255, "*** FAILURE: unable to allocate memory.\n");
    }

    rewind(f);

    if (size != fread(&file->stringtab_size, 1, size, f)) {
        FAIL(255, "%s\n", strerror(errno));
    }

    fclose(f);

    file->string_ptr = &file->buffer[file->public_symbols_number * 2 * sizeof(int)];
    file->public_ptr = (int *)file->buffer;
    file->code_ptr = &file->string_ptr[file->stringtab_size];
    file->global_ptr = (int *)malloc(file->global_area_size * sizeof(int));

    return file;
}

char *get_string(bytefile *f, int pos) {
    return &f->string_ptr[pos];
}

char *get_public_name(bytefile *f, int i) {
    return get_string(f, f->public_ptr[i * 2]);
}

int get_public_offset(bytefile *f, int i) {
    return f->public_ptr[i * 2 + 1];
}
