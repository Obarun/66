/*
 * iso_scan.c
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

/* Exhaustive, mutation-proven tests for the table-driven ISO datetime DFA
 * log_iso_scan(). Every assertion checks an EFFECT (return, end offset, ts
 * fields), never the return alone. The process forces TZ=UTC0 so mktime is
 * leap-free and the decoded tv_sec equals the fixed UTC epochs asserted below;
 * that makes the field-mapping assertions non-tautological (a swapped month/day
 * mapping yields a different epoch, caught here). */

#include "ctest.h"

#include <time.h>
#include <stddef.h>

#include <66/log.h>

/* sentinels to prove "nothing written" on every return-0 path */
#define POISON_SEC ((time_t)0x5a5a5a5a)
#define POISON_NSEC 123456789L
#define POISON_END ((size_t)0xdead)

static int scan(char const *s, struct timespec *ts, size_t *end)
{
    return log_iso_scan(s, strlen(s), ts, end) ;
}

/* ---- accepting paths ---------------------------------------------------- */

static void test_date_only(void)
{
    struct timespec ts = { POISON_SEC, POISON_NSEC } ;
    size_t end = POISON_END ;
    T_ASSERT_EQ(1, scan("2026-06-29", &ts, &end), "valid date accepted") ;
    T_ASSERT_EQ(10, end, "date end == 10") ;
    T_ASSERT_EQ(1782691200, ts.tv_sec, "2026-06-29 00:00:00 UTC epoch") ;
    T_ASSERT_EQ(0, ts.tv_nsec, "date has zero nsec") ;
}

static void test_date_field_mapping(void)
{
    /* month != day so a mon<->day swap would change the epoch */
    struct timespec ts = { POISON_SEC, POISON_NSEC } ;
    size_t end = POISON_END ;
    T_ASSERT_EQ(1, scan("2025-03-07", &ts, &end), "date accepted") ;
    T_ASSERT_EQ(1741305600, ts.tv_sec, "2025-03-07 00:00:00 UTC epoch (mapping)") ;
    T_ASSERT_EQ(10, end, "end 10") ;
}

static void test_datetime_separators(void)
{
    char const *forms[] = { "2026-06-29 15:17:35", "2026-06-29T15:17:35", "2026-06-29t15:17:35" } ;
    for (size_t i = 0 ; i < 3 ; i++) {
        struct timespec ts = { POISON_SEC, POISON_NSEC } ;
        size_t end = POISON_END ;
        T_ASSERT_EQ(1, scan(forms[i], &ts, &end), "datetime accepted") ;
        T_ASSERT_EQ(19, end, "datetime end == 19") ;
        T_ASSERT_EQ(1782746255, ts.tv_sec, "2026-06-29 15:17:35 UTC epoch") ;
        T_ASSERT_EQ(0, ts.tv_nsec, "no fraction -> zero nsec") ;
    }
}

static void test_datetime_field_mapping(void)
{
    /* distinct H/MI/S to catch any field permutation */
    struct timespec ts = { POISON_SEC, POISON_NSEC } ;
    size_t end = POISON_END ;
    T_ASSERT_EQ(1, scan("2025-03-07 04:05:06", &ts, &end), "datetime accepted") ;
    T_ASSERT_EQ(1741320306, ts.tv_sec, "2025-03-07 04:05:06 UTC epoch (HMS mapping)") ;
    T_ASSERT_EQ(19, end, "end 19") ;
}

static void test_fraction_scaling(void)
{
    struct timespec ts ;
    size_t end ;

    T_ASSERT_EQ(1, scan("2025-03-07 04:05:06.5", &ts, &end), ".5 accepted") ;
    T_ASSERT_EQ(500000000, ts.tv_nsec, ".5 -> 5e8 ns") ;
    T_ASSERT_EQ(21, end, ".5 end") ;

    T_ASSERT_EQ(1, scan("2025-03-07 04:05:06.123456789", &ts, &end), "9-digit frac") ;
    T_ASSERT_EQ(123456789, ts.tv_nsec, "9 digits verbatim") ;
    T_ASSERT_EQ(29, end, "9-digit frac end") ;

    T_ASSERT_EQ(1, scan("2025-03-07 04:05:06.000000001", &ts, &end), "smallest frac") ;
    T_ASSERT_EQ(1, ts.tv_nsec, ".000000001 -> 1 ns") ;

    /* zero fraction: digits ARE consumed (end moves) but nsec is 0 */
    T_ASSERT_EQ(1, scan("2025-03-07 04:05:06.000", &ts, &end), ".000 accepted") ;
    T_ASSERT_EQ(0, ts.tv_nsec, ".000 -> 0 ns") ;
    T_ASSERT_EQ(23, end, ".000 consumes the three digits (end 23)") ;
    T_ASSERT_EQ(1741320306, ts.tv_sec, "sec unaffected by fraction") ;
}

static void test_fraction_over_nine_digits(void)
{
    /* digits beyond 9 are consumed (end advances) but ignored in the nsec value */
    struct timespec ts ;
    size_t end ;
    char const *s = "2025-03-07 04:05:06.123456789999" ;
    T_ASSERT_EQ(1, scan(s, &ts, &end), "over-9 frac accepted") ;
    T_ASSERT_EQ(123456789, ts.tv_nsec, "only first 9 frac digits used") ;
    T_ASSERT_EQ((size_t)strlen(s), end, "all frac digits consumed into end") ;
}

/* ---- greedy / longest-match -------------------------------------------- */

static void test_greedy_longest_match(void)
{
    struct timespec ts ;
    size_t end ;
    /* datetime+fraction is longer than datetime is longer than date */
    T_ASSERT_EQ(1, scan("2026-06-29T15:17:35.5", &ts, &end), "full match") ;
    T_ASSERT_EQ(21, end, "longest match reaches the fraction") ;
}

static void test_trailing_junk_after_datetime(void)
{
    struct timespec ts ;
    size_t end ;
    T_ASSERT_EQ(1, scan("2026-06-29T15:17:35junk", &ts, &end), "prefix matched") ;
    T_ASSERT_EQ(19, end, "end stops before junk, no silent trailing accept") ;
    T_ASSERT_EQ(1782746255, ts.tv_sec, "datetime decoded despite junk tail") ;
}

static void test_trailing_dot_not_consumed(void)
{
    struct timespec ts ;
    size_t end ;
    /* a dot with no following digit is not part of any accepting state */
    T_ASSERT_EQ(1, scan("2026-06-29T15:17:35.", &ts, &end), "dot-only tail ignored") ;
    T_ASSERT_EQ(19, end, "end stops at the second, dot not consumed") ;
    T_ASSERT_EQ(0, ts.tv_nsec, "no fraction digits -> 0 ns") ;
}

static void test_trailing_junk_after_date(void)
{
    struct timespec ts ;
    size_t end ;
    T_ASSERT_EQ(1, scan("2026-06-29junk", &ts, &end), "date prefix matched") ;
    T_ASSERT_EQ(10, end, "end stops after the date") ;
}

static void test_separator_without_time(void)
{
    struct timespec ts ;
    size_t end ;
    /* separator present but no time digits: falls back to the date accept */
    T_ASSERT_EQ(1, scan("2026-06-29T", &ts, &end), "trailing separator -> date") ;
    T_ASSERT_EQ(10, end, "end is the date, separator not part of a match") ;
}

/* ---- range validation (each boundary, both sides) ----------------------- */

static void test_range_month(void)
{
    struct timespec ts ; size_t end ;
    T_ASSERT_EQ(0, scan("2026-00-01", &ts, &end), "month 00 rejected") ;
    T_ASSERT_EQ(0, scan("2026-13-01", &ts, &end), "month 13 rejected") ;
    T_ASSERT_EQ(1, scan("2026-01-15", &ts, &end), "month 01 accepted") ;
    T_ASSERT_EQ(1, scan("2026-12-15", &ts, &end), "month 12 accepted") ;
}

static void test_range_day(void)
{
    struct timespec ts ; size_t end ;
    T_ASSERT_EQ(0, scan("2026-06-00", &ts, &end), "day 00 rejected") ;
    T_ASSERT_EQ(0, scan("2026-06-32", &ts, &end), "day 32 rejected") ;
    T_ASSERT_EQ(1, scan("2026-06-01", &ts, &end), "day 01 accepted") ;
    T_ASSERT_EQ(1, scan("2026-06-31", &ts, &end), "day 31 accepted (not calendar-checked)") ;
}

static void test_range_hour(void)
{
    struct timespec ts ; size_t end ;
    T_ASSERT_EQ(0, scan("2026-06-29T24:00:00", &ts, &end), "hour 24 rejected") ;
    T_ASSERT_EQ(1, scan("2026-06-29T23:00:00", &ts, &end), "hour 23 accepted") ;
    T_ASSERT_EQ(1, scan("2026-06-29T00:00:00", &ts, &end), "hour 00 accepted") ;
}

static void test_range_minute(void)
{
    struct timespec ts ; size_t end ;
    T_ASSERT_EQ(0, scan("2026-06-29T12:60:00", &ts, &end), "minute 60 rejected") ;
    T_ASSERT_EQ(1, scan("2026-06-29T12:59:00", &ts, &end), "minute 59 accepted") ;
}

static void test_range_second_leap(void)
{
    struct timespec ts ; size_t end ;
    T_ASSERT_EQ(0, scan("2026-06-29T12:00:61", &ts, &end), "second 61 rejected") ;
    T_ASSERT_EQ(1, scan("2026-06-29T12:00:60", &ts, &end), "second 60 accepted (leap second)") ;
    T_ASSERT_EQ(1, scan("2026-06-29T12:00:59", &ts, &end), "second 59 accepted") ;
}

/* ---- structural rejection ---------------------------------------------- */

static void test_reject_empty(void)
{
    struct timespec ts = { POISON_SEC, POISON_NSEC } ;
    size_t end = POISON_END ;
    T_ASSERT_EQ(0, log_iso_scan("", 0, &ts, &end), "empty rejected") ;
    T_ASSERT_EQ(POISON_SEC, ts.tv_sec, "empty: ts untouched") ;
    T_ASSERT_EQ(POISON_END, end, "empty: end untouched") ;
}

static void test_reject_garbage(void)
{
    struct timespec ts ; size_t end ;
    T_ASSERT_EQ(0, scan("not a date", &ts, &end), "leading non-digit rejected") ;
    T_ASSERT_EQ(0, scan("2026/06/29", &ts, &end), "wrong separator rejected") ;
    T_ASSERT_EQ(0, scan("2026-6-29", &ts, &end), "single-digit month rejected") ;
    T_ASSERT_EQ(0, scan("202-06-29", &ts, &end), "three-digit year rejected") ;
    T_ASSERT_EQ(0, scan("20260629", &ts, &end), "missing dashes rejected") ;
}

static void test_reject_does_not_write(void)
{
    /* a range failure must leave ts and end exactly as the caller left them */
    struct timespec ts = { POISON_SEC, POISON_NSEC } ;
    size_t end = POISON_END ;
    T_ASSERT_EQ(0, scan("2026-13-01", &ts, &end), "month 13 rejected") ;
    T_ASSERT_EQ(POISON_SEC, ts.tv_sec, "range-fail: tv_sec untouched") ;
    T_ASSERT_EQ(POISON_NSEC, ts.tv_nsec, "range-fail: tv_nsec untouched") ;
    T_ASSERT_EQ(POISON_END, end, "range-fail: end untouched") ;
}

static void test_partial_len(void)
{
    /* len cuts the buffer mid-token: only the bytes within len are scanned */
    struct timespec ts ; size_t end ;
    T_ASSERT_EQ(0, log_iso_scan("2026-06-29", 4, &ts, &end), "len 4 -> incomplete -> reject") ;
    T_ASSERT_EQ(1, log_iso_scan("2026-06-29T15:17:35", 10, &ts, &end), "len 10 -> date only") ;
    T_ASSERT_EQ(10, end, "len-bounded date end") ;
}

static void test_date_with_failed_time_attempt(void)
{
    /* a date followed by a separator and partial/invalid time digits: the DFA
     * greedily tries a datetime, fails, and falls back to the date accept
     * (got==1). The leftover H/MI/S digits from that failed attempt must NOT
     * contaminate the result -- value is local midnight, and their magnitude
     * must not flip the range check (the "8x accepted vs 88x rejected" wart). */
    struct timespec ts ; size_t end ;

    T_ASSERT_EQ(1, scan("2026-06-29 8x", &ts, &end), "date + 1 leftover digit accepted") ;
    T_ASSERT_EQ(10, end, "end is the date") ;
    T_ASSERT_EQ(1782691200, ts.tv_sec, "midnight, not 08:00 (no H contamination)") ;

    T_ASSERT_EQ(1, scan("2026-06-29 88x", &ts, &end), "date + out-of-range leftover still accepted") ;
    T_ASSERT_EQ(10, end, "end is the date (88 must not range-reject it)") ;
    T_ASSERT_EQ(1782691200, ts.tv_sec, "midnight despite leftover H=88") ;

    T_ASSERT_EQ(1, scan("2026-06-29 42 widgets", &ts, &end), "content line: date prefix") ;
    T_ASSERT_EQ(10, end, "end stops after the date") ;
    T_ASSERT_EQ(1782691200, ts.tv_sec, "midnight") ;
    T_ASSERT_EQ(0, ts.tv_nsec, "no nsec") ;
}

static void test_date_then_digit(void)
{
    /* S_DATE_OK on a DIGIT transitions to S_ERR: a 5th day digit does not extend
     * the date, the match stops at the accepted date (end 10) */
    struct timespec ts ; size_t end ;
    T_ASSERT_EQ(1, scan("2026-06-291", &ts, &end), "date accepted") ;
    T_ASSERT_EQ(10, end, "trailing digit not absorbed into the date") ;
    T_ASSERT_EQ(1782691200, ts.tv_sec, "2026-06-29 midnight") ;
}

static void test_datetime_then_digit(void)
{
    /* S_DT_OK on a DIGIT -> S_ERR: a 3rd seconds digit does not extend the time */
    struct timespec ts ; size_t end ;
    T_ASSERT_EQ(1, scan("2026-06-29T15:17:359", &ts, &end), "datetime accepted") ;
    T_ASSERT_EQ(19, end, "trailing digit not absorbed into the seconds") ;
    T_ASSERT_EQ(1782746255, ts.tv_sec, "15:17:35, not 15:17:359") ;
}

static void test_fraction_then_junk(void)
{
    /* S_FR on a non-digit -> S_ERR: the fraction stops, end is the last accepted
     * fraction digit, the trailing junk is not part of the match */
    struct timespec ts ; size_t end ;
    T_ASSERT_EQ(1, scan("2026-06-29T15:17:35.25zzz", &ts, &end), "fraction accepted") ;
    T_ASSERT_EQ(22, end, "end stops after the two fraction digits") ;
    T_ASSERT_EQ(250000000, ts.tv_nsec, ".25 -> 2.5e8 ns") ;
}

T_SUITE("log_iso_scan")
{
    setenv("TZ", "UTC0", 1) ;
    tzset() ;

    T_RUN(test_date_only) ;
    T_RUN(test_date_field_mapping) ;
    T_RUN(test_datetime_separators) ;
    T_RUN(test_datetime_field_mapping) ;
    T_RUN(test_fraction_scaling) ;
    T_RUN(test_fraction_over_nine_digits) ;
    T_RUN(test_greedy_longest_match) ;
    T_RUN(test_trailing_junk_after_datetime) ;
    T_RUN(test_trailing_dot_not_consumed) ;
    T_RUN(test_trailing_junk_after_date) ;
    T_RUN(test_separator_without_time) ;
    T_RUN(test_date_with_failed_time_attempt) ;
    T_RUN(test_date_then_digit) ;
    T_RUN(test_datetime_then_digit) ;
    T_RUN(test_fraction_then_junk) ;
    T_RUN(test_range_month) ;
    T_RUN(test_range_day) ;
    T_RUN(test_range_hour) ;
    T_RUN(test_range_minute) ;
    T_RUN(test_range_second_leap) ;
    T_RUN(test_reject_empty) ;
    T_RUN(test_reject_garbage) ;
    T_RUN(test_reject_does_not_write) ;
    T_RUN(test_partial_len) ;
}
