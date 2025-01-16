#include "bytefile.h"
#include "error.h"

void *__start_custom_data;
void *__stop_custom_data;

bytefile *read_file(const char *fname) {
    FILE *f = fopen(fname, "rb");
    long size;
    bytefile *file;

    if (f == 0) {
        FAIL(1, "%s\n", strerror(errno));
    }

    if (fseek(f, 0, SEEK_END) == -1) {
        FAIL(1, "%s\n", strerror(errno));
    }

    size_t total_size = sizeof(int) * 5 + (size = ftell(f));
    file = (bytefile *)malloc(total_size);
    file->size = total_size;

    if (file == 0) {
        FAIL(1, "*** FAILURE: unable to allocate memory.\n");
    }

    rewind(f);

    if (size != fread(&file->stringtab_size, 1, size, f)) {
        FAIL(1, "%s\n", strerror(errno));
    }

    fclose(f);

    ASSERT(file->stringtab_size >= 0, 1, "Negative string section size");
    ASSERT(file->public_symbols_number >= 0, 1, "Negative public symbols number");
    ASSERT(file->global_area_size >= 0, 1, "Negative global area size");

    int total_sections_size = file->stringtab_size + file->public_symbols_number * 2 * sizeof(int) + file->global_area_size * sizeof(int);

    ASSERT(total_sections_size + sizeof(bytefile) <= file->size,
           1, "Invalid sections layout");

    file->string_ptr = &file->buffer[file->public_symbols_number * 2 * sizeof(int)];
    file->public_ptr = (int *)file->buffer;
    file->code_ptr = &file->string_ptr[file->stringtab_size];
    file->global_ptr = (int *)malloc(file->global_area_size * sizeof(int));

    return file;
}

const char *get_string(const bytefile *f, int pos) {
    ASSERT(pos >= 0, 1,
           "Negative string index %d",
           pos);
    ASSERT(pos < f->stringtab_size, 1,
           "String #%d does not exist (%d strings in file)",
           pos, f->stringtab_size);
    return &f->string_ptr[pos];
}

const char *get_public_name(const bytefile *f, int i) {
    ASSERT(i >= 0, 1,
           "Negative public symbol index %d",
           i);
    ASSERT(i < f->public_symbols_number, 1,
           "Public symbol #%d does not exist (%d public symbols in file)",
           i, f->public_symbols_number);
    return get_string(f, f->public_ptr[i * 2]);
}

int get_public_offset(const bytefile *f, int i) {
    ASSERT(i >= 0, 1,
           "Negative public symbol index %d",
           i);
    ASSERT(i < f->public_symbols_number, 1,
           "Public symbol #%d does not exist (%d public symbols in file)",
           i, f->public_symbols_number);
    int offset = f->public_ptr[i * 2 + 1];
    ASSERT(offset < f->size, 1,
           "Invalid public offset: %d > %d (file size)",
           offset, f->size);
    return offset;
}

int get_code_size(const bytefile *f) {
    return f->size - (f->code_ptr - (const char *)f);
}

std::vector<const char *> get_entrypoints(const bytefile *file) {
    std::vector<const char *> public_symbol_offsets;
    for (int i = 0; i < file->public_symbols_number; i++) {
        // public_symbol_offsets.push_back(file->code_ptr + get_public_offset(file, i));
    }
    return public_symbol_offsets;
}
