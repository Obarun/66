/*
 * follow.c
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

/* Integration tests for log_follow(): a blocking inotify/epoll event loop that
 * tails a log source. Each test forks a child that redirects fd 1 to a scratch
 * file and calls log_follow(); the parent drives the source (append, rotate,
 * grep) and then delivers SIGTERM/SIGINT. The child calls exit() (not _exit) so
 * LeakSanitizer runs on log_follow's own allocations; a leak/UAF makes the child
 * exit non-zero and the parent's status assertion bites. Assertions check the
 * captured stdout by EFFECT (bytes present/absent/counted). An outer `timeout`
 * (from the run recipe) is the anti-hang backstop; the parent also always signals
 * and reaps its child. TZ=UTC0 makes the TAI64N->local reformat deterministic. */

#include "ctest.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>
#include <stdint.h>
#include <regex.h>

#include <oblibs/clock.h>

#include <66/log.h>

static char root[256] ;   /* per-suite scratch root under the sandbox tmpfs */
static int g_case ;       /* per-test counter, gives each test a fresh subdir */

static void nap(long ms)
{
    struct timespec t = { ms / 1000, (ms % 1000) * 1000000L } ;
    nanosleep(&t, 0) ;
}

/* let the child reach sse_poll (open + seek to END) before we mutate the source */
#define SETTLE 500
/* let an appended/rotated change propagate through inotify + drain */
#define STEP   350

static void mkcase(char *dir, size_t n)
{
    snprintf(dir, n, "%s/c%d", root, g_case++) ;
    T_ASSERT_EQ(0, mkdir(dir, 0755), "mkdir case dir") ;
}

static void append(char const *path, char const *s)
{
    int fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0644) ;
    T_ASSERT(fd >= 0, "open append") ;
    size_t len = strlen(s) ;
    T_ASSERT_EQ((ssize_t)len, write(fd, s, len), "write append") ;
    close(fd) ;
}

static char slurp_buf[1 << 16] ;
static char *slurp(char const *path)
{
    int fd = open(path, O_RDONLY) ;
    T_ASSERT(fd >= 0, "open slurp") ;
    ssize_t n = 0, r ;
    while ((size_t)n < sizeof slurp_buf - 1 &&
           (r = read(fd, slurp_buf + n, sizeof slurp_buf - 1 - n)) > 0)
        n += r ;
    close(fd) ;
    slurp_buf[n < 0 ? 0 : n] = 0 ;
    return slurp_buf ;
}

static size_t count_occurrences(char const *hay, char const *needle)
{
    size_t c = 0, nl = strlen(needle) ;
    for (char const *p = hay ; (p = strstr(p, needle)) ; p += nl)
        c++ ;
    return c ;
}

#define WANT_PRESENT(out, s) do { \
    if (!strstr((out), (s))) { \
        fprintf(stderr, "FAIL: %s:%d: expected present <%s>\n--- output ---\n%s---\n", \
                __func__, __LINE__, (s), (out)) ; exit(1) ; } \
} while (0)

#define WANT_ABSENT(out, s) do { \
    if (strstr((out), (s))) { \
        fprintf(stderr, "FAIL: %s:%d: expected absent <%s>\n--- output ---\n%s---\n", \
                __func__, __LINE__, (s), (out)) ; exit(1) ; } \
} while (0)

#define WANT_COUNT(out, s, k) do { \
    size_t t_c_ = count_occurrences((out), (s)) ; \
    if (t_c_ != (size_t)(k)) { \
        fprintf(stderr, "FAIL: %s:%d: <%s> expected %d time(s), got %zu\n--- output ---\n%s---\n", \
                __func__, __LINE__, (s), (int)(k), t_c_, (out)) ; exit(1) ; } \
} while (0)

static pid_t spawn_follow(char const *name, char const *path, uint8_t is_logdir,
                          uint8_t withname, regex_t *re, char const *outpath)
{
    pid_t pid = fork() ;
    T_ASSERT(pid >= 0, "fork") ;
    if (!pid) {
        int outfd = open(outpath, O_WRONLY | O_CREAT | O_TRUNC, 0644) ;
        if (outfd < 0) _exit(3) ;
        if (dup2(outfd, 1) < 0) _exit(3) ;
        close(outfd) ;
        int r = log_follow(name, path, is_logdir, withname, re) ;
        /* exit() (not _exit) so LSan runs; encode the clean-exit contract in the
         * status: r==1 means log_follow returned "clean signal exit" */
        exit(r == 1 ? 0 : 1) ;
    }
    return pid ;
}

static void reap_ok(pid_t pid, int sig)
{
    T_ASSERT_EQ(0, kill(pid, sig), "kill child") ;
    int st = 0 ;
    T_ASSERT(waitpid(pid, &st, 0) == pid, "waitpid") ;
    T_ASSERT(WIFEXITED(st), "child exited normally (no ASan/LSan abort, no crash)") ;
    T_ASSERT_EQ(0, WEXITSTATUS(st), "child clean exit (log_follow returned 1, no leak)") ;
}

/* ---- logdir: no-backlog, tagging, 3 stamp kinds, rotation ------------------- */

static void test_logdir_backlog_tags_rotation(void)
{
    char dir[256], cur[320], arch[400], out[320] ;
    mkcase(dir, sizeof dir) ;
    snprintf(cur, sizeof cur, "%s/current", dir) ;
    snprintf(arch, sizeof arch, "%s/@4000000067000000deadbeef.s", dir) ;
    snprintf(out, sizeof out, "%s/out", dir) ;

    append(cur, "BACKLOG must not appear\n") ;   /* written before follow starts */

    pid_t pid = spawn_follow("system", dir, 1, 1, 0, out) ;
    nap(SETTLE) ;

    append(cur, "hello no stamp\n") ;
    append(cur, "2026-06-30 12:00:00.123456 iso line\n") ;

    /* a TAI64N line: emit must reformat it to local time */
    struct timespec ts = { .tv_sec = 1782746255, .tv_nsec = 42 } ;
    char tai[64] ;
    size_t tl = clock_tai64n_fmt(tai, &ts) ;
    tai[tl] = ' ' ;
    memcpy(tai + tl + 1, "tai line\n", 9) ;
    tai[tl + 1 + 9] = 0 ;
    append(cur, tai) ;
    nap(STEP) ;

    /* rotate like 66-log: current -> archive, then a fresh current */
    T_ASSERT_EQ(0, rename(cur, arch), "rotate current -> archive") ;
    append(cur, "after rotation\n") ;
    nap(STEP) ;

    reap_ok(pid, SIGTERM) ;

    char *o = slurp(out) ;
    WANT_ABSENT(o, "BACKLOG") ;
    WANT_PRESENT(o, "system: hello no stamp") ;
    WANT_PRESENT(o, "2026-06-30 12:00:00.123456 system: iso line") ;
    WANT_PRESENT(o, "2026-06-29 15:17:35.000000042 system: tai line") ;
    WANT_PRESENT(o, "system: after rotation") ;
    /* no loss AND no duplicate across rotation */
    WANT_COUNT(o, "hello no stamp", 1) ;
    WANT_COUNT(o, "after rotation", 1) ;
}

/* ---- service source: withname=0 emits the bare message, no tag ------------- */

static void test_logdir_no_name_tag(void)
{
    char dir[256], cur[320], out[320] ;
    mkcase(dir, sizeof dir) ;
    snprintf(cur, sizeof cur, "%s/current", dir) ;
    snprintf(out, sizeof out, "%s/out", dir) ;
    append(cur, "prelude\n") ;

    pid_t pid = spawn_follow("myservice", dir, 1, 0, 0, out) ;
    nap(SETTLE) ;
    append(cur, "bare body\n") ;
    nap(STEP) ;
    reap_ok(pid, SIGTERM) ;

    char *o = slurp(out) ;
    WANT_PRESENT(o, "bare body") ;
    WANT_ABSENT(o, "myservice:") ;   /* no tag when withname==0 */
    WANT_ABSENT(o, "prelude") ;      /* backlog excluded */
}

/* ---- grep filter: only matching lines pass -------------------------------- */

static void test_grep_filter(void)
{
    char dir[256], cur[320], out[320] ;
    mkcase(dir, sizeof dir) ;
    snprintf(cur, sizeof cur, "%s/current", dir) ;
    snprintf(out, sizeof out, "%s/out", dir) ;
    append(cur, "old\n") ;

    regex_t re ;
    T_ASSERT_EQ(0, regcomp(&re, "keep", REG_EXTENDED | REG_NEWLINE), "regcomp") ;

    pid_t pid = spawn_follow("system", dir, 1, 1, &re, out) ;
    nap(SETTLE) ;
    append(cur, "keep this one\n") ;
    append(cur, "drop this one\n") ;
    nap(STEP) ;
    reap_ok(pid, SIGTERM) ;
    regfree(&re) ;

    char *o = slurp(out) ;
    WANT_PRESENT(o, "system: keep this one") ;
    WANT_ABSENT(o, "drop this one") ;
}

/* ---- SIGINT also exits cleanly and flushes the tail ------------------------ */

static void test_sigint_clean_exit(void)
{
    char dir[256], cur[320], out[320] ;
    mkcase(dir, sizeof dir) ;
    snprintf(cur, sizeof cur, "%s/current", dir) ;
    snprintf(out, sizeof out, "%s/out", dir) ;
    append(cur, "start\n") ;

    pid_t pid = spawn_follow("system", dir, 1, 1, 0, out) ;
    nap(SETTLE) ;
    append(cur, "last line before sigint\n") ;
    nap(STEP) ;
    reap_ok(pid, SIGINT) ;   /* status assertions inside prove clean exit + flush */

    char *o = slurp(out) ;
    WANT_PRESENT(o, "system: last line before sigint") ;
}

/* ---- logdir with no 'current' at start: appears via IN_CREATE -------------- */

static void test_logdir_current_appears(void)
{
    char dir[256], cur[320], out[320] ;
    mkcase(dir, sizeof dir) ;
    snprintf(cur, sizeof cur, "%s/current", dir) ;
    snprintf(out, sizeof out, "%s/out", dir) ;
    /* NO current yet */

    pid_t pid = spawn_follow("system", dir, 1, 1, 0, out) ;
    nap(SETTLE) ;
    append(cur, "born after follow\n") ;   /* creation triggers reopen from start */
    nap(STEP) ;
    reap_ok(pid, SIGTERM) ;

    char *o = slurp(out) ;
    WANT_PRESENT(o, "system: born after follow") ;
}

/* ---- plain file (is_logdir=0): follow the file itself --------------------- */

static void test_plain_file(void)
{
    char dir[256], file[320], out[320] ;
    mkcase(dir, sizeof dir) ;
    snprintf(file, sizeof file, "%s/plain.log", dir) ;
    snprintf(out, sizeof out, "%s/out", dir) ;
    append(file, "preexisting\n") ;   /* backlog */

    pid_t pid = spawn_follow("system", file, 0, 1, 0, out) ;
    nap(SETTLE) ;
    append(file, "appended live\n") ;
    nap(STEP) ;
    reap_ok(pid, SIGTERM) ;

    char *o = slurp(out) ;
    WANT_PRESENT(o, "system: appended live") ;
    WANT_ABSENT(o, "preexisting") ;   /* backlog excluded on a plain file too */
}

/* ---- partial line split across reads: reliquat is kept and completed ------- */

static void test_chunked_partial_line(void)
{
    char dir[256], cur[320], out[320] ;
    mkcase(dir, sizeof dir) ;
    snprintf(cur, sizeof cur, "%s/current", dir) ;
    snprintf(out, sizeof out, "%s/out", dir) ;
    append(cur, "anchor\n") ;

    pid_t pid = spawn_follow("system", dir, 1, 0, 0, out) ;
    nap(SETTLE) ;
    append(cur, "part1 ") ;            /* no newline: held as pending */
    nap(STEP) ;
    append(cur, "part2\n") ;           /* completes the line */
    nap(STEP) ;
    reap_ok(pid, SIGTERM) ;

    char *o = slurp(out) ;
    WANT_PRESENT(o, "part1 part2") ;   /* reassembled exactly once */
    WANT_COUNT(o, "part1 part2", 1) ;
    WANT_ABSENT(o, "anchor") ;         /* backlog excluded */
}

/* ---- one read yields a complete line AND a partial remainder: the pending
 *      buffer must be compacted (bytes consumed dropped) before the remainder is
 *      completed by a later write, else stale bytes corrupt the next line ------ */

static void test_chunked_remainder_compaction(void)
{
    char dir[256], cur[320], out[320] ;
    mkcase(dir, sizeof dir) ;
    snprintf(cur, sizeof cur, "%s/current", dir) ;
    snprintf(out, sizeof out, "%s/out", dir) ;
    append(cur, "anchor\n") ;

    pid_t pid = spawn_follow("system", dir, 1, 0, 0, out) ;
    nap(SETTLE) ;
    /* single write: "alpha\n" completes a line, "bra" is a partial remainder that
     * must survive AND be compacted to the buffer front */
    append(cur, "alpha\nbra") ;
    nap(STEP) ;
    append(cur, "vo\n") ;              /* completes the remainder -> "bravo" */
    nap(STEP) ;
    reap_ok(pid, SIGTERM) ;

    char *o = slurp(out) ;
    /* exact whole-output match: NONE lines, no tag, no timestamp -> the capture is
     * precisely these two lines. Any stale byte left uncompacted in the pending
     * buffer would surface as a spurious third line and break this equality. */
    if (strcmp(o, "alpha\nbravo\n") != 0) {
        fprintf(stderr, "FAIL: %s: exact output mismatch\n  expected <alpha\\nbravo\\n>\n--- got ---\n%s---\n",
                __func__, o) ;
        exit(1) ;
    }
}

/* ---- non-existent plain file: error return (LOG_EXIT_ZERO), child exit 1 --- */

static void test_plain_file_missing(void)
{
    char dir[256], file[320], out[320] ;
    mkcase(dir, sizeof dir) ;
    snprintf(file, sizeof file, "%s/nope.log", dir) ;
    snprintf(out, sizeof out, "%s/out", dir) ;

    /* is_logdir=0 on a missing file: log_follow must fail fast (return 0), so the
     * child exits 1 WITHOUT us signalling it. A blocking loop here would hang and
     * the outer timeout would fire -> this proves the early error path is taken. */
    pid_t pid = spawn_follow("system", file, 0, 1, 0, out) ;
    int st = 0 ;
    T_ASSERT(waitpid(pid, &st, 0) == pid, "waitpid missing-file child") ;
    T_ASSERT(WIFEXITED(st), "child exited normally") ;
    T_ASSERT_EQ(1, WEXITSTATUS(st), "missing plain file -> log_follow returned 0") ;
}

T_SUITE("log_follow")
{
    setenv("TZ", "UTC0", 1) ;
    tzset() ;
    signal(SIGCHLD, SIG_DFL) ;

    char tmpl[] = "/tmp/66followXXXXXX" ;
    T_ASSERT(t_tmpdir(tmpl) != NULL, "mkdtemp scratch root") ;
    memcpy(root, tmpl, sizeof tmpl) ;

    T_RUN(test_logdir_backlog_tags_rotation) ;
    T_RUN(test_logdir_no_name_tag) ;
    T_RUN(test_grep_filter) ;
    T_RUN(test_sigint_clean_exit) ;
    T_RUN(test_logdir_current_appears) ;
    T_RUN(test_plain_file) ;
    T_RUN(test_chunked_partial_line) ;
    T_RUN(test_chunked_remainder_compaction) ;
    T_RUN(test_plain_file_missing) ;
}
