#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int _test_count = 0;
static int _test_pass = 0;
static int _test_fail = 0;

#define ASSERT(cond, msg) do { \
    _test_count++; \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL [%s:%d] %s\n", __FILE__, __LINE__, msg); \
        _test_fail++; \
    } else { \
        _test_pass++; \
    } \
} while (0)

#define ASSERT_EQ_INT(a, b, msg) do { \
    int _a = (a), _b = (b); \
    _test_count++; \
    if (_a != _b) { \
        fprintf(stderr, "  FAIL [%s:%d] %s: got %d, want %d\n", \
                __FILE__, __LINE__, msg, _a, _b); \
        _test_fail++; \
    } else { \
        _test_pass++; \
    } \
} while (0)

#define ASSERT_EQ_STR(a, b, msg) do { \
    const char *_a = (a), *_b = (b); \
    _test_count++; \
    if (strcmp(_a, _b) != 0) { \
        fprintf(stderr, "  FAIL [%s:%d] %s: got \"%s\", want \"%s\"\n", \
                __FILE__, __LINE__, msg, _a, _b); \
        _test_fail++; \
    } else { \
        _test_pass++; \
    } \
} while (0)

#define ASSERT_NEAR(a, b, tol, msg) do { \
    double _a = (a), _b = (b), _t = (tol); \
    _test_count++; \
    if (fabs(_a - _b) > _t) { \
        fprintf(stderr, "  FAIL [%s:%d] %s: got %f, want %f (tol %f)\n", \
                __FILE__, __LINE__, msg, _a, _b, _t); \
        _test_fail++; \
    } else { \
        _test_pass++; \
    } \
} while (0)

#define RUN_TEST(fn) do { \
    fprintf(stderr, "  %s... ", #fn); \
    fn(); \
    fprintf(stderr, "ok\n"); \
} while (0)

#define TEST_SUMMARY() do { \
    fprintf(stderr, "\n  %d tests, %d passed, %d failed\n", \
            _test_count, _test_pass, _test_fail); \
    return _test_fail > 0 ? 1 : 0; \
} while (0)

#endif
