/*
 * line_key.c
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

/* Mutation-proven tests for log_line_key(): the three classification branches
 * (TAI64N, ISO, NONE) plus the msgoff computation (stamp width + space skip).
 * Every assertion checks the decoded ts, the returned kind, AND the message
 * offset by dereferencing the byte it points at. */

#include "ctest.h"

#include <time.h>
#include <stddef.h>
#include <string.h>

#include <oblibs/clock.h>

#include <66/log.h>

#define POISON_SEC ((time_t)0x5a5a5a5a)
#define POISON_NSEC 424242L

static void test_none(void)
{
    struct timespec ts = { POISON_SEC, POISON_NSEC } ;
    size_t msgoff = 999 ;
    char const *l = "just a plain line" ;
    T_ASSERT_EQ(LOG_STAMP_NONE, log_line_key(l, strlen(l), &ts, &msgoff), "plain -> NONE") ;
    T_ASSERT_EQ(0, msgoff, "NONE msgoff is 0") ;
    /* contract: ts is untouched for NONE */
    T_ASSERT_EQ(POISON_SEC, ts.tv_sec, "NONE leaves ts.tv_sec untouched") ;
    T_ASSERT_EQ(POISON_NSEC, ts.tv_nsec, "NONE leaves ts.tv_nsec untouched") ;
}

static void test_iso(void)
{
    struct timespec ts ;
    size_t msgoff ;
    char const *l = "2026-06-29 12:34:56.123456789 hello" ;
    T_ASSERT_EQ(LOG_STAMP_ISO, log_line_key(l, strlen(l), &ts, &msgoff), "ISO detected") ;
    T_ASSERT_EQ(1782736496, ts.tv_sec, "ISO tv_sec (UTC epoch)") ;
    T_ASSERT_EQ(123456789, ts.tv_nsec, "ISO tv_nsec") ;
    T_ASSERT_EQ('h', l[msgoff], "msgoff points at the message") ;
    T_ASSERT_EQ(30, msgoff, "msgoff == stamp(29) + one space") ;
}

static void test_iso_multispace(void)
{
    struct timespec ts ;
    size_t msgoff ;
    /* the 66-log date directive emits two spaces; all spaces must be skipped */
    char const *l = "2026-06-29 12:34:56.123456789  hello" ;
    T_ASSERT_EQ(LOG_STAMP_ISO, log_line_key(l, strlen(l), &ts, &msgoff), "ISO detected") ;
    T_ASSERT_EQ('h', l[msgoff], "both spaces skipped, msgoff at message") ;
    T_ASSERT_EQ(31, msgoff, "msgoff skips both spaces") ;
}

static void test_iso_date_only(void)
{
    struct timespec ts ;
    size_t msgoff ;
    char const *l = "2026-06-29 plain text after a bare date" ;
    T_ASSERT_EQ(LOG_STAMP_ISO, log_line_key(l, strlen(l), &ts, &msgoff), "bare date -> ISO") ;
    T_ASSERT_EQ('p', l[msgoff], "msgoff after date + space") ;
    T_ASSERT_EQ(11, msgoff, "date(10) + one space") ;
}

static void test_tai64n_with_space(void)
{
    struct timespec stamp = { .tv_sec = 1782000000, .tv_nsec = 42 } ;
    char l[64] ;
    size_t n = clock_tai64n_fmt(l, &stamp) ;
    T_ASSERT_EQ(CLOCK_TAI64N_LEN, n, "fmt width") ;
    l[n] = ' ' ;
    memcpy(l + n + 1, "the message", 11) ;

    struct timespec ts ;
    size_t msgoff ;
    T_ASSERT_EQ(LOG_STAMP_TAI64N, log_line_key(l, n + 1 + 11, &ts, &msgoff), "TAI64N detected") ;
    T_ASSERT_EQ(stamp.tv_sec, ts.tv_sec, "TAI64N tv_sec") ;
    T_ASSERT_EQ(stamp.tv_nsec, ts.tv_nsec, "TAI64N tv_nsec") ;
    T_ASSERT_EQ(CLOCK_TAI64N_LEN + 1, msgoff, "msgoff skips the single space") ;
    T_ASSERT_EQ('t', l[msgoff], "msgoff at message") ;
}

static void test_tai64n_no_space(void)
{
    /* no space between stamp and message: msgoff must be exactly the stamp width */
    struct timespec stamp = { .tv_sec = 1782000000, .tv_nsec = 7 } ;
    char l[64] ;
    size_t n = clock_tai64n_fmt(l, &stamp) ;
    memcpy(l + n, "msg", 3) ;

    struct timespec ts ;
    size_t msgoff ;
    T_ASSERT_EQ(LOG_STAMP_TAI64N, log_line_key(l, n + 3, &ts, &msgoff), "TAI64N detected") ;
    T_ASSERT_EQ(CLOCK_TAI64N_LEN, msgoff, "no space -> msgoff is stamp width") ;
    T_ASSERT_EQ('m', l[msgoff], "msgoff at message, no skip") ;
}

static void test_tai64n_exact_len(void)
{
    /* line is exactly the stamp, nothing after */
    struct timespec stamp = { .tv_sec = 1782000000, .tv_nsec = 9 } ;
    char l[64] ;
    size_t n = clock_tai64n_fmt(l, &stamp) ;

    struct timespec ts ;
    size_t msgoff ;
    T_ASSERT_EQ(LOG_STAMP_TAI64N, log_line_key(l, n, &ts, &msgoff), "stamp-only line") ;
    T_ASSERT_EQ(stamp.tv_sec, ts.tv_sec, "tv_sec decoded") ;
    T_ASSERT_EQ(CLOCK_TAI64N_LEN, msgoff, "msgoff at end of line") ;
}

static void test_tai64n_too_short(void)
{
    /* leading '@' but fewer than CLOCK_TAI64N_LEN bytes: not a TAI64N stamp,
     * and not ISO either -> NONE */
    struct timespec ts ;
    size_t msgoff = 999 ;
    char const *l = "@4000" ;
    T_ASSERT_EQ(LOG_STAMP_NONE, log_line_key(l, strlen(l), &ts, &msgoff), "short @ -> NONE") ;
    T_ASSERT_EQ(0, msgoff, "NONE msgoff 0") ;
}

static void test_tai64n_short_no_overread(void)
{
    /* '@' + valid hex but FEWER than CLOCK_TAI64N_LEN bytes, placed at the very
     * end of an exact-size heap allocation. The `len >= CLOCK_TAI64N_LEN` guard
     * must short-circuit BEFORE clock_tai64n_scan reads its fixed 25 bytes;
     * dropping the guard makes that scan read past the buffer -> ASan catches it.
     * Without ASan the effect is still observable: result is NONE, msgoff 0. */
    size_t n = CLOCK_TAI64N_LEN - 1 ;          /* one byte short of a full stamp */
    char *buf = malloc(n) ;
    T_ASSERT(buf != NULL, "malloc short stamp") ;
    buf[0] = '@' ;
    memset(buf + 1, '0', n - 1) ;              /* all valid hex digits */

    struct timespec ts = { POISON_SEC, POISON_NSEC } ;
    size_t msgoff = 999 ;
    T_ASSERT_EQ(LOG_STAMP_NONE, log_line_key(buf, n, &ts, &msgoff),
                "sub-25 '@'+hex -> NONE (no over-read)") ;
    T_ASSERT_EQ(0, msgoff, "NONE msgoff 0") ;
    T_ASSERT_EQ(POISON_SEC, ts.tv_sec, "NONE leaves ts untouched") ;
    free(buf) ;
}

static void test_tai64n_invalid_hex(void)
{
    /* '@' + 24 non-hex bytes: width OK but clock_tai64n_scan rejects -> NONE */
    struct timespec ts ;
    size_t msgoff = 999 ;
    char l[CLOCK_TAI64N_LEN + 4] ;
    l[0] = '@' ;
    memset(l + 1, 'g', CLOCK_TAI64N_LEN - 1) ;
    memcpy(l + CLOCK_TAI64N_LEN, " hi", 3) ;
    T_ASSERT_EQ(LOG_STAMP_NONE, log_line_key(l, CLOCK_TAI64N_LEN + 3, &ts, &msgoff),
                "non-hex stamp -> NONE") ;
    T_ASSERT_EQ(0, msgoff, "NONE msgoff 0") ;
}

static void test_tai64n_requires_at_sign(void)
{
    /* a valid 25-char hex run that does NOT start with '@' is not TAI64N; with a
     * leading digit it also fails the ISO DFA (5th char is a digit, not '-') */
    struct timespec stamp = { .tv_sec = 1782000000, .tv_nsec = 1 } ;
    char l[64] ;
    size_t n = clock_tai64n_fmt(l, &stamp) ;
    l[0] = '4' ;                       /* clobber the '@' with a hex digit */
    memcpy(l + n, " x", 2) ;

    struct timespec ts ;
    size_t msgoff = 999 ;
    T_ASSERT_EQ(LOG_STAMP_NONE, log_line_key(l, n + 2, &ts, &msgoff),
                "missing '@' -> not TAI64N -> NONE") ;
    T_ASSERT_EQ(0, msgoff, "NONE msgoff 0") ;
}

T_SUITE("log_line_key")
{
    setenv("TZ", "UTC0", 1) ;
    tzset() ;

    T_RUN(test_none) ;
    T_RUN(test_iso) ;
    T_RUN(test_iso_multispace) ;
    T_RUN(test_iso_date_only) ;
    T_RUN(test_tai64n_with_space) ;
    T_RUN(test_tai64n_no_space) ;
    T_RUN(test_tai64n_exact_len) ;
    T_RUN(test_tai64n_too_short) ;
    T_RUN(test_tai64n_short_no_overread) ;
    T_RUN(test_tai64n_invalid_hex) ;
    T_RUN(test_tai64n_requires_at_sign) ;
}
