#ifndef VERIFY_H
#define VERIFY_H

#include "bytefile.h"

void verify_reachable_instructions(const bytefile *file, const std::vector<const char *> &entrypoints);

#endif // VERIFY_H
