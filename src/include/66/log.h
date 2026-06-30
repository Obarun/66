/*
 * log.h
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

#ifndef SS_LOG_H
#define SS_LOG_H

#include <sys/types.h>
#include <time.h>
#include <stdint.h>

#include <oblibs/strbuf.h>

#include <66/config.h>

/** Kind of timestamp prefix found at the head of a log line. */
#define LOG_STAMP_NONE   0
#define LOG_STAMP_TAI64N 1
#define LOG_STAMP_ISO    2

typedef struct log_line_s log_line_t, *log_line_t_ref ;
struct log_line_s {
    struct timespec stamp ; // time key as Unix UTC epoch (inherited for unstamped lines)
    size_t off ;            // line start offset in the owning source buffer
    size_t len ;            // line length, trailing newline excluded
    size_t msgoff ;         // offset within the line to the message (past the stamp)
    uint8_t type ;          // LOG_STAMP_*
} ;

typedef struct log_source_s log_source_t, *log_source_t_ref ;
struct log_source_s {
    char name[SS_MAX_SERVICE_NAME + 1] ; // service name, or "system" (bounded, no alloc)
    strbuf data ;           // owned, concatenated raw content of every file
    log_line_t *line ;      // owned array of parsed lines
    size_t nline ;
    size_t cur ;            // merge cursor
    uint8_t stamped ;       // 1 if at least one line carried a stamp
} ;

#define LOG_SOURCE_ZERO { .data = STRBUF_ZERO, .line = 0, .nline = 0, .cur = 0, .stamped = 0 }

/**
 * @brief Parse a "YYYY-MM-DD[<sep>HH:MM:SS[.frac]]" local-time prefix (<sep> is a
 *        space, 'T' or 't') via a table-driven DFA.
 * @param[in] s    Bytes to scan.
 * @param[in] len  Length of @s.
 * @param[out] ts  Receives the decoded local time (Unix UTC epoch + nanoseconds).
 * @param[out] end Receives the offset just past the matched timestamp (longest match).
 * @return 1 if a valid date or datetime prefix was parsed (fields range-checked).
 * @return 0 otherwise (nothing written).
 */
extern int log_iso_scan(char const *s, size_t len, struct timespec *ts, size_t *end) ;

/**
 * @brief Detect and parse the leading timestamp of a log line.
 * @param[in] line   Line bytes (newline excluded), not NUL-terminated.
 * @param[in] len    Line length.
 * @param[out] ts    Receives the decoded time key (untouched for LOG_STAMP_NONE).
 * @param[out] msgoff Receives the offset to the message (past stamp+spaces); 0 for NONE.
 * @return LOG_STAMP_TAI64N, LOG_STAMP_ISO, or LOG_STAMP_NONE.
 */
extern int log_line_key(char const *line, size_t len, struct timespec *ts, size_t *msgoff) ;

/**
 * @brief Load a 66-log logdir into @src: concatenate the rotated archives
 *        (`@<tai64n>.{s,u}`, name-sorted ascending) then `current`, and parse the
 *        result into time-keyed lines. @name is copied (owned).
 * @return 1 on success.
 * @return 0 on system error (errno preserved).
 */
extern int log_source_logdir(log_source_t *src, char const *name, char const *logdir) ;

/**
 * @brief Load a single plain log file into @src and parse it. @name is copied.
 * @return 1 on success.
 * @return 0 on system error (errno preserved).
 */
extern int log_source_file(log_source_t *src, char const *name, char const *file) ;

/** @brief Release every buffer owned by @src and zero it. */
extern void log_source_free(log_source_t *src) ;

#endif
