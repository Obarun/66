/* ctest.h — canonical C test harness (test-forge). Dependency-free, C11.
   Include this FIRST in a test file: it sets the feature-test macros that make
   mkdtemp/fcntl visible under -std=c11. */
#ifndef CTEST_H
#define CTEST_H

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

static int t_pass_count = 0 ;

#define T_FAIL_(msg, expr) do { \
    fprintf(stderr, "FAIL: %s:%d: %s\n      (%s)\n", __func__, __LINE__, (msg), (expr)) ; \
    if (errno) fprintf(stderr, "      errno: %d (%s)\n", errno, strerror(errno)) ; \
    exit(1) ; \
} while (0)

/* Boolean assertion. */
#define T_ASSERT(cond, msg) do { if (!(cond)) T_FAIL_((msg), #cond) ; } while (0)

/* Integer equality with value printing (the two operands must be integral). */
#define T_ASSERT_EQ(expected, actual, msg) do { \
    long long t_e_ = (long long)(expected), t_a_ = (long long)(actual) ; \
    if (t_e_ != t_a_) { \
        fprintf(stderr, "FAIL: %s:%d: %s\n      expected %lld, got %lld\n", \
                __func__, __LINE__, (msg), t_e_, t_a_) ; \
        exit(1) ; \
    } \
} while (0)

/* errno must equal `want` right now. */
#define T_ASSERT_ERRNO(want, msg) do { \
    if (errno != (want)) { \
        fprintf(stderr, "FAIL: %s:%d: %s\n      expected errno %d (%s), got %d (%s)\n", \
                __func__, __LINE__, (msg), (want), strerror(want), errno, strerror(errno)) ; \
        exit(1) ; \
    } \
} while (0)

/* Run one test function and report it. */
#define T_RUN(fn) do { \
    fprintf(stderr, "  %-44s", #fn) ; \
    fn() ; \
    fprintf(stderr, "PASS\n") ; \
    t_pass_count++ ; \
} while (0)

/* Suite wrapper: write `T_SUITE("name") { T_RUN(test_a); T_RUN(test_b); }`. */
#define T_SUITE(name) \
    static void t_body_(void) ; \
    int main(void) { \
        signal(SIGPIPE, SIG_IGN) ;   /* writes to a dead peer report EPIPE, not death */ \
        fprintf(stderr, "=== %s ===\n", name) ; \
        t_body_() ; \
        fprintf(stderr, "OK: %d test(s) passed\n", t_pass_count) ; \
        return 0 ; \
    } \
    static void t_body_(void)

/* Disposable temp dir under the sandbox tmpfs; caller removes it. `template`
   must end in XXXXXX, e.g. "/tmp/ctest_XXXXXX". Returns NULL on failure. */
static inline char *t_tmpdir(char *template) { return mkdtemp(template) ; }

/* True iff `fd` is closed (a closed fd makes fcntl fail with EBADF). */
static inline int t_fd_closed(int fd) {
    return fcntl(fd, F_GETFD) == -1 && errno == EBADF ;
}

#endif
