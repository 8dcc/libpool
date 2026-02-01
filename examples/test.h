/*
 * Copyright 2026 8dcc
 *
 * This file is part of 8dcc's testing framework.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef TEST_H_
#define TEST_H_ 1

#include <stdbool.h>
#include <stdio.h>

static unsigned long test_num_run    = 0;
static unsigned long test_num_passed = 0;
static unsigned long test_num_failed = 0;
static bool test_cur_result          = true;
static const char* test_cur_name     = NULL;

/*----------------------------------------------------------------------------*/
/* Private macros */

#define TEST_PRINT_FAILED_ASSERT(COND_STR)                                     \
    do {                                                                       \
        printf("[FAIL] %s\n",                                                  \
               (test_cur_name != NULL) ? test_cur_name : "???");               \
        printf("    Assertion failed: %s\n", COND_STR);                        \
        printf("    At %s:%d\n", __FILE__, __LINE__);                          \
    } while (0)

#define TEST_PRETTY_ASSERT(COND, COND_STR)                                     \
    do {                                                                       \
        if (!(COND)) {                                                         \
            TEST_PRINT_FAILED_ASSERT(COND_STR);                                \
            test_cur_result = false;                                           \
            return;                                                            \
        }                                                                      \
    } while (0)

/*----------------------------------------------------------------------------*/
/* Public macros */

/* Expose macros to the user, "hiding" the internal globals */
#define TEST_NUM_RUN    test_num_run
#define TEST_NUM_PASSED test_num_passed
#define TEST_NUM_FAILED test_num_failed

/*
 * Declare a test with the specified name.
 */
#define TEST_DECL(NAME) static void test__##NAME(void)

/*
 * Run the previously-declared test of the specified name.
 */
#define TEST_RUN(NAME)                                                         \
    do {                                                                       \
        test_num_run++;                                                        \
        test_cur_result = true;                                                \
        test_cur_name   = #NAME;                                               \
        test__##NAME();                                                        \
        if (test_cur_result) {                                                 \
            printf("[PASS] %s\n", #NAME);                                      \
            test_num_passed++;                                                 \
        } else {                                                               \
            test_num_failed++;                                                 \
        }                                                                      \
        test_cur_name = NULL;                                                  \
    } while (0)

/*
 * Print the results of the currently-ran tests to the specified FILE pointer.
 */
#define TEST_PRINT_RESULTS(FILE_PTR)                                           \
    fprintf(FILE_PTR, "Tests run:    %ld\n", test_num_run);                    \
    fprintf(FILE_PTR,                                                          \
            "Tests passed: %ld (%ld%%)\n",                                     \
            test_num_passed,                                                   \
            test_num_passed * 100 / test_num_run);                             \
    fprintf(FILE_PTR,                                                          \
            "Tests failed: %ld (%ld%%)\n",                                     \
            test_num_failed,                                                   \
            test_num_failed * 100 / test_num_run);

/*
 * Assert that the specified condition is true (non-zero).
 */
#define TEST_ASSERT(COND) TEST_PRETTY_ASSERT(COND, #COND)

/*
 * Assert that the specified condition is true (non-zero). Wrapper for
 * 'TEST_ASSERT'.
 */
#define TEST_ASSERT_TRUE(COND) TEST_ASSERT(COND)

/*
 * Assert that the specified condition is false (zero).
 */
#define TEST_ASSERT_FALSE(COND)                                                \
    TEST_PRETTY_ASSERT((COND) == false, "(" #COND ") == false")

/*
 * Assert that A and B are equal.
 */
#define TEST_ASSERT_EQ(A, B) TEST_PRETTY_ASSERT((A) == (B), #A " == " #B)

/*
 * Assert that A and B are not equal.
 */
#define TEST_ASSERT_NE(A, B) TEST_PRETTY_ASSERT((A) != (B), #A " != " #B)

/*
 * Assert that the specified pointer is NULL.
 */
#define TEST_ASSERT_NULL(PTR) TEST_PRETTY_ASSERT((PTR) == NULL, #PTR " == NULL")

/*
 * Assert that the specified pointer not is NULL.
 */
#define TEST_ASSERT_NOT_NULL(PTR)                                              \
    TEST_PRETTY_ASSERT((PTR) != NULL, #PTR " != NULL")

#endif /* TEST_H_ */
