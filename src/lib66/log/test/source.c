/*
 * source.c
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

/* Mutation-proven tests for log_source_file / log_source_logdir / log_source_free.
 * Scratch lives under /tmp (a fresh tmpfs inside the 66-ns sandbox). Every test
 * checks the parsed line array by EFFECT: counts, per-line off/len/msgoff/type,
 * the stamp inheritance for unstamped lines, archive-before-current ordering,
 * the inter-file newline guard, and the missing/empty short-circuits. */

#include "ctest.h"

#include <time.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#include <66/log.h>

static char gdir[256] ;

static void write_file(char const *path, void const *buf, size_t len)
{
    FILE *f = fopen(path, "wb") ;
    T_ASSERT(f != NULL, "fopen scratch file") ;
    T_ASSERT_EQ(len, fwrite(buf, 1, len, f), "fwrite full content") ;
    T_ASSERT_EQ(0, fclose(f), "fclose") ;
}

static char const *line_ptr(log_source_t *s, size_t i)
{
    return s->data.s + s->line[i].off ;
}

static char const *msg_ptr(log_source_t *s, size_t i)
{
    return s->data.s + s->line[i].off + s->line[i].msgoff ;
}

/* ---- log_source_file ---------------------------------------------------- */

static void test_file_two_lines_no_tail_newline(void)
{
    char path[300] ;
    snprintf(path, sizeof path, "%s/file1", gdir) ;
    char const *a = "2026-06-29 10:00:00.0 first" ;
    char const *b = "2026-06-29 10:00:01.0 second" ;       /* no trailing newline */
    char buf[128] ;
    size_t la = strlen(a), lb = strlen(b) ;
    memcpy(buf, a, la) ; buf[la] = '\n' ; memcpy(buf + la + 1, b, lb) ;
    write_file(path, buf, la + 1 + lb) ;

    log_source_t s = LOG_SOURCE_ZERO ;
    T_ASSERT_EQ(1, log_source_file(&s, "svc", path), "file load ok") ;
    T_ASSERT_EQ(2, s.nline, "two lines (incl. no-newline tail)") ;
    T_ASSERT_EQ(1, s.stamped, "at least one stamp seen") ;

    T_ASSERT_EQ(0, s.line[0].off, "line0 offset 0") ;
    T_ASSERT_EQ(la, s.line[0].len, "line0 length excludes newline") ;
    T_ASSERT_EQ(LOG_STAMP_ISO, s.line[0].type, "line0 ISO") ;
    T_ASSERT_EQ(0, strncmp(msg_ptr(&s, 0), "first", 5), "line0 message 'first'") ;

    T_ASSERT_EQ(la + 1, s.line[1].off, "line1 offset past newline") ;
    T_ASSERT_EQ(lb, s.line[1].len, "line1 length is full tail") ;
    T_ASSERT_EQ(0, strncmp(msg_ptr(&s, 1), "second", 6), "line1 message 'second'") ;
    T_ASSERT(s.line[1].stamp.tv_sec > s.line[0].stamp.tv_sec, "line1 stamp later than line0") ;

    log_source_free(&s) ;
    unlink(path) ;
}

static void test_file_missing_is_success(void)
{
    char path[300] ;
    snprintf(path, sizeof path, "%s/does_not_exist", gdir) ;
    log_source_t s = LOG_SOURCE_ZERO ;
    T_ASSERT_EQ(1, log_source_file(&s, "svc", path), "missing file -> success") ;
    T_ASSERT_EQ(0, s.nline, "missing file -> zero lines") ;
    T_ASSERT_EQ(0, s.stamped, "missing file -> not stamped") ;
    log_source_free(&s) ;
}

static void test_file_empty(void)
{
    char path[300] ;
    snprintf(path, sizeof path, "%s/empty", gdir) ;
    write_file(path, "", 0) ;
    log_source_t s = LOG_SOURCE_ZERO ;
    T_ASSERT_EQ(1, log_source_file(&s, "svc", path), "empty file -> success") ;
    T_ASSERT_EQ(0, s.nline, "empty file -> zero lines") ;
    log_source_free(&s) ;
    unlink(path) ;
}

static void test_file_line_count_variants(void)
{
    char path[300] ;
    struct { char const *content ; size_t len ; size_t want ; char const *msg ; } v[] = {
        { "a\nb\n", 4, 2, "two terminated lines" },
        { "a\nb",   3, 2, "second line lacks newline" },
        { "a",      1, 1, "single line no newline" },
        { "\n",     1, 1, "one empty terminated line" },
    } ;
    for (size_t i = 0 ; i < sizeof(v)/sizeof(*v) ; i++) {
        snprintf(path, sizeof path, "%s/count%zu", gdir, i) ;
        write_file(path, v[i].content, v[i].len) ;
        log_source_t s = LOG_SOURCE_ZERO ;
        T_ASSERT_EQ(1, log_source_file(&s, "svc", path), "load ok") ;
        T_ASSERT_EQ(v[i].want, s.nline, v[i].msg) ;
        log_source_free(&s) ;
        unlink(path) ;
    }
}

static void test_file_stamp_inheritance(void)
{
    char path[300] ;
    snprintf(path, sizeof path, "%s/inherit", gdir) ;
    char const *content =
        "plain leading line\n"                       /* line0: NONE, before any stamp */
        "2026-06-29 10:00:00.0 anchor\n"             /* line1: ISO */
        "continuation without stamp\n" ;             /* line2: NONE, inherits line1 */
    write_file(path, content, strlen(content)) ;

    log_source_t s = LOG_SOURCE_ZERO ;
    T_ASSERT_EQ(1, log_source_file(&s, "svc", path), "load ok") ;
    T_ASSERT_EQ(3, s.nline, "three lines") ;

    T_ASSERT_EQ(LOG_STAMP_NONE, s.line[0].type, "line0 unstamped") ;
    T_ASSERT_EQ(0, s.line[0].stamp.tv_sec, "line0 has no inheritable key -> 0 sec") ;
    T_ASSERT_EQ(0, s.line[0].stamp.tv_nsec, "line0 -> 0 nsec") ;

    T_ASSERT_EQ(LOG_STAMP_ISO, s.line[1].type, "line1 stamped") ;

    T_ASSERT_EQ(LOG_STAMP_NONE, s.line[2].type, "line2 unstamped") ;
    T_ASSERT_EQ(s.line[1].stamp.tv_sec, s.line[2].stamp.tv_sec, "line2 inherits sec") ;
    T_ASSERT_EQ(s.line[1].stamp.tv_nsec, s.line[2].stamp.tv_nsec, "line2 inherits nsec") ;

    log_source_free(&s) ;
    unlink(path) ;
}

/* ---- log_source_logdir -------------------------------------------------- */

static void test_logdir_missing_is_success(void)
{
    char path[300] ;
    snprintf(path, sizeof path, "%s/nodir", gdir) ;
    log_source_t s = LOG_SOURCE_ZERO ;
    T_ASSERT_EQ(1, log_source_logdir(&s, "svc", path), "missing dir -> success (ENOENT)") ;
    T_ASSERT_EQ(0, s.nline, "missing dir -> zero lines") ;
    log_source_free(&s) ;
}

static void test_logdir_empty(void)
{
    char path[300] ;
    snprintf(path, sizeof path, "%s/emptydir", gdir) ;
    T_ASSERT_EQ(0, mkdir(path, 0755), "mkdir empty logdir") ;
    log_source_t s = LOG_SOURCE_ZERO ;
    T_ASSERT_EQ(1, log_source_logdir(&s, "svc", path), "empty dir -> success") ;
    T_ASSERT_EQ(0, s.nline, "empty dir -> zero lines") ;
    log_source_free(&s) ;
    rmdir(path) ;
}

static void test_logdir_current_only(void)
{
    char dir[300], path[400] ;
    snprintf(dir, sizeof dir, "%s/ld_cur", gdir) ;
    T_ASSERT_EQ(0, mkdir(dir, 0755), "mkdir") ;
    snprintf(path, sizeof path, "%s/current", dir) ;
    write_file(path, "2026-06-29 11:00:00.0 only\n", 27) ;

    log_source_t s = LOG_SOURCE_ZERO ;
    T_ASSERT_EQ(1, log_source_logdir(&s, "svc", dir), "current-only load ok") ;
    T_ASSERT_EQ(1, s.nline, "one line from current") ;
    T_ASSERT_EQ(0, strncmp(msg_ptr(&s, 0), "only", 4), "current line content") ;
    log_source_free(&s) ;

    unlink(path) ; rmdir(dir) ;
}

static void test_logdir_archive_before_current(void)
{
    char dir[300], a[400], c[400] ;
    snprintf(dir, sizeof dir, "%s/ld_ord", gdir) ;
    T_ASSERT_EQ(0, mkdir(dir, 0755), "mkdir") ;
    snprintf(a, sizeof a, "%s/@400000006a0000000000000a.s", dir) ;
    snprintf(c, sizeof c, "%s/current", dir) ;
    write_file(a, "2026-06-29 09:00:00.0 archived\n", 31) ;
    write_file(c, "2026-06-29 12:00:00.0 live\n", 27) ;

    log_source_t s = LOG_SOURCE_ZERO ;
    T_ASSERT_EQ(1, log_source_logdir(&s, "svc", dir), "load ok") ;
    T_ASSERT_EQ(2, s.nline, "archive + current = two lines") ;
    T_ASSERT_EQ(0, strncmp(msg_ptr(&s, 0), "archived", 8), "archive line is first") ;
    T_ASSERT_EQ(0, strncmp(msg_ptr(&s, 1), "live", 4), "current line is last") ;
    log_source_free(&s) ;

    unlink(a) ; unlink(c) ; rmdir(dir) ;
}

static void test_logdir_archives_sorted(void)
{
    char dir[300], a1[400], a2[400] ;
    snprintf(dir, sizeof dir, "%s/ld_sort", gdir) ;
    T_ASSERT_EQ(0, mkdir(dir, 0755), "mkdir") ;
    /* lexically a1 < a2; their content must appear in that order regardless of
     * the directory's physical entry order */
    snprintf(a2, sizeof a2, "%s/@400000006a0000000000001400.s", dir) ;
    write_file(a2, "2026-06-29 09:00:02.0 younger\n", 30) ;
    snprintf(a1, sizeof a1, "%s/@400000006a0000000000000a00.s", dir) ;
    write_file(a1, "2026-06-29 09:00:01.0 older\n", 28) ;

    log_source_t s = LOG_SOURCE_ZERO ;
    T_ASSERT_EQ(1, log_source_logdir(&s, "svc", dir), "load ok") ;
    T_ASSERT_EQ(2, s.nline, "two archive lines") ;
    T_ASSERT_EQ(0, strncmp(msg_ptr(&s, 0), "older", 5), "lexically-smaller archive first") ;
    T_ASSERT_EQ(0, strncmp(msg_ptr(&s, 1), "younger", 7), "lexically-larger archive second") ;
    log_source_free(&s) ;

    unlink(a1) ; unlink(a2) ; rmdir(dir) ;
}

static void test_logdir_newline_guard(void)
{
    char dir[300], a[400], c[400] ;
    snprintf(dir, sizeof dir, "%s/ld_guard", gdir) ;
    T_ASSERT_EQ(0, mkdir(dir, 0755), "mkdir") ;
    /* archive does NOT end in newline; without the inter-file guard its last line
     * would glue onto the first line of current under a single '\n' split */
    snprintf(a, sizeof a, "%s/@400000006a0000000000000a.s", dir) ;
    snprintf(c, sizeof c, "%s/current", dir) ;
    write_file(a, "ARCHTAIL", 8) ;                 /* no newline */
    write_file(c, "CURHEAD\n", 8) ;

    log_source_t s = LOG_SOURCE_ZERO ;
    T_ASSERT_EQ(1, log_source_logdir(&s, "svc", dir), "load ok") ;
    T_ASSERT_EQ(2, s.nline, "guard keeps the two lines separate") ;
    T_ASSERT_EQ(8, s.line[0].len, "line0 is exactly 'ARCHTAIL'") ;
    T_ASSERT_EQ(0, strncmp(line_ptr(&s, 0), "ARCHTAIL", 8), "line0 not merged") ;
    T_ASSERT_EQ(0, strncmp(line_ptr(&s, 1), "CURHEAD", 7), "line1 is current head") ;
    log_source_free(&s) ;

    unlink(a) ; unlink(c) ; rmdir(dir) ;
}

static void test_file_embedded_nul(void)
{
    /* memchr-based splitting is binary-safe: a NUL inside a line must not end it,
     * and the line length/offsets must span the NUL verbatim. */
    char path[300] ;
    snprintf(path, sizeof path, "%s/binnul", gdir) ;
    /* line0 (ISO) carries an embedded NUL in its message; line1 follows it */
    char const head[] = "2026-06-29 10:00:00.0 a" ;        /* 23 bytes, no NUL */
    char const tail[] = "b" ;
    char const second[] = "2026-06-29 10:00:01.0 next" ;
    char buf[128] ;
    size_t p = 0 ;
    memcpy(buf + p, head, sizeof head - 1) ; p += sizeof head - 1 ;
    buf[p++] = '\0' ;                                       /* embedded NUL */
    memcpy(buf + p, tail, sizeof tail - 1) ; p += sizeof tail - 1 ;
    buf[p++] = '\n' ;
    size_t line0len = p - 1 ;                               /* excludes the '\n' */
    memcpy(buf + p, second, sizeof second - 1) ; p += sizeof second - 1 ;
    buf[p++] = '\n' ;
    write_file(path, buf, p) ;

    log_source_t s = LOG_SOURCE_ZERO ;
    T_ASSERT_EQ(1, log_source_file(&s, "svc", path), "load ok") ;
    T_ASSERT_EQ(2, s.nline, "NUL does not split: two lines") ;
    T_ASSERT_EQ(line0len, s.line[0].len, "line0 length spans the embedded NUL") ;
    T_ASSERT_EQ(LOG_STAMP_ISO, s.line[0].type, "line0 still ISO-stamped past NUL") ;
    /* the byte right after the message 'a' is the NUL, proving it was kept inline */
    T_ASSERT_EQ('a', line_ptr(&s, 0)[22], "byte 22 is the message char 'a'") ;
    T_ASSERT_EQ('\0', line_ptr(&s, 0)[23], "byte at offset 23 is the embedded NUL") ;
    T_ASSERT_EQ('b', line_ptr(&s, 0)[24], "byte after the NUL is still in line0") ;
    T_ASSERT_EQ(0, strncmp(msg_ptr(&s, 1), "next", 4), "line1 parsed after the NUL line") ;

    log_source_free(&s) ;
    unlink(path) ;
}

/* ---- name length guard (bounded inline buffer, no overflow) -------------- */

static void fill_name(char *dst, size_t n)
{
    memset(dst, 'x', n) ;
    dst[n] = 0 ;
}

static void test_name_too_long_file(void)
{
    /* strlen(name) > SS_MAX_SERVICE_NAME must be rejected with EINVAL BEFORE any
     * copy into the fixed name[SS_MAX_SERVICE_NAME+1] buffer (overflow guard). */
    char path[300] ;
    snprintf(path, sizeof path, "%s/forfree", gdir) ;     /* path is irrelevant */
    char name[SS_MAX_SERVICE_NAME + 2] ;
    fill_name(name, SS_MAX_SERVICE_NAME + 1) ;             /* one over the limit */

    log_source_t s = LOG_SOURCE_ZERO ;
    errno = 0 ;
    T_ASSERT_EQ(0, log_source_file(&s, name, path), "over-long name rejected") ;
    T_ASSERT_ERRNO(EINVAL, "over-long name -> EINVAL") ;
    T_ASSERT_EQ(0, s.nline, "rejected before any parsing") ;
    T_ASSERT(s.line == NULL, "rejected before any allocation") ;
    log_source_free(&s) ;
}

static void test_name_too_long_logdir(void)
{
    char name[SS_MAX_SERVICE_NAME + 2] ;
    fill_name(name, SS_MAX_SERVICE_NAME + 1) ;

    log_source_t s = LOG_SOURCE_ZERO ;
    errno = 0 ;
    T_ASSERT_EQ(0, log_source_logdir(&s, name, gdir), "over-long name rejected (logdir)") ;
    T_ASSERT_ERRNO(EINVAL, "over-long name -> EINVAL (logdir)") ;
    T_ASSERT_EQ(0, s.nline, "rejected before any parsing") ;
    log_source_free(&s) ;
}

static void test_name_exact_length(void)
{
    /* a name of exactly SS_MAX_SERVICE_NAME chars is accepted and copied in full
     * (the guard is '>' not '>='); the inline buffer holds MAX chars + the NUL. */
    char path[300] ;
    snprintf(path, sizeof path, "%s/exactname", gdir) ;
    write_file(path, "2026-06-29 10:00:00.0 x\n", 24) ;
    char name[SS_MAX_SERVICE_NAME + 1] ;
    fill_name(name, SS_MAX_SERVICE_NAME) ;                 /* exactly the limit */

    log_source_t s = LOG_SOURCE_ZERO ;
    T_ASSERT_EQ(1, log_source_file(&s, name, path), "exact-length name accepted") ;
    T_ASSERT_EQ(SS_MAX_SERVICE_NAME, strlen(s.name), "name copied at full length") ;
    T_ASSERT_EQ(0, strcmp(s.name, name), "name copied verbatim, NUL-terminated") ;
    T_ASSERT_EQ(1, s.nline, "content still parsed") ;
    log_source_free(&s) ;
    unlink(path) ;
}

/* ---- log_source_free ---------------------------------------------------- */

static void test_free_zeroes(void)
{
    /* name is a bounded inline buffer (no heap, never freed); log_source_free
     * releases the dynamic buffers (data, line) and zeroes the line bookkeeping */
    char path[300] ;
    snprintf(path, sizeof path, "%s/forfree", gdir) ;
    write_file(path, "2026-06-29 10:00:00.0 x\n", 24) ;
    log_source_t s = LOG_SOURCE_ZERO ;
    T_ASSERT_EQ(1, log_source_file(&s, "myname", path), "load ok") ;
    T_ASSERT_EQ(0, strcmp(s.name, "myname"), "name copied into the source buffer") ;
    T_ASSERT(s.line != NULL, "line array allocated") ;
    T_ASSERT_EQ(1, s.nline, "one line parsed before free") ;

    log_source_free(&s) ;
    T_ASSERT(s.line == NULL, "free releases and zeroes line") ;
    T_ASSERT_EQ(0, s.nline, "free zeroes nline") ;
    T_ASSERT_EQ(0, s.cur, "free zeroes cur") ;
    unlink(path) ;
}

T_SUITE("log_source")
{
    setenv("TZ", "UTC0", 1) ;
    tzset() ;

    char tmpl[] = "/tmp/66logsrcXXXXXX" ;
    T_ASSERT(t_tmpdir(tmpl) != NULL, "mkdtemp scratch root") ;
    memcpy(gdir, tmpl, sizeof tmpl) ;

    T_RUN(test_file_two_lines_no_tail_newline) ;
    T_RUN(test_file_missing_is_success) ;
    T_RUN(test_file_empty) ;
    T_RUN(test_file_line_count_variants) ;
    T_RUN(test_file_stamp_inheritance) ;
    T_RUN(test_file_embedded_nul) ;
    T_RUN(test_name_too_long_file) ;
    T_RUN(test_name_too_long_logdir) ;
    T_RUN(test_name_exact_length) ;
    T_RUN(test_logdir_missing_is_success) ;
    T_RUN(test_logdir_empty) ;
    T_RUN(test_logdir_current_only) ;
    T_RUN(test_logdir_archive_before_current) ;
    T_RUN(test_logdir_archives_sorted) ;
    T_RUN(test_logdir_newline_guard) ;
    T_RUN(test_free_zeroes) ;

    rmdir(gdir) ;
}
