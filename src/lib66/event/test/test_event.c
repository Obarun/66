/* test_event.c — exhaustive, mutation-proven test suite for the 66 `event`
 * module (event_fifodir.c / event_reader.c / event_wait.c).
 *
 * Asserts by EFFECT (stat modes, real fds, on-disk fifo state, transition bytes
 * delivered, struct fields), never by return code alone. Hardened: ASan + UBSan
 * + LSan, with a per-process alarm() anti-hang (a mis-handled fifo would block).
 */
#include "ctest.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <oblibs/sse.h>
#include <oblibs/log.h>

#include <66/event.h>
#include "helpers.h"

/* anti-hang: any test that blocks on a fifo dies here, the run is a FAILURE. */
static void on_alarm(int sig) { (void)sig ; _exit(124) ; }

/* deterministic scratch dir under the sandbox tmpfs */
static char *mkdir_scratch(char *tmpl)
{
    char *p = t_tmpdir(tmpl) ;
    T_ASSERT(p != NULL, "mkdtemp") ;
    return p ;
}

static void rm_rf(char const *dir)
{
    DIR *d = opendir(dir) ;
    if (d) {
        struct dirent *e ;
        char path[1024] ;
        while ((e = readdir(d))) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue ;
            snprintf(path, sizeof(path), "%s/%s", dir, e->d_name) ;
            unlink(path) ;
        }
        closedir(d) ;
    }
    rmdir(dir) ;
}

/* ------------------------------------------------------------------ */
/* event_fifodir_make                                                  */
/* ------------------------------------------------------------------ */

static void test_make_nogid_mode_01733(void)
{
    char tmpl[] = "/tmp/ev_mk_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char path[1024] ; snprintf(path, sizeof(path), "%s/fd", base) ;

    /* run under a non-trivial umask to prove the function neutralises it */
    mode_t old = umask(077) ;

    int r = event_fifodir_make(path, (gid_t)-1) ;
    T_ASSERT_EQ(1, r, "make no-gid returns 1") ;

    struct stat st ;
    T_ASSERT_EQ(0, stat(path, &st), "stat created dir") ;
    T_ASSERT(S_ISDIR(st.st_mode), "is a directory") ;
    /* 01733 = sticky + rwx-wx-wx ; umask(077) must NOT have masked it */
    T_ASSERT_EQ(01733, st.st_mode & 07777, "no-gid mode is exactly 01733") ;

    /* umask must be restored to what we set (077), proving the function put it
     * back after its umask(0). A single probe reads the live umask. */
    mode_t now = umask(old) ;   /* sets back to original, returns the live one */
    T_ASSERT_EQ(077, now, "umask restored after make") ;

    rm_rf(base) ;
}

/* make on an existing-and-ours dir RE-APPLIES the canonical mode (it no longer
 * leaves perms untouched): after wrecking the mode, a second make must restore
 * 01733 (no-gid) so the postcondition holds however the dir was left. */
static void test_make_reapplies_mode(void)
{
    char tmpl[] = "/tmp/ev_idem_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char path[1024] ; snprintf(path, sizeof(path), "%s/fd", base) ;

    T_ASSERT_EQ(1, event_fifodir_make(path, (gid_t)-1), "first make") ;
    /* deliberately wreck the mode; the second make must RESTORE the canonical one */
    T_ASSERT_EQ(0, chmod(path, 0700), "force mode 0700") ;

    T_ASSERT_EQ(1, event_fifodir_make(path, (gid_t)-1), "second make re-applies") ;

    struct stat st ;
    T_ASSERT_EQ(0, stat(path, &st), "stat") ;
    T_ASSERT_EQ(01733, st.st_mode & 07777, "second make restores the canonical mode") ;

    rm_rf(base) ;
}

static void test_make_existing_not_dir_ENOTDIR(void)
{
    char tmpl[] = "/tmp/ev_nd_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char path[1024] ; snprintf(path, sizeof(path), "%s/fd", base) ;

    int fd = open(path, O_CREAT | O_WRONLY, 0600) ;
    T_ASSERT(fd >= 0, "create regular file") ;
    close(fd) ;

    errno = 0 ;
    int r = event_fifodir_make(path, (gid_t)-1) ;
    T_ASSERT_EQ(0, r, "make on a regular file returns 0") ;
    T_ASSERT_ERRNO(ENOTDIR, "errno ENOTDIR on non-dir") ;

    rm_rf(base) ;
}

/* lstat (not stat) hardening: a symlink planted at the fifodir path must be
 * REFUSED (ENOTDIR), never followed. With stat() the link would resolve to the
 * owned target dir and make would chmod THROUGH it; lstat sees the link itself. */
static void test_make_rejects_symlink(void)
{
    char tmpl[] = "/tmp/ev_sym_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char target[1024] ; snprintf(target, sizeof(target), "%s/real", base) ;
    char path[1024] ;   snprintf(path, sizeof(path), "%s/fd", base) ;

    T_ASSERT_EQ(0, mkdir(target, 0700), "create owned target dir") ;
    T_ASSERT_EQ(0, symlink(target, path), "plant symlink at fifodir path") ;

    struct stat before ;
    T_ASSERT_EQ(0, stat(target, &before), "stat target before") ;

    errno = 0 ;
    int r = event_fifodir_make(path, (gid_t)-1) ;
    T_ASSERT_EQ(0, r, "make on a symlink returns 0") ;
    T_ASSERT_ERRNO(ENOTDIR, "errno ENOTDIR: symlink rejected, not followed") ;

    // target must be untouched: make must not have chmod'd through the link
    struct stat after ;
    T_ASSERT_EQ(0, stat(target, &after), "stat target after") ;
    T_ASSERT_EQ((int)(before.st_mode & 07777), (int)(after.st_mode & 07777),
                "symlink target perms left untouched") ;

    rm_rf(base) ;
}

static void test_make_gid_path_chmod_branch(void)
{
    /* The chown leg must be load-bearing: a fresh dir inherits the process egid
     * as its group, so to prove chown actually ran we target a SUPPLEMENTARY
     * group that differs from the egid. chown(-1, gid) to one of our own groups
     * is allowed unprivileged. If no such group exists, the chown leg is not
     * separable from the default and we skip the group assertion (declared). */
    gid_t egid = getegid() ;
    gid_t groups[64] ;
    int ng = getgroups(64, groups) ;
    T_ASSERT(ng >= 0, "getgroups") ;
    gid_t target = (gid_t)-1 ;
    for (int i = 0 ; i < ng ; i++)
        if (groups[i] != egid) { target = groups[i] ; break ; }

    char tmpl[] = "/tmp/ev_gid_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char path[1024] ; snprintf(path, sizeof(path), "%s/fd", base) ;

    gid_t g = (target != (gid_t)-1) ? target : egid ;
    int r = event_fifodir_make(path, g) ;
    T_ASSERT_EQ(1, r, "make with own gid returns 1") ;

    struct stat st ;
    T_ASSERT_EQ(0, stat(path, &st), "stat") ;
    T_ASSERT_EQ(03730, st.st_mode & 07777, "gid mode is exactly 03730 (sgid+grp-write)") ;
    /* this assertion only bites the chown leg when target != egid */
    T_ASSERT_EQ((long long)g, (long long)st.st_gid, "dir group is the requested gid (chown ran)") ;
    if (target == (gid_t)-1)
        fprintf(stderr, "[no supplementary group != egid; chown leg not separable] ") ;

    rm_rf(base) ;
}

static void test_make_parent_missing_returns_0(void)
{
    /* mkdir fails with ENOENT (parent absent), errno != EEXIST branch */
    char path[] = "/tmp/ev_nope_does_not_exist_XXX/sub/fd" ;
    errno = 0 ;
    int r = event_fifodir_make(path, (gid_t)-1) ;
    T_ASSERT_EQ(0, r, "make with missing parent returns 0") ;
    T_ASSERT_ERRNO(ENOENT, "errno ENOENT propagated from mkdir") ;
}

/* ------------------------------------------------------------------ */
/* event_fifodir_clean                                                 */
/* ------------------------------------------------------------------ */

/* build a fifo whose name has the exact ftrig1 layout (len 39). */
static void make_fifo_named(char const *dir, char const *name, char *out, size_t outn)
{
    snprintf(out, outn, "%s/%s", dir, name) ;
    T_ASSERT_EQ(0, mkfifo(out, 0622), "mkfifo named") ;
}

/* a valid 39-char ftrig1 name (prefix7 + 25 stamp + ':' + 6 rand) */
#define VALID_NAME_A "ftrig1:@400000000000000000000000:aaaaaa"
#define VALID_NAME_B "ftrig1:@400000000000000000000000:bbbbbb"

static void test_clean_orphan_unlinked(void)
{
    char tmpl[] = "/tmp/ev_cl1_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;

    char fifo[1024] ;
    make_fifo_named(base, VALID_NAME_A, fifo, sizeof(fifo)) ;
    T_ASSERT_EQ(EVENT_FIFO_NAMELEN, (long long)strlen(VALID_NAME_A), "name is exactly 39") ;

    /* no reader: O_WRONLY|O_NONBLOCK gives ENXIO -> orphan -> unlinked */
    int r = event_fifodir_clean(base) ;
    T_ASSERT_EQ(1, r, "clean returns 1") ;

    struct stat st ;
    errno = 0 ;
    T_ASSERT_EQ(-1, stat(fifo, &st), "orphan fifo is gone") ;
    T_ASSERT_ERRNO(ENOENT, "orphan fifo unlinked (ENOENT)") ;

    rm_rf(base) ;
}

static void test_clean_live_kept(void)
{
    char tmpl[] = "/tmp/ev_cl2_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;

    char fifo[1024] ;
    make_fifo_named(base, VALID_NAME_A, fifo, sizeof(fifo)) ;

    /* hold a read end open -> the fifo HAS a reader -> O_WRONLY succeeds -> kept */
    int rfd = open(fifo, O_RDONLY | O_NONBLOCK | O_CLOEXEC) ;
    T_ASSERT(rfd >= 0, "open read end") ;

    int r = event_fifodir_clean(base) ;
    T_ASSERT_EQ(1, r, "clean returns 1") ;

    struct stat st ;
    T_ASSERT_EQ(0, stat(fifo, &st), "live fifo is kept") ;
    T_ASSERT(S_ISFIFO(st.st_mode), "still a fifo") ;

    close(rfd) ;
    rm_rf(base) ;
}

static void test_clean_ignores_nonmatching(void)
{
    char tmpl[] = "/tmp/ev_cl3_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;

    /* (a) wrong prefix, right length-ish */
    char f1[1024] ; snprintf(f1, sizeof(f1), "%s/%s", base, "zzzzzz:@4000000000000000000000000:aaaaaa") ;
    T_ASSERT_EQ(0, mkfifo(f1, 0622), "mkfifo wrong-prefix orphan") ;
    /* (b) right prefix, wrong length (too short) */
    char f2[1024] ; snprintf(f2, sizeof(f2), "%s/%s", base, "ftrig1:short") ;
    T_ASSERT_EQ(0, mkfifo(f2, 0622), "mkfifo short orphan") ;

    int r = event_fifodir_clean(base) ;
    T_ASSERT_EQ(1, r, "clean returns 1") ;

    /* both are orphans by the ENXIO test, but neither matches the filter -> kept */
    struct stat st ;
    T_ASSERT_EQ(0, stat(f1, &st), "wrong-prefix entry kept") ;
    T_ASSERT_EQ(0, stat(f2, &st), "wrong-length entry kept") ;

    rm_rf(base) ;
}

static void test_clean_mixed_orphan_and_live(void)
{
    char tmpl[] = "/tmp/ev_cl4_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;

    char orphan[1024], live[1024] ;
    make_fifo_named(base, VALID_NAME_A, orphan, sizeof(orphan)) ;
    make_fifo_named(base, VALID_NAME_B, live, sizeof(live)) ;

    int rfd = open(live, O_RDONLY | O_NONBLOCK | O_CLOEXEC) ;
    T_ASSERT(rfd >= 0, "open live read end") ;

    T_ASSERT_EQ(1, event_fifodir_clean(base), "clean returns 1") ;

    struct stat st ;
    errno = 0 ;
    T_ASSERT_EQ(-1, stat(orphan, &st), "orphan swept") ;
    T_ASSERT_ERRNO(ENOENT, "orphan gone") ;
    T_ASSERT_EQ(0, stat(live, &st), "live kept") ;

    close(rfd) ;
    rm_rf(base) ;
}

static void test_clean_missing_dir_returns_0(void)
{
    errno = 0 ;
    int r = event_fifodir_clean("/tmp/ev_absent_dir_zzz_XXX") ;
    T_ASSERT_EQ(0, r, "clean on absent dir returns 0") ;
    T_ASSERT_ERRNO(ENOENT, "opendir ENOENT") ;
}

/* ------------------------------------------------------------------ */
/* event_fifo_subscribe — the fifo create trick                      */
/* ------------------------------------------------------------------ */

/* a sink handler that records every byte of each chunk into a caller buffer */
typedef struct { char buf[4096] ; size_t n ; } sink_t ;
static void sink_handler(event_reader_t *r, char const *buf, size_t len, void *data)
{
    (void)r ;
    sink_t *s = data ;
    for (size_t i = 0 ; i < len && s->n < sizeof(s->buf) ; i++)
        s->buf[s->n++] = buf[i] ;
}

static void test_subscribe_effects_and_mode(void)
{
    char tmpl[] = "/tmp/ev_sub_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char ev[1024] ; snprintf(ev, sizeof(ev), "%s/event", base) ;
    T_ASSERT_EQ(1, event_fifodir_make(ev, (gid_t)-1), "make eventdir") ;

    sse_epoll_t ep ;
    T_ASSERT_EQ(1, sse_new(&ep, 1), "sse_new") ;

    sink_t s = {0} ;
    event_fifo_t r ;
    int rc = event_fifo_subscribe(&r, &ep, ev, sink_handler, &s, 0) ;
    T_ASSERT_EQ(1, rc, "subscribe returns 1") ;

    /* EFFECT: fifopath is set, visible, a fifo, mode forced to 0622 */
    T_ASSERT(r.fifopath[0] != 0, "fifopath populated") ;
    T_ASSERT(strstr(r.fifopath, "/ftrig1:") != NULL, "visible name has ftrig1 prefix") ;
    T_ASSERT(strstr(r.fifopath, "/.ftrig1:") == NULL, "visible name is NOT the hidden name") ;

    struct stat st ;
    T_ASSERT_EQ(0, stat(r.fifopath, &st), "fifo exists on disk") ;
    T_ASSERT(S_ISFIFO(st.st_mode), "it is a fifo") ;
    T_ASSERT_EQ(0622, st.st_mode & 07777, "fifo mode forced to 0622") ;

    /* EFFECT: write end held open (>=0) */
    T_ASSERT(r.wfd >= 0, "write end held open") ;

    /* EFFECT: exactly one producer-eligible fifo, and a producer can open it */
    int enx = -1 ;
    T_ASSERT_EQ(1, fanout_count(ev), "exactly one visible fifo") ;
    T_ASSERT_EQ(1, fanout_open_ok(ev, &enx), "producer opens it without ENXIO") ;
    T_ASSERT_EQ(0, enx, "no ENXIO on the published fifo") ;

    /* no hidden leftover */
    T_ASSERT_EQ(1, dir_entries(ev), "no stray (hidden) leftover entry") ;

    event_fifo_unsubscribe(&r) ;
    /* EFFECT: unsubscribe closes wfd and unlinks the fifo */
    T_ASSERT_EQ(-1, r.wfd, "wfd reset to -1") ;
    T_ASSERT_EQ(0, r.fifopath[0], "fifopath cleared") ;
    T_ASSERT_EQ(0, dir_entries(ev), "fifo unlinked on unsubscribe") ;

    sse_free(&ep) ;
    rm_rf(ev) ; rm_rf(base) ;
}

/* THE TRICK, hammered: across many subscribe/unsubscribe cycles, a concurrent
 * producer scanning the dir must NEVER see a visible ftrig1 fifo it cannot open
 * (i.e. never an ENXIO). The hidden-name + rename ordering is what guarantees
 * this. We interleave a fanout scan right after subscribe each time. */
static void test_subscribe_trick_never_visible_without_reader(void)
{
    char tmpl[] = "/tmp/ev_trick_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char ev[1024] ; snprintf(ev, sizeof(ev), "%s/event", base) ;
    T_ASSERT_EQ(1, event_fifodir_make(ev, (gid_t)-1), "make eventdir") ;

    sse_epoll_t ep ;
    T_ASSERT_EQ(1, sse_new(&ep, 1), "sse_new") ;
    sink_t s = {0} ;

    for (int i = 0 ; i < 200 ; i++) {
        event_fifo_t r ;
        T_ASSERT_EQ(1, event_fifo_subscribe(&r, &ep, ev, sink_handler, &s, 0), "subscribe cycle") ;

        /* the instant the fifo is visible, its read end already exists:
         * a producer opening it must succeed, never ENXIO. */
        int enx = -1 ;
        int ok = fanout_open_ok(ev, &enx) ;
        T_ASSERT_EQ(1, ok, "visible fifo always openable by producer") ;
        T_ASSERT_EQ(0, enx, "never a visible fifo without a reader") ;

        event_fifo_unsubscribe(&r) ;
        T_ASSERT_EQ(0, dir_entries(ev), "no leftover after unsubscribe") ;
    }

    sse_free(&ep) ;
    rm_rf(ev) ; rm_rf(base) ;
}

/* THE TRICK — deterministic, mutation-sensitive proof of the hidden name.
 *
 * The create-side guarantee is "a producer (filtering on the visible 'ftrig1:'
 * prefix + length 39) never sees the fifo before its read end exists". The
 * protected window (mkfifo -> open read end) is sub-microsecond, so a black-box
 * concurrent race cannot hit it reliably. (The teardown side opens no window:
 * event_fifo_unsubscribe unlinks the visible name before closing the ends,
 * like s6 ftrig1_free.) What CAN be
 * proven deterministically, and what the hidden name exists to ensure, is:
 *
 *   (1) the PUBLISHED entry carries the visible 'ftrig1:' name, never '.ftrig1:'
 *   (2) the hidden temp '.ftrig1:' never survives into the published dir
 *   (3) at no point after subscribe returns are there more producer-visible
 *       entries than live readers
 *
 * Mutating the hidden name away (auto_strings "/." -> "/") leaves the temp
 * visible to the producer filter during create AND, on a name clash retry path,
 * could publish duplicates; assertion (1)/(3) below bite on the simplest form
 * of that mutation because the published-vs-hidden distinction collapses. */
static void test_subscribe_trick_hidden_name(void)
{
    char tmpl[] = "/tmp/ev_hid_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char ev[1024] ; snprintf(ev, sizeof(ev), "%s/event", base) ;
    T_ASSERT_EQ(1, event_fifodir_make(ev, (gid_t)-1), "make eventdir") ;

    sse_epoll_t epp ;
    T_ASSERT_EQ(1, sse_new(&epp, 1), "sse_new") ;
    sink_t s = {0} ;
    event_fifo_t r ;
    T_ASSERT_EQ(1, event_fifo_subscribe(&r, &epp, ev, sink_handler, &s, 0), "subscribe") ;

    /* (1) published name is visible, not the hidden '.' name */
    char const *slash = strrchr(r.fifopath, '/') ;
    T_ASSERT(slash != NULL, "fifopath has a slash") ;
    T_ASSERT_EQ(0, strncmp(slash + 1, EVENT_FIFO_PREFIX, EVENT_FIFO_PREFIXLEN),
                "published basename starts with visible ftrig1: (not '.')") ;
    T_ASSERT(slash[1] != '.', "published basename is not the hidden '.ftrig1:' name") ;

    /* (2) no '.ftrig1:' temp survives; exactly one entry, and it is producer-visible */
    DIR *d = opendir(ev) ; T_ASSERT(d != NULL, "opendir") ;
    struct dirent *e ; int hidden = 0, total = 0 ;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue ;
        total++ ;
        if (!strncmp(e->d_name, ".ftrig1:", 8)) hidden++ ;
    }
    closedir(d) ;
    T_ASSERT_EQ(0, hidden, "no hidden '.ftrig1:' temp left behind") ;
    T_ASSERT_EQ(1, total, "exactly one entry on disk") ;
    /* (3) producer-visible entries == live readers (1) */
    T_ASSERT_EQ(1, fanout_count(ev), "one producer-visible fifo for one live reader") ;

    event_fifo_unsubscribe(&r) ;
    sse_free(&epp) ;
    rm_rf(ev) ; rm_rf(base) ;
}

/* THE TRICK under a concurrent producer. A child hammers the dir like
 * s6-supervise's fanout while the parent churns subscriptions. Besides the
 * stress/hardening goal (ASan/UBSan stay clean, no hang, no double-free under
 * real fd churn), it ASSERTS the producer never hits ENXIO: now that unsubscribe
 * unlinks before it closes the ends, the visible name only ever exists while the
 * read end is open (otherwise the open races to ENOENT, never ENXIO). */
static void test_subscribe_trick_concurrent_smoke(void)
{
    char tmpl[] = "/tmp/ev_race_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char ev[1024] ; snprintf(ev, sizeof(ev), "%s/event", base) ;
    T_ASSERT_EQ(1, event_fifodir_make(ev, (gid_t)-1), "make eventdir") ;

    volatile long *sh = mmap(NULL, 2 * sizeof(long), PROT_READ | PROT_WRITE,
                             MAP_SHARED | MAP_ANONYMOUS, -1, 0) ;
    T_ASSERT(sh != MAP_FAILED, "mmap shared") ;
    sh[0] = 0 ;   /* stop flag */
    sh[1] = 0 ;   /* ENXIO count seen by the producer */

    pid_t pid = fork() ;
    T_ASSERT(pid >= 0, "fork") ;
    if (pid == 0) {
        while (!sh[0]) { int enx = 0 ; fanout_open_ok(ev, &enx) ; sh[1] += enx ; }
        _exit(0) ;
    }

    sse_epoll_t epp ;
    T_ASSERT_EQ(1, sse_new(&epp, 1), "sse_new") ;
    sink_t s = {0} ;
    for (int i = 0 ; i < 2000 ; i++) {
        event_fifo_t r ;
        T_ASSERT_EQ(1, event_fifo_subscribe(&r, &epp, ev, sink_handler, &s, 0), "subscribe churn") ;
        event_fifo_unsubscribe(&r) ;
        T_ASSERT_EQ(0, r.wfd != -1, "wfd cleared after unsubscribe") ;
    }
    sse_free(&epp) ;

    sh[0] = 1 ;
    int wst = 0 ;
    T_ASSERT(waitpid(pid, &wst, 0) == pid, "reap child") ;

    T_ASSERT_EQ(0, (int)sh[1], "producer never hit ENXIO (unlink-before-close)") ;

    munmap((void *)sh, 2 * sizeof(long)) ;
    rm_rf(ev) ; rm_rf(base) ;
}

/* DOUBLE-FD: the reader holds the write end, so even when an external producer
 * opens and closes its own write end repeatedly, the read end never sees EOF
 * (no SSE_HUP). We prove it by reading actual bytes after many open/close
 * churns of an external writer, and by the watcher never reporting HUP. */
static void test_subscribe_double_fd_no_eof(void)
{
    char tmpl[] = "/tmp/ev_dfd_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char ev[1024] ; snprintf(ev, sizeof(ev), "%s/event", base) ;
    T_ASSERT_EQ(1, event_fifodir_make(ev, (gid_t)-1), "make eventdir") ;

    sse_epoll_t ep ;
    T_ASSERT_EQ(1, sse_new(&ep, 1), "sse_new") ;
    sink_t s = {0} ;
    event_fifo_t r ;
    T_ASSERT_EQ(1, event_fifo_subscribe(&r, &ep, ev, sink_handler, &s, 0), "subscribe") ;

    /* external producer: open O_WRONLY, write nothing, close — repeatedly.
     * If the reader did NOT hold its own wfd, the first such close would put
     * the read end at EOF (HUP). The held wfd prevents that. */
    for (int i = 0 ; i < 50 ; i++) {
        int w = open(r.fifopath, O_WRONLY | O_NONBLOCK | O_CLOEXEC) ;
        T_ASSERT(w >= 0, "external producer opens write end") ;
        close(w) ;
    }

    /* now actually deliver a byte and confirm it reads cleanly (no HUP path) */
    T_ASSERT_EQ(1, fanout_write(ev, event_to_byte(EVENT_UP)), "producer writes one byte") ;

    /* drive the loop once (timeout so it can't hang); the byte must arrive */
    int pr = sse_run(&ep, 200) ;
    T_ASSERT_EQ(1, pr, "sse_run ok") ;
    T_ASSERT_EQ(1, (long long)s.n, "exactly one byte delivered") ;
    T_ASSERT_EQ(event_to_byte(EVENT_UP), s.buf[0], "the delivered byte is 'u'") ;

    /* the read end is still alive (wfd kept it open) — verify by a second byte */
    s.n = 0 ;
    T_ASSERT_EQ(1, fanout_write(ev, event_to_byte(EVENT_READY)), "producer writes again") ;
    T_ASSERT_EQ(1, sse_run(&ep, 200), "sse_run ok 2") ;
    T_ASSERT_EQ(1, (long long)s.n, "second byte delivered after writer churn") ;
    T_ASSERT_EQ(event_to_byte(EVENT_READY), s.buf[0], "second byte is 'U'") ;

    event_fifo_unsubscribe(&r) ;
    sse_free(&ep) ;
    rm_rf(ev) ; rm_rf(base) ;
}

/* PRODUCER fanout: event_fifodir_notify must reach a live subscriber with the
 * exact bytes, including a multi-byte combo like the "dD" set_down_and_ready
 * emits, and report success. */
static void test_notify_delivers_to_subscriber(void)
{
    char tmpl[] = "/tmp/ev_nf_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char ev[1024] ; snprintf(ev, sizeof(ev), "%s/event", base) ;
    T_ASSERT_EQ(1, event_fifodir_make(ev, (gid_t)-1), "make eventdir") ;

    sse_epoll_t ep ;
    T_ASSERT_EQ(1, sse_new(&ep, 1), "sse_new") ;
    sink_t s = {0} ;
    event_fifo_t r ;
    T_ASSERT_EQ(1, event_fifo_subscribe(&r, &ep, ev, sink_handler, &s, 0), "subscribe") ;

    /* single transition byte */
    T_ASSERT_EQ(1, event_fifodir_notify(ev, "U", 1), "notify returns 1") ;
    T_ASSERT_EQ(1, sse_run(&ep, 200), "sse_run ok") ;
    T_ASSERT_EQ(1, (long long)s.n, "exactly one byte delivered") ;
    T_ASSERT_EQ(event_to_byte(EVENT_READY), s.buf[0], "the delivered byte is 'U'") ;

    /* multi-byte combo arrives intact */
    s.n = 0 ;
    T_ASSERT_EQ(1, event_fifodir_notify(ev, "dD", 2), "notify combo returns 1") ;
    T_ASSERT_EQ(1, sse_run(&ep, 200), "sse_run ok 2") ;
    T_ASSERT_EQ(2, (long long)s.n, "two bytes delivered") ;
    T_ASSERT_EQ(event_to_byte(EVENT_DOWN), s.buf[0], "first byte is 'd'") ;
    T_ASSERT_EQ(event_to_byte(EVENT_DOWN_READY), s.buf[1], "second byte is 'D'") ;

    event_fifo_unsubscribe(&r) ;
    sse_free(&ep) ;
    rm_rf(ev) ; rm_rf(base) ;
}

/* event_fifodir_notify sweeps an orphan fifo (no reader -> ENXIO -> unlink) and
 * still succeeds; a missing fifodir is a hard error (returns 0). */
static void test_notify_sweeps_orphan_and_missing_dir(void)
{
    char tmpl[] = "/tmp/ev_no_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char ev[1024] ; snprintf(ev, sizeof(ev), "%s/event", base) ;
    T_ASSERT_EQ(1, event_fifodir_make(ev, (gid_t)-1), "make eventdir") ;

    /* a producer-eligible fifo with NO reader: "ftrig1:" + filler, 39 chars */
    char name[EVENT_FIFO_NAMELEN + 1] ;
    memset(name, 'a', EVENT_FIFO_NAMELEN) ;
    memcpy(name, EVENT_FIFO_PREFIX, EVENT_FIFO_PREFIXLEN) ;
    name[EVENT_FIFO_NAMELEN] = 0 ;
    char orphan[1024] ; snprintf(orphan, sizeof(orphan), "%s/%s", ev, name) ;
    T_ASSERT_EQ(0, mkfifo(orphan, 0622), "create orphan fifo") ;
    T_ASSERT_EQ(1, fanout_count(ev), "orphan is producer-eligible") ;

    /* open hits ENXIO (no reader) -> unlink; the fanout still returns 1 */
    T_ASSERT_EQ(1, event_fifodir_notify(ev, "u", 1), "notify sweeps orphan, returns 1") ;
    T_ASSERT_EQ(0, fanout_count(ev), "orphan unlinked") ;

    /* a missing fifodir cannot be opened: hard error */
    T_ASSERT_EQ(0, event_fifodir_notify("/tmp/ev_absent_zzz_QQQ", "u", 1), "missing dir returns 0") ;

    rm_rf(ev) ; rm_rf(base) ;
}

static void test_subscribe_nametoolong(void)
{
    /* build an eventdir path long enough that strlen+2+39+1 > SS_MAX_PATH,
     * but the dir itself need not exist: the length check precedes any syscall. */
    char ev[SS_MAX_PATH + 64] ;
    memset(ev, 'a', sizeof(ev) - 1) ;
    ev[0] = '/' ;
    ev[sizeof(ev) - 1] = 0 ;

    sse_epoll_t ep ;
    T_ASSERT_EQ(1, sse_new(&ep, 1), "sse_new") ;

    event_fifo_t r ;
    errno = 0 ;
    int rc = event_fifo_subscribe(&r, &ep, ev, sink_handler, NULL, 0) ;
    T_ASSERT_EQ(0, rc, "subscribe returns 0 on too-long path") ;
    T_ASSERT_ERRNO(ENAMETOOLONG, "errno ENAMETOOLONG") ;
    T_ASSERT_EQ(0, r.fifopath[0], "nothing published") ;
    T_ASSERT_EQ(-1, r.wfd, "no write fd opened") ;

    sse_free(&ep) ;
}

static void test_subscribe_eventdir_missing_cleanup(void)
{
    /* eventdir does not exist: mkfifo fails with ENOENT (not EEXIST) ->
     * subscribe returns 0, fifopath cleared, nothing left behind. */
    sse_epoll_t ep ;
    T_ASSERT_EQ(1, sse_new(&ep, 1), "sse_new") ;

    event_fifo_t r ;
    errno = 0 ;
    int rc = event_fifo_subscribe(&r, &ep, "/tmp/ev_no_such_dir_zzz_XXX", sink_handler, NULL, 0) ;
    T_ASSERT_EQ(0, rc, "subscribe on missing dir returns 0") ;
    T_ASSERT_ERRNO(ENOENT, "errno ENOENT from mkfifo") ;
    T_ASSERT_EQ(0, r.fifopath[0], "fifopath cleared on failure") ;
    T_ASSERT_EQ(-1, r.wfd, "wfd stays -1 on failure") ;

    sse_free(&ep) ;
}

/* ------------------------------------------------------------------ */
/* event_reader_cb — byte delivery                                     */
/* ------------------------------------------------------------------ */

/* drive the loop until n bytes seen or a bounded number of polls elapse */
static void pump(sse_epoll_t *ep, sink_t *s, size_t want)
{
    for (int i = 0 ; i < 1000 && s->n < want ; i++)
        if (sse_run(ep, 200) != 1) break ;
}

static void subscribe_one(sse_epoll_t *ep, event_fifo_t *r, sink_t *s, char const *ev)
{
    T_ASSERT_EQ(1, event_fifo_subscribe(r, ep, ev, sink_handler, s, 0), "subscribe") ;
}

static void test_cb_single_byte(void)
{
    char tmpl[] = "/tmp/ev_cb1_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char ev[1024] ; snprintf(ev, sizeof(ev), "%s/event", base) ;
    T_ASSERT_EQ(1, event_fifodir_make(ev, (gid_t)-1), "make") ;
    sse_epoll_t ep ; T_ASSERT_EQ(1, sse_new(&ep, 1), "sse_new") ;
    sink_t s = {0} ; event_fifo_t r ;
    subscribe_one(&ep, &r, &s, ev) ;

    T_ASSERT_EQ(1, fanout_write(ev, event_to_byte(EVENT_DOWN)), "write 1 byte") ;
    pump(&ep, &s, 1) ;
    T_ASSERT_EQ(1, (long long)s.n, "one byte reported once") ;
    T_ASSERT_EQ(event_to_byte(EVENT_DOWN), s.buf[0], "byte value 'd'") ;

    event_fifo_unsubscribe(&r) ; sse_free(&ep) ; rm_rf(ev) ; rm_rf(base) ;
}

static void test_cb_multi_in_one_write(void)
{
    char tmpl[] = "/tmp/ev_cb2_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char ev[1024] ; snprintf(ev, sizeof(ev), "%s/event", base) ;
    T_ASSERT_EQ(1, event_fifodir_make(ev, (gid_t)-1), "make") ;
    sse_epoll_t ep ; T_ASSERT_EQ(1, sse_new(&ep, 1), "sse_new") ;
    sink_t s = {0} ; event_fifo_t r ;
    subscribe_one(&ep, &r, &s, ev) ;

    /* one write of several bytes -> each reported once, in order */
    char const *seq = "uUdDOsx" ;
    int w = open(r.fifopath, O_WRONLY | O_NONBLOCK | O_CLOEXEC) ;
    T_ASSERT(w >= 0, "open writer") ;
    T_ASSERT_EQ(7, (long long)write(w, seq, 7), "write 7 bytes at once") ;
    close(w) ;

    pump(&ep, &s, 7) ;
    T_ASSERT_EQ(7, (long long)s.n, "all 7 bytes reported") ;
    T_ASSERT_EQ(0, memcmp(s.buf, seq, 7), "bytes reported in order, once each") ;

    event_fifo_unsubscribe(&r) ; sse_free(&ep) ; rm_rf(ev) ; rm_rf(base) ;
}

static void test_cb_large_batch_over_256(void)
{
    /* > 256 bytes forces the internal read loop (buf[256]) to iterate */
    char tmpl[] = "/tmp/ev_cb3_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char ev[1024] ; snprintf(ev, sizeof(ev), "%s/event", base) ;
    T_ASSERT_EQ(1, event_fifodir_make(ev, (gid_t)-1), "make") ;
    sse_epoll_t ep ; T_ASSERT_EQ(1, sse_new(&ep, 1), "sse_new") ;
    sink_t s = {0} ; event_fifo_t r ;
    subscribe_one(&ep, &r, &s, ev) ;

    enum { N = 1000 } ;
    char big[N] ;
    for (int i = 0 ; i < N ; i++) big[i] = (i & 1) ? event_to_byte(EVENT_UP) : event_to_byte(EVENT_DOWN) ;

    int w = open(r.fifopath, O_WRONLY | O_NONBLOCK | O_CLOEXEC) ;
    T_ASSERT(w >= 0, "open writer") ;
    /* write the whole batch (pipe buffer is >=4096, N=1000 fits) */
    ssize_t tot = 0 ;
    while (tot < N) {
        ssize_t k = write(w, big + tot, N - tot) ;
        if (k < 0) { if (errno == EINTR) continue ; break ; }
        tot += k ;
    }
    close(w) ;
    T_ASSERT_EQ(N, (long long)tot, "wrote full batch") ;

    /* a SINGLE poll must drain the whole batch: the callback's inner read loop
     * iterates over the 256-byte buffer until EAGAIN. (This bites a mutation
     * that breaks out of the read loop after one buffer-full.) */
    T_ASSERT_EQ(1, sse_run(&ep, 500), "sse_run ok") ;
    T_ASSERT_EQ(N, (long long)s.n, "one poll drains all >256 bytes (inner read loop)") ;
    T_ASSERT_EQ(0, memcmp(s.buf, big, N), "batch content preserved exactly") ;

    event_fifo_unsubscribe(&r) ; sse_free(&ep) ; rm_rf(ev) ; rm_rf(base) ;
}

static void test_cb_multiple_writes(void)
{
    char tmpl[] = "/tmp/ev_cb4_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char ev[1024] ; snprintf(ev, sizeof(ev), "%s/event", base) ;
    T_ASSERT_EQ(1, event_fifodir_make(ev, (gid_t)-1), "make") ;
    sse_epoll_t ep ; T_ASSERT_EQ(1, sse_new(&ep, 1), "sse_new") ;
    sink_t s = {0} ; event_fifo_t r ;
    subscribe_one(&ep, &r, &s, ev) ;

    char const *want = "udU" ;
    for (int i = 0 ; i < 3 ; i++) {
        T_ASSERT_EQ(1, fanout_write(ev, want[i]), "discrete write") ;
        pump(&ep, &s, (size_t)(i + 1)) ;
    }
    T_ASSERT_EQ(3, (long long)s.n, "three discrete writes, three bytes") ;
    T_ASSERT_EQ(0, memcmp(s.buf, want, 3), "order preserved across writes") ;

    event_fifo_unsubscribe(&r) ; sse_free(&ep) ; rm_rf(ev) ; rm_rf(base) ;
}

/* ------------------------------------------------------------------ */
/* event_wait — wait_and over N fifodirs (the prod scenario)           */
/* ------------------------------------------------------------------ */

/* make N eventdirs as base/ev0..evN-1, fill dirs[] with their paths (owned). */
static void make_dirs(char *base, size_t n, char **dirs)
{
    for (size_t i = 0 ; i < n ; i++) {
        dirs[i] = malloc(1024) ;
        T_ASSERT(dirs[i] != NULL, "malloc dir") ;
        snprintf(dirs[i], 1024, "%s/ev%zu", base, i) ;
        T_ASSERT_EQ(1, event_fifodir_make(dirs[i], (gid_t)-1), "make eventdir") ;
    }
}
static void free_dirs(size_t n, char **dirs) { for (size_t i = 0 ; i < n ; i++) { rm_rf(dirs[i]) ; free(dirs[i]) ; } }

static void test_wait_all_triggered(void)
{
    char tmpl[] = "/tmp/ev_wa_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    enum { N = 4 } ;
    char *dirs[N] ; make_dirs(base, N, dirs) ;

    event_wait_t w ;
    T_ASSERT_EQ(1, event_wait_init(&w, (char const *const *)dirs, N, EVENT_READY), "init") ;

    /* producer triggers AFTER subscribe, BEFORE run (prod ordering) */
    for (size_t i = 0 ; i < N ; i++)
        T_ASSERT_EQ(1, fanout_write(dirs[i], event_to_byte(EVENT_READY)), "trigger each dir") ;

    int r = event_wait_run(&w, 2000) ;
    T_ASSERT_EQ(1, r, "run returns 1 when all triggered") ;
    T_ASSERT_EQ(N, (long long)w.triggered, "triggered == n") ;

    event_wait_free(&w) ;
    T_ASSERT_EQ(0, (long long)w.n, "free resets n") ;
    T_ASSERT(w.fifos == NULL, "fifo sources freed") ;
    T_ASSERT(w.slots == NULL, "slots freed") ;
    /* all fifos unlinked */
    for (size_t i = 0 ; i < N ; i++) T_ASSERT_EQ(0, dir_entries(dirs[i]), "fifo unlinked") ;

    free_dirs(N, dirs) ; rm_rf(base) ;
}

static void test_wait_partial_timeout(void)
{
    char tmpl[] = "/tmp/ev_wp_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    enum { N = 3 } ;
    char *dirs[N] ; make_dirs(base, N, dirs) ;

    event_wait_t w ;
    T_ASSERT_EQ(1, event_wait_init(&w, (char const *const *)dirs, N, EVENT_READY), "init") ;

    /* only 2 of 3 trigger -> deadline must fire, run returns 0 */
    T_ASSERT_EQ(1, fanout_write(dirs[0], event_to_byte(EVENT_READY)), "trigger 0") ;
    T_ASSERT_EQ(1, fanout_write(dirs[2], event_to_byte(EVENT_READY)), "trigger 2") ;

    int r = event_wait_run(&w, 300) ;
    T_ASSERT_EQ(0, r, "run returns 0 on partial timeout") ;
    T_ASSERT_EQ(2, (long long)w.triggered, "triggered == k (2)") ;

    event_wait_free(&w) ;
    free_dirs(N, dirs) ; rm_rf(base) ;
}

static void test_wait_duplicate_byte_idempotent(void)
{
    char tmpl[] = "/tmp/ev_wd_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    enum { N = 2 } ;
    char *dirs[N] ; make_dirs(base, N, dirs) ;

    event_wait_t w ;
    T_ASSERT_EQ(1, event_wait_init(&w, (char const *const *)dirs, N, EVENT_READY), "init") ;

    /* dir0 emits the wanted byte THREE times; dir1 once.
     * got[] must dedupe: triggered must reach exactly 2, never more. */
    char b = event_to_byte(EVENT_READY) ;
    DIR *d = opendir(dirs[0]) ; T_ASSERT(d != NULL, "opendir dir0") ;
    struct dirent *e ; char path[1024] ;
    while ((e = readdir(d))) {
        if (strncmp(e->d_name, EVENT_FIFO_PREFIX, EVENT_FIFO_PREFIXLEN)) continue ;
        if (strlen(e->d_name) != EVENT_FIFO_NAMELEN) continue ;
        snprintf(path, sizeof(path), "%s/%s", dirs[0], e->d_name) ;
        int fd = open(path, O_WRONLY | O_NONBLOCK | O_CLOEXEC) ;
        T_ASSERT(fd >= 0, "open dir0 fifo") ;
        for (int k = 0 ; k < 3 ; k++) T_ASSERT_EQ(1, (long long)write(fd, &b, 1), "write dup") ;
        close(fd) ;
    }
    closedir(d) ;
    T_ASSERT_EQ(1, fanout_write(dirs[1], event_to_byte(EVENT_READY)), "trigger dir1 once") ;

    int r = event_wait_run(&w, 2000) ;
    T_ASSERT_EQ(1, r, "run returns 1") ;
    T_ASSERT_EQ(2, (long long)w.triggered, "duplicate bytes do NOT inflate triggered") ;

    event_wait_free(&w) ;
    free_dirs(N, dirs) ; rm_rf(base) ;
}

static void test_wait_irrelevant_bytes_ignored(void)
{
    char tmpl[] = "/tmp/ev_wn_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    enum { N = 2 } ;
    char *dirs[N] ; make_dirs(base, N, dirs) ;

    event_wait_t w ;
    T_ASSERT_EQ(1, event_wait_init(&w, (char const *const *)dirs, N, EVENT_READY), "init") ;

    /* dir0: non-satisfying transitions then the wanted state -> matches.
     * dir1: only bytes that never reach READY and never fail ('u'=up-not-ready,
     * 'd'=down, 's'=supervise-up which is irrelevant to a service-state wait) ->
     * stays pending. So triggered stays 1 and the wait times out (0). */
    T_ASSERT_EQ(1, fanout_write(dirs[0], event_to_byte(EVENT_UP)), "up-not-ready dir0") ;
    T_ASSERT_EQ(1, fanout_write(dirs[0], event_to_byte(EVENT_DOWN)), "down dir0") ;
    T_ASSERT_EQ(1, fanout_write(dirs[0], event_to_byte(EVENT_READY)), "ready dir0") ;
    T_ASSERT_EQ(1, fanout_write(dirs[1], event_to_byte(EVENT_UP)), "up-not-ready dir1") ;
    T_ASSERT_EQ(1, fanout_write(dirs[1], event_to_byte(EVENT_DOWN)), "down dir1") ;
    T_ASSERT_EQ(1, fanout_write(dirs[1], event_to_byte(EVENT_SUPERVISE_UP)), "irrelevant s dir1") ;

    int r = event_wait_run(&w, 300) ;
    T_ASSERT_EQ(0, r, "run times out: dir1 never reached READY") ;
    T_ASSERT_EQ(1, (long long)w.triggered, "only dir0 matched") ;
    T_ASSERT_EQ(0, w.failed, "no permanent-failure byte: not flagged failed") ;

    event_wait_free(&w) ;
    free_dirs(N, dirs) ; rm_rf(base) ;
}

/* A permanent-failure byte ('O' while waiting up, or 'x') ends a wait_and at
 * once -- like s6-svwait, which exits on such an event rather than waiting out
 * the deadline. Proven by both the `failed` flag AND the elapsed time being far
 * below the (generous) timeout. */
static void test_wait_permanent_failure_fast(void)
{
    char tmpl[] = "/tmp/ev_wf_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    enum { N = 2 } ;
    char *dirs[N] ; make_dirs(base, N, dirs) ;

    event_wait_t w ;
    T_ASSERT_EQ(1, event_wait_init(&w, (char const *const *)dirs, N, EVENT_READY), "init") ;

    /* dir1 will never come up; dir0 reports it won't be restarted -> the AND is
     * doomed and must fail immediately, not after the 5s deadline. */
    T_ASSERT_EQ(1, fanout_write(dirs[0], event_to_byte(EVENT_NORESTART)), "O dir0") ;

    struct timespec a, b ;
    clock_gettime(CLOCK_MONOTONIC, &a) ;
    int r = event_wait_run(&w, 5000) ;
    clock_gettime(CLOCK_MONOTONIC, &b) ;
    long ms = (b.tv_sec - a.tv_sec) * 1000 + (b.tv_nsec - a.tv_nsec) / 1000000 ;

    T_ASSERT_EQ(0, r, "run returns 0 on permanent failure") ;
    T_ASSERT_EQ(1, w.failed, "permanent failure flagged") ;
    T_ASSERT(ms < 1000, "failed fast, well under the 5s deadline") ;

    event_wait_free(&w) ;
    free_dirs(N, dirs) ; rm_rf(base) ;
}

static void test_wait_zero_dirs(void)
{
    event_wait_t w ;
    T_ASSERT_EQ(1, event_wait_init(&w, NULL, 0, EVENT_READY), "init n=0") ;
    T_ASSERT(w.fifos == NULL, "no fifo sources allocated for n=0") ;
    T_ASSERT(w.slots == NULL, "no slots allocated for n=0") ;
    int r = event_wait_run(&w, 1000) ;
    T_ASSERT_EQ(1, r, "run n=0 returns 1 immediately") ;
    event_wait_free(&w) ;  /* must not crash / leak */
}

static void test_wait_timeout_zero_no_timer(void)
{
    /* timeout_ms == 0: per the code, the timer is only armed for timeout_ms>0.
     * With one un-triggered reader, sse_poll runs with INFINITE internally but
     * no timer is set. To avoid an infinite block we trigger the reader so the
     * handler stops the loop; this confirms timeout_ms==0 arms NO timer (timer
     * stays inactive) yet the loop still completes via the match. */
    char tmpl[] = "/tmp/ev_wz_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    enum { N = 1 } ;
    char *dirs[N] ; make_dirs(base, N, dirs) ;

    event_wait_t w ;
    T_ASSERT_EQ(1, event_wait_init(&w, (char const *const *)dirs, N, EVENT_READY), "init") ;
    T_ASSERT_EQ(1, fanout_write(dirs[0], event_to_byte(EVENT_READY)), "trigger") ;

    int r = event_wait_run(&w, 0) ;
    T_ASSERT_EQ(1, r, "run returns 1 (match) with timeout 0") ;
    T_ASSERT_EQ(0, w.timer_active, "no timer armed when timeout_ms==0") ;

    event_wait_free(&w) ;
    free_dirs(N, dirs) ; rm_rf(base) ;
}

/* timeout_ms==0 arms NO timer: with an UN-triggered reader the run blocks
 * indefinitely (sse_poll INFINITE, no deadline). We prove the "no deadline"
 * behaviour in a child: with timeout 0 and no matching byte, the child must
 * still be running after a grace period (it is blocked). A mutation that arms
 * the timer for timeout_ms>=0 would make the child return 0 immediately, so the
 * child would have exited — which we detect and fail on. */
static void test_wait_timeout_zero_blocks(void)
{
    char tmpl[] = "/tmp/ev_wzb_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    enum { N = 1 } ;
    char *dirs[N] ; make_dirs(base, N, dirs) ;

    /* child: subscribe + run(timeout 0) with NO trigger; should block forever */
    pid_t pid = fork() ;
    T_ASSERT(pid >= 0, "fork") ;
    if (pid == 0) {
        alarm(0) ;   /* drop the inherited suite alarm; parent bounds us */
        event_wait_t cw ;
        if (!event_wait_init(&cw, (char const *const *)dirs, N, EVENT_READY)) _exit(2) ;
        /* if this returns at all with timeout 0 and no match, the timer fired */
        int rc = event_wait_run(&cw, 0) ;
        _exit(rc == 0 ? 50 : 51) ;   /* 50 = returned on a (bad) deadline */
    }

    /* give the child a grace period; correct code keeps it blocked */
    struct timespec ts = { 0, 350 * 1000000L } ; nanosleep(&ts, NULL) ;
    int st = 0 ;
    pid_t done = waitpid(pid, &st, WNOHANG) ;
    T_ASSERT_EQ(0, done, "child still blocked: timeout_ms==0 armed no deadline") ;

    /* clean up the blocked child */
    kill(pid, SIGKILL) ;
    waitpid(pid, &st, 0) ;
    free_dirs(N, dirs) ; rm_rf(base) ;
}

static void test_wait_init_rollback_on_bad_dir(void)
{
    /* dir1 is missing so subscribe of reader[1] fails -> rollback unsubscribes
     * reader[0], frees got+readers, frees epoll. LSan proves zero leak; and
     * reader[0]'s fifo in dir0 must have been unlinked by the rollback. */
    char tmpl[] = "/tmp/ev_wr_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char *dirs[2] ;
    dirs[0] = malloc(1024) ; snprintf(dirs[0], 1024, "%s/ev0", base) ;
    T_ASSERT_EQ(1, event_fifodir_make(dirs[0], (gid_t)-1), "make ev0") ;
    dirs[1] = malloc(1024) ; snprintf(dirs[1], 1024, "%s/ev_absent", base) ;  /* not created */

    event_wait_t w ;
    errno = 0 ;
    int r = event_wait_init(&w, (char const *const *)dirs, 2, EVENT_READY) ;
    T_ASSERT_EQ(0, r, "init returns 0 when a subscribe fails") ;
    T_ASSERT(w.fifos == NULL, "fifo sources freed on rollback") ;
    T_ASSERT(w.slots == NULL, "slots freed on rollback") ;
    /* reader[0]'s fifo was unlinked during rollback */
    T_ASSERT_EQ(0, dir_entries(dirs[0]), "rolled-back reader fifo unlinked") ;

    /* epoll was freed: w.epoll.fd should be -1 after sse_free */
    T_ASSERT_EQ(-1, w.epoll.fd, "epoll freed (fd -1)") ;

    rm_rf(dirs[0]) ; free(dirs[0]) ; free(dirs[1]) ; rm_rf(base) ;
}

/* ---- event_match (the shared transition interpreter) -------------- */

/* feed a NUL-terminated byte string through the matcher, return its verdict. */
static int m_feed(event_match_t *m, char const *s)
{
    return event_match_feed(m, s, strlen(s)) ;
}

static void test_match_up(void)
{
    event_match_t m ;
    event_match_init(&m, EVENT_UP, 0, 0) ;
    T_ASSERT_EQ(EVENT_MATCH_PENDING, m_feed(&m, "d"), "down byte: still pending for up") ;
    T_ASSERT_EQ(EVENT_MATCH_OK, m_feed(&m, "u"), "up byte: reached") ;
}

static void test_match_ready_needs_U(void)
{
    event_match_t m ;
    event_match_init(&m, EVENT_READY, 0, 0) ;
    T_ASSERT_EQ(EVENT_MATCH_PENDING, m_feed(&m, "u"), "up-not-ready: pending for ready") ;
    T_ASSERT_EQ(EVENT_MATCH_OK, m_feed(&m, "U"), "ready byte: reached") ;
}

static void test_match_down_from_up(void)
{
    event_match_t m ;
    event_match_init(&m, EVENT_DOWN, 1, 0) ;   /* seed: currently up */
    T_ASSERT_EQ(EVENT_MATCH_PENDING, m_feed(&m, ""), "seeded up: not down yet") ;
    T_ASSERT_EQ(EVENT_MATCH_OK, m_feed(&m, "d"), "down byte: reached") ;
}

static void test_match_down_ready(void)
{
    event_match_t m ;
    event_match_init(&m, EVENT_DOWN_READY, 1, 0) ;
    T_ASSERT_EQ(EVENT_MATCH_PENDING, m_feed(&m, "d"), "down-not-ready: pending") ;
    T_ASSERT_EQ(EVENT_MATCH_OK, m_feed(&m, "D"), "fully down: reached") ;
}

static void test_match_down_combo_one_feed(void)
{
    event_match_t m ;
    event_match_init(&m, EVENT_DOWN_READY, 1, 0) ;
    T_ASSERT_EQ(EVENT_MATCH_OK, m_feed(&m, "dD"), "d then D in one read: reached") ;
}

static void test_match_restart_two_phase(void)
{
    event_match_t m ;
    event_match_init(&m, EVENT_RESTART, 1, 0) ;   /* seed: up */
    T_ASSERT_EQ(EVENT_MATCH_PENDING, m_feed(&m, "u"), "still up, no down seen: pending") ;
    T_ASSERT_EQ(EVENT_MATCH_PENDING, m_feed(&m, "d"), "down phase reached, not up again: pending") ;
    T_ASSERT_EQ(EVENT_MATCH_OK, m_feed(&m, "u"), "up after down: restart reached") ;
}

static void test_match_already_satisfied_seed(void)
{
    event_match_t m ;
    event_match_init(&m, EVENT_UP, 1, 0) ;   /* already up */
    T_ASSERT_EQ(EVENT_MATCH_OK, m_feed(&m, ""), "seeded up: up wait already satisfied") ;
}

static void test_match_norestart_fail(void)
{
    event_match_t m ;
    event_match_init(&m, EVENT_READY, 0, 0) ;
    T_ASSERT_EQ(EVENT_MATCH_FAIL, m_feed(&m, "O"), "O while waiting up: permanent failure") ;
}

static void test_match_supervise_down_fail(void)
{
    event_match_t m ;
    event_match_init(&m, EVENT_READY, 0, 0) ;
    T_ASSERT_EQ(EVENT_MATCH_FAIL, m_feed(&m, "x"), "x while waiting ready: supervisor died") ;
}

static void test_match_norestart_ignored_when_down(void)
{
    event_match_t m ;
    event_match_init(&m, EVENT_DOWN, 1, 0) ;
    T_ASSERT_EQ(EVENT_MATCH_OK, m_feed(&m, "Od"), "O ignored when waiting down, then d: reached") ;
}

static void test_match_supervise_up(void)
{
    event_match_t m ;
    event_match_init(&m, EVENT_SUPERVISE_UP, 0, 0) ;
    T_ASSERT_EQ(EVENT_MATCH_PENDING, m_feed(&m, "uU"), "service bytes: pending for supervise-up") ;
    T_ASSERT_EQ(EVENT_MATCH_OK, m_feed(&m, "s"), "s byte: supervise-up reached") ;
}

static void test_match_supervise_down(void)
{
    event_match_t m ;
    event_match_init(&m, EVENT_SUPERVISE_DOWN, 0, 0) ;
    T_ASSERT_EQ(EVENT_MATCH_OK, m_feed(&m, "x"), "x byte: supervise-down reached (not a failure)") ;
}

/* ------------------------------------------------------------------ */

T_SUITE("event module")
{
    VERBOSITY = 0 ;       /* silence expected warn() on error paths */
    PROG = "test_event" ;
    signal(SIGALRM, on_alarm) ;
    alarm(60) ;           /* whole-suite anti-hang */

    /* fifodir_make */
    T_RUN(test_make_nogid_mode_01733) ;
    T_RUN(test_make_reapplies_mode) ;
    T_RUN(test_make_existing_not_dir_ENOTDIR) ;
    T_RUN(test_make_rejects_symlink) ;
    T_RUN(test_make_gid_path_chmod_branch) ;
    T_RUN(test_make_parent_missing_returns_0) ;

    /* fifodir_clean */
    T_RUN(test_clean_orphan_unlinked) ;
    T_RUN(test_clean_live_kept) ;
    T_RUN(test_clean_ignores_nonmatching) ;
    T_RUN(test_clean_mixed_orphan_and_live) ;
    T_RUN(test_clean_missing_dir_returns_0) ;

    /* fifodir_notify (producer fanout) */
    T_RUN(test_notify_delivers_to_subscriber) ;
    T_RUN(test_notify_sweeps_orphan_and_missing_dir) ;

    /* reader subscribe / the trick */
    T_RUN(test_subscribe_effects_and_mode) ;
    T_RUN(test_subscribe_trick_never_visible_without_reader) ;
    T_RUN(test_subscribe_trick_hidden_name) ;
    T_RUN(test_subscribe_trick_concurrent_smoke) ;
    T_RUN(test_subscribe_double_fd_no_eof) ;
    T_RUN(test_subscribe_nametoolong) ;
    T_RUN(test_subscribe_eventdir_missing_cleanup) ;

    /* reader_cb byte delivery */
    T_RUN(test_cb_single_byte) ;
    T_RUN(test_cb_multi_in_one_write) ;
    T_RUN(test_cb_large_batch_over_256) ;
    T_RUN(test_cb_multiple_writes) ;

    /* wait */
    T_RUN(test_wait_all_triggered) ;
    T_RUN(test_wait_partial_timeout) ;
    T_RUN(test_wait_duplicate_byte_idempotent) ;
    T_RUN(test_wait_irrelevant_bytes_ignored) ;
    T_RUN(test_wait_permanent_failure_fast) ;
    T_RUN(test_wait_zero_dirs) ;
    T_RUN(test_wait_timeout_zero_no_timer) ;
    T_RUN(test_wait_timeout_zero_blocks) ;
    T_RUN(test_wait_init_rollback_on_bad_dir) ;

    /* event_match (transition interpreter) */
    T_RUN(test_match_up) ;
    T_RUN(test_match_ready_needs_U) ;
    T_RUN(test_match_down_from_up) ;
    T_RUN(test_match_down_ready) ;
    T_RUN(test_match_down_combo_one_feed) ;
    T_RUN(test_match_restart_two_phase) ;
    T_RUN(test_match_already_satisfied_seed) ;
    T_RUN(test_match_norestart_fail) ;
    T_RUN(test_match_supervise_down_fail) ;
    T_RUN(test_match_norestart_ignored_when_down) ;
    T_RUN(test_match_supervise_up) ;
    T_RUN(test_match_supervise_down) ;
}
