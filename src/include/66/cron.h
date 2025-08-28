/*
 * cron.h
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#ifndef CRON_EXPR_H
#define CRON_EXPR_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <oblibs/bits.h>

#define CRON_MIN_YEARS 1970
#define CRON_MAX_YEARS 2200
#define CRON_ZERO 0
#define CRON_MAX_SECONDS 60
#define CRON_MAX_MINUTES 60
#define CRON_MAX_HOURS 24
#define CRON_MAX_DOM 32
#define CRON_MAX_DOW 7
#define CRON_MAX_MONTHS 12
#define CRON_DAY_IN_SECONDS 24 * 60 * 60

#define CRON_BITSET_SECONDS 60
#define CRON_BITSET_MINUTES 60
#define CRON_BITSET_HOURS 24
#define CRON_BITSET_DOM 31
#define CRON_BITSET_MONTHS 12
#define CRON_BITSET_DOW 7
#define CRON_BITSET_YEARS 229

struct cron_s
{
    bitset8_t seconds ; // 8 bytes
    bitset8_t minutes ; // 8 bytes
    bitset8_t hours ; // 3 bytes
    bitset8_t dom ; // days of month 4 bytes
    bitset8_t months ; // 2 bytes
    bitset8_t dow ; // days of week 1 bytes
    bitset8_t years ; // 29 bytes

    // Special flags
    bool last ; // L
    bool wday ; // W
    int hash ; // e.g. 6#3 dow = 6, hash = 3 ;
    bool dom_dow ; // false using dom, true using dow

    char tz[256] ; // store timezone, 256 should be sufficient
} ;
typedef struct cron_s cron_t ;

#define CRON_EXPR_ZERO { BITSET8_ZERO, BITSET8_ZERO, BITSET8_ZERO, BITSET8_ZERO, BITSET8_ZERO, BITSET8_ZERO, BITSET8_ZERO, false, false, -1, false, {0} }

enum cron_type_e {
    CRON_TYPE_SECOND,
    CRON_TYPE_MINUTE,
    CRON_TYPE_HOUR,
    CRON_TYPE_DOM,
    CRON_TYPE_MONTH,
    CRON_TYPE_DOW,
    CRON_TYPE_YEAR,
    CRON_TYPE_OTHER,
    CRON_TYPE_ENDOFKEY
} ;
typedef enum cron_type_e cron_type_t ;

/**
 * @brief Parses a cron expression string into a structured format.
 *
 * This function takes a standard cron expression string and populates a
 * `cron_t` structure with bitsets representing the scheduled times
 * for seconds, minutes, hours, day of month, month, day of week, and year.
 * It supports standard cron fields, special characters (?, *, /, -, L, W, #),
 * and predefined macros (@yearly, @monthly, etc.).
 *
 * The expected format is:
 * `seconds minutes hours day_of_month month day_of_week [year]`
 * or a macro like `@daily`.
 *
 * @param expression
 * The null-terminated cron expression string to parse.
 * This can be a standard expression or a predefined macro.
 *
 * @param expr
 * Pointer to a `cron_t` structure to be populated
 * with the parsed results. The structure should be
 * initialized (e.g., using `CRON_EXPR_ZERO`) before calling.
 *
 * @param tz
 * Array of char specifying the timezone for
 * the calculation (e.g., "UTC", "Europe/London", "America/New_York").
 * If `NULL` or an empty string, the calculation is performed in UTC.
 *
 * @return
 * 1 on successful parsing, 0 on failure.
 *
 * @retval
 * 1 Parsing was successful. The `expr` structure is populated.
 * 0 Parsing failed due to:
 *      - Invalid input (`expression` or `expr` is NULL).
 *      - Memory allocation failure (`ENOMEM`).
 *      - Incorrect number of fields (not 5, 6, or 7) (`EINVAL`).
 *      - Syntax error in any field (`EINVAL`).
 *      - Invalid use of special characters (e.g., both DOM and DOW specified without '?' (`EINVAL`).
 *
 * @note
 * The function uses `errno` to report specific error codes.
 *
 * If the year field is omitted, it defaults to all years within
 * the range `[CRON_MIN_YEARS, CRON_MAX_YEARS]`.
 *
 * Predefined macros are expanded into their equivalent standard expressions
 * before parsing.
 *
 * The parser enforces that either the Day of Month (DOM) or Day of Week (DOW)
 * field (but not both, unless one is '?') specifies the day.
 */
int parse_cron(const char *expression, cron_t *expr, const char *tz) ;

/**
 * @brief Calculates the next execution time based on a parsed cron expression.
 *
 * This function determines the next `time_t` value that matches the schedule
 * defined by the `expr` cron expression, starting from the time specified by `now`.
 * It correctly handles timezones specified by the `tz` parameter.
 *
 * The function iterates through potential future times, checking each component
 * (year, month, day, hour, minute, second) against the constraints defined in
 * the `cron_t` structure. It efficiently skips large periods of time that
 * cannot match, such as months or years not included in the schedule.
 *
 * Special cron features like `?`, `L` (last), `W` (weekday), and `#` (nth weekday)
 * are handled through the `handle_day_condition` function.
 *
 * @param expr
 * Pointer to a `cron_t` structure that has been successfully
 * populated by `cron_parse_expr`.
 *
 * @param now
 * Pointer to a `time_t` value representing the current time. The
 * search for the next execution time starts *after* this moment.
 * If `NULL`, the behavior is undefined.
 *
 * @return
 * The `time_t` value of the next scheduled execution time.
 * (time_t)-1 An error occurred during calculation (e.g., timezone handling
 * failure, internal time conversion error, or no valid time found
 * within a 100-year search limit).
 *
 * @note
 * The function uses `log_warn` to report issues.
 *
 * The search is limited to 100 years into the future to prevent potential
 * infinite loops. If no matching time is found within this window,
 * `(time_t)-1` is returned.
 */
time_t cron_next(cron_t *expr, const time_t *now) ;

/**
 * @brief Sets a range of bits in a bitset.
 *
 * This function sets bits in the specified bitset (`field`) starting from
 * the `min` index (inclusive) up to, but not including, the `max` index (exclusive),
 * incrementing by `step`. This is a private function.
 *
 * @param field
 * Pointer to the `bitset8_t` structure to modify.
 *
 * @param min
 * The starting index (inclusive) of the range to set. Must be less than `max`.
 *
 * @param max
 * The ending index (exclusive) of the range to set. Must be greater than `min`.
 *
 * @param step
 * The increment between bit indices to set. If `step` is less than or equal to 0,
 * it is treated as 1. Must be positive to make progress if `min` < `max`.
 *
 * @note If `min` is greater than or equal to `max`, the loop condition `pos < max` will be
 *       false initially, and no bits will be set.
 * @note
 * If `step` is 0 or negative, it defaults to 1 to prevent an infinite loop or no progress.
 */
void bitset_range(bitset8_t *field, uint32_t min, uint32_t max, int step) ;

#endif
