#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define FAIL(code, ...)                                                          \
    do {                                                                         \
        static_assert((code), "Expected non-zero integer literal");              \
        fprintf(stderr, "Failed at line %d with code %d\n\t", __LINE__, (code)); \
        fprintf(stderr, __VA_ARGS__);                                            \
        fprintf(stderr, "\n");                                                   \
        exit(code);                                                              \
    } while (0)

// #define DEBUG_MODE

#ifdef DEBUG_MODE
#define SAFE_MODE
#define CERR(...) fprintf(stderr, __VA_ARGS__);
#else
#define CERR(...)
#endif // DEBUG_MODE

#define SAFE_MODE

#ifdef SAFE_MODE
#define ASSERT(condition, code, ...)   \
    do {                               \
        if (!(condition))              \
            FAIL((code), __VA_ARGS__); \
    } while (0)
#else
#define ASSERT(condition, code, ...)
#endif // SAFE_MODE
