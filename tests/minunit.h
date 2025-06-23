#ifndef MINUNIT_H
#define MINUNIT_H

#include <stdio.h>
#include <string.h>

extern int tests_run;

#define mu_assert(message, test) do { \
    if (!(test)) return message; \
} while (0)

#define mu_run_test(test) do { \
    char *message = test(); \
    tests_run++; \
    if (message) return message; \
} while (0)

#define mu_test_suite_start() do { \
    tests_run = 0; \
    printf("Running test suite...\n"); \
} while (0)

#define mu_test_suite_end(result) do { \
    if (result != 0) { \
        printf("\nFAILED: %s\n", result); \
    } else { \
        printf("\nALL %d TESTS PASSED\n", tests_run); \
    } \
    printf("Tests run: %d\n", tests_run); \
    return result != 0; \
} while (0)

#endif