/*
 * emit.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

/* Mutation-proven, effect-based tests for log_emit(). log_emit writes to the
 * buffered ostream_1 (fd 1) and does NOT flush, so each test redirects fd 1 to a
 * scratch file, emits, flushes ostream_1 explicitly, restores fd 1, then asserts
 * the EXACT bytes produced. TZ is forced to UTC0 so the TAI64N->local reformat is
 * deterministic. The three timestamp kinds (TAI64N / ISO / NONE) are crossed with
 * withname in {0,1}, plus empty-message and stamp/tag-ordering edges. */

#include "ctest.h"

#include <sys/types.h>
#include <time.h>
#include <stdint.h>

#include <oblibs/stream.h>
#include <oblibs/clock.h>

#include <66/log.h>

static char capfile[256] ;
static int saved_fd1 = -1 ;

/* redirect fd 1 to a truncated scratch file; any bytes already buffered in
 * ostream_1 are drained to the OLD fd 1 first so they cannot leak into capture */
static void cap_begin(void)
{
    ostream_flush(ostream_1) ;
    saved_fd1 = dup(1) ;
    T_ASSERT(saved_fd1 >= 0, "dup fd 1") ;
    int fd = open(capfile, O_RDWR | O_CREAT | O_TRUNC, 0644) ;
    T_ASSERT(fd >= 0, "open capfile") ;
    T_ASSERT(dup2(fd, 1) >= 0, "dup2 capfile -> 1") ;
    close(fd) ;
}

/* flush emitted bytes into the capture (fd 1 still points at it), restore fd 1,
 * then read the captured bytes back into buf; returns the byte count */
static size_t cap_end(char *buf, size_t max)
{
    T_ASSERT(ostream_flush(ostream_1), "flush ostream_1") ;
    T_ASSERT(dup2(saved_fd1, 1) >= 0, "restore fd 1") ;
    close(saved_fd1) ;
    saved_fd1 = -1 ;
    int fd = open(capfile, O_RDONLY) ;
    T_ASSERT(fd >= 0, "reopen capfile for read") ;
    ssize_t n = 0, r ;
    while ((size_t)n < max - 1 && (r = read(fd, buf + n, max - 1 - n)) > 0)
        n += r ;
    T_ASSERT(r >= 0, "read capfile") ;
    close(fd) ;
    buf[n] = 0 ;
    return (size_t)n ;
}

#define T_BYTES(buf, n, expected) do { \
    size_t t_el_ = strlen(expected) ; \
    if ((n) != t_el_ || memcmp((buf), (expected), t_el_) != 0) { \
        fprintf(stderr, "FAIL: %s:%d: byte mismatch\n  expected [%zu]: <%s>\n  got      [%zu]: <%s>\n", \
                __func__, __LINE__, t_el_, (expected), (size_t)(n), (buf)) ; \
        exit(1) ; \
    } \
} while (0)

/* ---- NONE: no stamp, msgoff must be 0, tag (if any) sits at the head -------- */

static void test_none_no_name(void)
{
    char const *line = "plain message" ;
    char buf[256] ;
    cap_begin() ;
    log_emit(line, strlen(line), 0, LOG_STAMP_NONE, 0, "svc", 0) ;
    size_t n = cap_end(buf, sizeof buf) ;
    T_BYTES(buf, n, "plain message\n") ;
}

static void test_none_with_name(void)
{
    char const *line = "plain message" ;
    char buf[256] ;
    cap_begin() ;
    log_emit(line, strlen(line), 0, LOG_STAMP_NONE, 0, "svc", 1) ;
    size_t n = cap_end(buf, sizeof buf) ;
    T_BYTES(buf, n, "svc: plain message\n") ;
}

/* ---- ISO: leading stamp bytes kept verbatim, tag placed AFTER the stamp ----- */

static void test_iso_no_name(void)
{
    /* stamp is 29 chars + 1 space = msgoff 30 */
    char const *line = "2026-06-29 15:17:35.123456789 hello" ;
    char buf[256] ;
    cap_begin() ;
    log_emit(line, strlen(line), 30, LOG_STAMP_ISO, 0, "svc", 0) ;
    size_t n = cap_end(buf, sizeof buf) ;
    T_BYTES(buf, n, "2026-06-29 15:17:35.123456789 hello\n") ;
}

static void test_iso_with_name(void)
{
    /* the "svc: " tag must appear between the verbatim stamp and the message,
     * never before the timestamp (syslog-style placement) */
    char const *line = "2026-06-29 15:17:35.123456789 hello" ;
    char buf[256] ;
    cap_begin() ;
    log_emit(line, strlen(line), 30, LOG_STAMP_ISO, 0, "svc", 1) ;
    size_t n = cap_end(buf, sizeof buf) ;
    T_BYTES(buf, n, "2026-06-29 15:17:35.123456789 svc: hello\n") ;
}

/* ---- TAI64N: stamp is REPLACED by the local reformat of `stamp`, the raw
 *      line[0..msgoff) must NOT be emitted -------------------------------------*/

static void test_tai64n_no_name(void)
{
    struct timespec stamp = { .tv_sec = 1782746255, .tv_nsec = 123456789 } ;
    /* build a realistic raw line "<25-char tai64n> hello" to prove the raw stamp
     * prefix is dropped and the reformatted local time is used instead */
    char line[64] ;
    size_t sl = clock_tai64n_fmt(line, &stamp) ;
    line[sl] = ' ' ;
    memcpy(line + sl + 1, "hello", 5) ;
    size_t len = sl + 1 + 5, msgoff = sl + 1 ;

    char buf[256] ;
    cap_begin() ;
    log_emit(line, len, msgoff, LOG_STAMP_TAI64N, &stamp, "svc", 0) ;
    size_t n = cap_end(buf, sizeof buf) ;
    T_BYTES(buf, n, "2026-06-29 15:17:35.123456789 hello\n") ;
    T_ASSERT(memchr(buf, '@', n) == NULL, "raw tai64n prefix not emitted") ;
}

static void test_tai64n_with_name(void)
{
    struct timespec stamp = { .tv_sec = 1782746255, .tv_nsec = 123456789 } ;
    char line[64] ;
    size_t sl = clock_tai64n_fmt(line, &stamp) ;
    line[sl] = ' ' ;
    memcpy(line + sl + 1, "hello", 5) ;
    size_t len = sl + 1 + 5, msgoff = sl + 1 ;

    char buf[256] ;
    cap_begin() ;
    log_emit(line, len, msgoff, LOG_STAMP_TAI64N, &stamp, "svc", 1) ;
    size_t n = cap_end(buf, sizeof buf) ;
    T_BYTES(buf, n, "2026-06-29 15:17:35.123456789 svc: hello\n") ;
}

static void test_tai64n_stamp_value_used(void)
{
    /* a different stamp must produce a different local prefix: proves the value
     * is actually read from `stamp` (not a constant) */
    struct timespec stamp = { .tv_sec = 0, .tv_nsec = 0 } ;
    char buf[256] ;
    cap_begin() ;
    log_emit("x", 1, 0, LOG_STAMP_TAI64N, &stamp, "svc", 0) ;
    size_t n = cap_end(buf, sizeof buf) ;
    T_BYTES(buf, n, "1970-01-01 00:00:00.000000000 x\n") ;
}

/* ---- empty message ---------------------------------------------------------- */

static void test_none_empty_message(void)
{
    char buf[256] ;
    cap_begin() ;
    log_emit("", 0, 0, LOG_STAMP_NONE, 0, "svc", 0) ;
    size_t n = cap_end(buf, sizeof buf) ;
    T_BYTES(buf, n, "\n") ;
}

static void test_none_empty_message_with_name(void)
{
    char buf[256] ;
    cap_begin() ;
    log_emit("", 0, 0, LOG_STAMP_NONE, 0, "svc", 1) ;
    size_t n = cap_end(buf, sizeof buf) ;
    T_BYTES(buf, n, "svc: \n") ;
}

static void test_iso_empty_message(void)
{
    /* stamp present, message empty: verbatim stamp + tag + newline, no body */
    char const *line = "2026-06-29 " ;   /* 11 bytes, msgoff == len */
    char buf[256] ;
    cap_begin() ;
    log_emit(line, strlen(line), strlen(line), LOG_STAMP_ISO, 0, "svc", 1) ;
    size_t n = cap_end(buf, sizeof buf) ;
    T_BYTES(buf, n, "2026-06-29 svc: \n") ;
}

/* ---- message boundary: msgoff must slice exactly, no off-by-one ------------- */

static void test_iso_msgoff_slicing(void)
{
    /* embed distinctive bytes on both sides of msgoff; a +1/-1 slip changes the
     * captured bytes (the space/first message char would move across the tag) */
    char const *line = "2026-06-29 AB" ;   /* stamp "2026-06-29 " (11), msg "AB" */
    char buf[256] ;
    cap_begin() ;
    log_emit(line, strlen(line), 11, LOG_STAMP_ISO, 0, "svc", 1) ;
    size_t n = cap_end(buf, sizeof buf) ;
    T_BYTES(buf, n, "2026-06-29 svc: AB\n") ;
}

/* ---- binary-safe body: an embedded NUL must be written verbatim ------------- */

static void test_body_embedded_nul(void)
{
    char line[8] = { 'a', 0, 'b', 0 } ;   /* body "a\0b" of length 3 */
    char buf[256] ;
    cap_begin() ;
    log_emit(line, 3, 0, LOG_STAMP_NONE, 0, "svc", 0) ;
    size_t n = cap_end(buf, sizeof buf) ;
    T_ASSERT_EQ(4, n, "a<NUL>b<NL> is 4 bytes") ;
    T_ASSERT_EQ('a', buf[0], "byte0 a") ;
    T_ASSERT_EQ('\0', buf[1], "byte1 embedded NUL preserved") ;
    T_ASSERT_EQ('b', buf[2], "byte2 b") ;
    T_ASSERT_EQ('\n', buf[3], "byte3 newline") ;
}

T_SUITE("log_emit")
{
    setenv("TZ", "UTC0", 1) ;
    tzset() ;

    char tmpl[] = "/tmp/66emitXXXXXX" ;
    int fd = mkstemp(tmpl) ;
    T_ASSERT(fd >= 0, "mkstemp capfile") ;
    close(fd) ;
    memcpy(capfile, tmpl, sizeof tmpl) ;

    T_RUN(test_none_no_name) ;
    T_RUN(test_none_with_name) ;
    T_RUN(test_iso_no_name) ;
    T_RUN(test_iso_with_name) ;
    T_RUN(test_tai64n_no_name) ;
    T_RUN(test_tai64n_with_name) ;
    T_RUN(test_tai64n_stamp_value_used) ;
    T_RUN(test_none_empty_message) ;
    T_RUN(test_none_empty_message_with_name) ;
    T_RUN(test_iso_empty_message) ;
    T_RUN(test_iso_msgoff_slicing) ;
    T_RUN(test_body_embedded_nul) ;

    unlink(capfile) ;
}
