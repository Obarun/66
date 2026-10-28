/*
 * service_graph_write.c
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

/** The write transaction of the system and its atomic writes, by effect: the
 * lock taken on <base>, the state of the process (nesting, dirty, another
 * system refused), the wait of a second writer and its message at verbosity 3,
 * <base>/.tmp emptied by the first begin, and every write leaving no
 * temporary behind; the end that gives the graph it built, in a child that
 * owns a user namespace and a chroot: SS_LIVE of the chroot, never the one of
 * the host. Everything happens under a private mkdtemp directory. */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/directory.h>
#include <oblibs/files.h>
#include <oblibs/io.h>
#include <oblibs/fd.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>

#include <66/symlink.h>
#include <66/constants.h>
#include <66/graph.h>
#include <66/utils.h>

static char root[64] = "/tmp/66-system-write.XXXXXX" ;

// the base and the cache of root in the chroot of test_end_graph()
#define BASE "/base/"
#define CACHE SS_LIVE SS_LIVEGRAPH "/0/" SS_LIVEGRAPH_FILE

static void path(char *dst, char const *base, char const *what)
{
    auto_strings(dst, base, what) ;
}

// the number of entries of @dir, . and .. aside
static int count_entries(char const *dir)
{
    int n = 0 ;
    DIR *d = opendir(dir) ;
    assert(d) ;
    struct dirent *de ;
    while ((de = readdir(d)))
        if (strcmp(de->d_name, ".") && strcmp(de->d_name, ".."))
            n++ ;
    closedir(d) ;
    return n ;
}

// the directories sanitize_system() makes at the first use of 66: the end builds the graph from them
static void system_make(char const *base)
{
    char dir[SS_MAX_PATH_LEN] ;
    path(dir, base, SS_SYSTEM SS_RESOLVE SS_SERVICE SS_PROVIDE) ;
    assert(dir_create_parent(dir, 0755)) ;
}

// no transaction is held any more: the lock was freed
static void released(void)
{
    errno = 0 ;
    assert(service_graph_write_end(0) == -1 && errno == EINVAL) ;
}

static void read_file(char const *file, char *buf, size_t n)
{
    ssize_t r = file_read(file, buf, n - 1) ;
    assert(r >= 0) ;
    buf[r] = 0 ;
}

static void test_begin_creates_and_empties(void)
{
    printf("begin creates <base> and empties <base>/.tmp\n") ;

    char base[SS_MAX_PATH_LEN], tmp[SS_MAX_PATH_LEN], left[SS_MAX_PATH_LEN] ;
    path(base, root, "/fresh/") ;
    path(tmp, base, SS_TMPDIR + 1) ;

    assert(access(base, F_OK) < 0) ;

    assert(service_graph_write_begin(getuid(), base, base) == 1) ;
    assert(count_entries(tmp) == 0) ;
    system_make(base) ;
    assert(service_graph_write_end(0) == 1) ;
    released() ;

    // what a killed writer left
    path(left, tmp, "/killed") ;
    assert(file_write(left, "x", 1)) ;
    path(left, tmp, "/killed.dir") ;
    assert(dir_create_parent(left, 0755)) ;
    assert(count_entries(tmp) == 2) ;

    assert(service_graph_write_begin(getuid(), base, base) == 1) ;
    assert(count_entries(tmp) == 0) ;

    // a nested begin does not empty it again
    path(left, tmp, "/inflight") ;
    assert(file_write(left, "x", 1)) ;
    assert(service_graph_write_begin(getuid(), base, base) == 1) ;
    assert(count_entries(tmp) == 1) ;
    assert(service_graph_write_end(0) == 1) ;
    assert(service_graph_write_end(0) == 1) ;
    released() ;
    file_tryunlink(left) ;
}

// a graph of one vertex, a, that the disk does not hold: the caller's old graph
static void held(service_graph_t *g)
{
    service_graph_vertex_t v = SERVICE_GRAPH_VERTEX_ZERO ;
    uint32_t sort = 0 ;

    service_graph_free(g) ;
    v.state = SERVICE_GRAPH_STATE_OK ;

    assert(strbuf_copyb(&g->strings, "a\0", 2)) ;
    assert(strbuf_copyb(&g->vertex, (char const *)&v, sizeof(v))) ;
    assert(strbuf_copyb(&g->sort, (char const *)&sort, sizeof(sort))) ;
}

static void test_end_without_hold(void)
{
    printf("end without a transaction: -1 EINVAL, the graph of the caller untouched\n") ;

    service_graph_t g = SERVICE_GRAPH_ZERO ;

    errno = 0 ;
    assert(service_graph_write_end(0) == -1) ;
    assert(errno == EINVAL) ;

    held(&g) ;
    errno = 0 ;
    assert(service_graph_write_end(&g) == -1 && errno == EINVAL) ;
    assert(service_graph_nvertex(&g) == 1) ;

    service_graph_free(&g) ;
}

static void test_other_system_refused(void)
{
    printf("a nested begin or try on another system is refused\n") ;

    char a[SS_MAX_PATH_LEN], b[SS_MAX_PATH_LEN] ;
    path(a, root, "/a/") ;
    path(b, root, "/b/") ;
    assert(dir_create_parent(b, 0755)) ;

    assert(service_graph_write_begin(getuid(), a, a) == 1) ;
    system_make(a) ;

    errno = 0 ;
    assert(service_graph_write_begin(getuid(), b, b) == -1 && errno == EDEADLK) ;
    errno = 0 ;
    assert(service_graph_write_try(getuid(), b, b) == -1 && errno == EDEADLK) ;
    errno = 0 ;
    assert(service_graph_write_begin(getuid() + 1, a, a) == -1 && errno == EDEADLK) ;

    // the refusals left the transaction as it was: one level
    assert(service_graph_write_end(0) == 1) ;
    released() ;
}

static void test_try(void)
{
    printf("try: absent base, hold, nesting, a begin nested\n") ;

    char absent[SS_MAX_PATH_LEN], base[SS_MAX_PATH_LEN], tmp[SS_MAX_PATH_LEN], left[SS_MAX_PATH_LEN] ;
    path(absent, root, "/absent/") ;
    path(base, root, "/try/") ;
    path(tmp, base, SS_TMPDIR + 1) ;

    // nothing to protect, nothing created
    assert(service_graph_write_try(getuid(), absent, absent) == 0) ;
    assert(access(absent, F_OK) < 0) ;
    errno = 0 ;
    assert(service_graph_write_end(0) == -1 && errno == EINVAL) ;

    assert(dir_create_parent(tmp, 0755)) ;
    system_make(base) ;
    path(left, tmp, "/killed") ;
    assert(file_write(left, "x", 1)) ;

    // a hold that does not write: nothing emptied
    assert(service_graph_write_try(getuid(), base, base) == 1) ;
    assert(count_entries(tmp) == 1) ;

    // a try nested in the hold only nests: nothing emptied either
    assert(service_graph_write_try(getuid(), base, base) == 1) ;
    assert(count_entries(tmp) == 1) ;
    assert(service_graph_write_end(0) == 1) ;

    // a begin nested in the hold leaves <base>/.tmp to the first begin
    assert(service_graph_write_begin(getuid(), base, base) == 1) ;
    assert(count_entries(tmp) == 1) ;
    assert(service_graph_write_end(0) == 1) ;
    assert(service_graph_write_end(0) == 1) ;
    released() ;
}

// a child takes the lock of @base, says so on @p, holds it @hold_ms, then exits
static pid_t hold_lock(char const *base, int const p[2], unsigned int hold_ms)
{
    pid_t pid = fork() ;
    assert(pid >= 0) ;

    if (!pid) {
        char c = 'x' ;
        int fd = (int)io_open(base, O_RDONLY | O_DIRECTORY | O_CLOEXEC) ;
        if (fd < 0 || lock_fd(fd, 1, 0) != 1)
            _exit(1) ;
        if (io_write(p[1], &c, 1) != 1)
            _exit(1) ;
        struct timespec t = { hold_ms / 1000, (long)(hold_ms % 1000) * 1000000L } ;
        nanosleep(&t, 0) ;
        _exit(0) ;
    }

    char c ;
    assert(io_read(p[0], &c, 1) == 1) ;

    return pid ;
}

static void test_contention(void)
{
    printf("a second writer waits and says so at verbosity 3, a reader does not wait\n") ;

    char base[SS_MAX_PATH_LEN], log[SS_MAX_PATH_LEN] ;
    path(base, root, "/busy/") ;
    system_make(base) ;
    path(log, root, "/stderr") ;
    assert(dir_create_parent(base, 0755)) ;

    int p[2] ;
    assert(!pipe(p)) ;

    // a reader never waits: the lock is busy, try gives 0 at once
    pid_t pid = hold_lock(base, p, 300) ;
    assert(service_graph_write_try(getuid(), base, base) == 0) ;
    released() ;
    assert(waitpid(pid, 0, 0) == pid) ;

    // a writer under a lock held 1.5 s: it waits, and says so once past a second
    int saved = dup(2) ;
    assert(saved >= 0) ;
    int out = (int)io_open_mode(log, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600) ;
    assert(out >= 0) ;
    unsigned int verbosity = VERBOSITY ;
    VERBOSITY = 3 ;

    pid = hold_lock(base, p, 1500) ;
    struct timespec a, b ;
    clock_gettime(CLOCK_MONOTONIC, &a) ;
    assert(dup2(out, 2) == 2) ;
    int r = service_graph_write_begin(getuid(), base, base) ;
    assert(dup2(saved, 2) == 2) ;
    clock_gettime(CLOCK_MONOTONIC, &b) ;
    VERBOSITY = verbosity ;
    close_fd(out) ;
    close_fd(saved) ;

    assert(r == 1) ;
    assert(waitpid(pid, 0, 0) == pid) ;
    double waited = (double)(b.tv_sec - a.tv_sec) + (double)(b.tv_nsec - a.tv_nsec) / 1e9 ;
    assert(waited >= 1.0) ;
    assert(service_graph_write_end(0) == 1) ;

    char buf[4096] ;
    read_file(log, buf, sizeof buf) ;
    assert(strstr(buf, "waiting for another 66 command to release the lock of the system")) ;

    // under a lock held less than a second, the wait is silent
    out = (int)io_open_mode(log, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600) ;
    saved = dup(2) ;
    VERBOSITY = 3 ;
    pid = hold_lock(base, p, 200) ;
    assert(dup2(out, 2) == 2) ;
    r = service_graph_write_begin(getuid(), base, base) ;
    assert(dup2(saved, 2) == 2) ;
    VERBOSITY = verbosity ;
    close_fd(out) ;
    close_fd(saved) ;
    assert(r == 1) ;
    assert(waitpid(pid, 0, 0) == pid) ;
    assert(service_graph_write_end(0) == 1) ;
    read_file(log, buf, sizeof buf) ;
    assert(!strstr(buf, "waiting for another 66 command")) ;

    close_fd(p[0]) ;
    close_fd(p[1]) ;
}

static void test_lock_released_on_exec(void)
{
    printf("the lock descriptor is O_CLOEXEC\n") ;

    char base[SS_MAX_PATH_LEN] ;
    path(base, root, "/cloexec/") ;
    system_make(base) ;

    assert(service_graph_write_begin(getuid(), base, base) == 1) ;

    int found = 0 ;
    for (int fd = 3 ; fd < 1024 ; fd++) {
        int flags = fcntl(fd, F_GETFD) ;
        if (flags < 0)
            continue ;
        char link[64], target[SS_MAX_PATH_LEN] ;
        snprintf(link, sizeof link, "/proc/self/fd/%d", fd) ;
        ssize_t n = readlink(link, target, sizeof target - 1) ;
        if (n <= 0)
            continue ;
        target[n] = 0 ;
        strcat(target, "/") ;
        if (!strcmp(target, base)) {
            assert(flags & FD_CLOEXEC) ;
            found = 1 ;
        }
    }
    assert(found) ;
    assert(service_graph_write_end(0) == 1) ;
}

static void test_writes(void)
{
    printf("atomic writes leave no temporary behind\n") ;

    char base[SS_MAX_PATH_LEN], sys[SS_MAX_PATH_LEN], tmp[SS_MAX_PATH_LEN] ;
    char file[SS_MAX_PATH_LEN], buf[256] ;
    path(base, root, "/writes/") ;
    path(sys, base, SS_SYSTEM) ;
    path(tmp, base, SS_TMPDIR + 1) ;
    // the system exists before its first write, sanitize_system() made it
    system_make(base) ;

    assert(service_graph_write_begin(getuid(), base, base) == 1) ;

    // a file of the system written through .tmp: created, then replaced as a whole
    path(file, sys, "/.version") ;
    assert(file_write_atomic_at(tmp, file, "0.9.1.2", 7) == 1) ;
    read_file(file, buf, sizeof buf) ;
    assert(!strcmp(buf, "0.9.1.2")) ;
    assert(file_write_atomic_at(tmp, file, "0.9.2.0", 7) == 1) ;
    read_file(file, buf, sizeof buf) ;
    assert(!strcmp(buf, "0.9.2.0")) ;
    assert(count_entries(tmp) == 0) ;

    // a file whose directory is missing: refused, nothing left
    path(file, sys, "/nodir/file") ;
    assert(file_write_atomic_at(tmp, file, "x", 1) == 0) ;
    assert(count_entries(tmp) == 0) ;

    // a link of the system made through .tmp, then replaced
    char link[SS_MAX_PATH_LEN] ;
    path(link, sys, "/link") ;
    assert(symlink_atomic_at(tmp, "/first", link) == 1) ;
    ssize_t n = readlink(link, buf, sizeof buf - 1) ;
    assert(n > 0) ;
    buf[n] = 0 ;
    assert(!strcmp(buf, "/first")) ;
    assert(symlink_atomic_at(tmp, "/second", link) == 1) ;
    n = readlink(link, buf, sizeof buf - 1) ;
    assert(n > 0) ;
    buf[n] = 0 ;
    assert(!strcmp(buf, "/second")) ;
    assert(count_entries(tmp) == 0) ;

    assert(service_graph_write_end(0) == 1) ;
}

// SS_TMPDIR_AT(): the one composition of the temporary directory
static void test_tmpdir(void)
{
    printf("the temporary directory composed\n") ;

    char dir[SS_MAX_PATH_LEN], tmp[TMPDIR_GETLEN(strlen(root)) + 1] ;

    auto_strings(tmp, root, SS_TMPDIR + 1) ;
    path(dir, root, SS_TMPDIR + 1) ;
    assert(!strcmp(tmp, dir) && strlen(tmp) == TMPDIR_GETLEN(strlen(root))) ;
}

static int cached(void)
{
    struct stat st ;

    return !lstat(CACHE, &st) ;
}

// the cache read back is the system on disk: empty
static void cache_is_the_system(void)
{
    service_graph_t r = SERVICE_GRAPH_ZERO ;

    assert(service_graph_cache_read(&r, 0, BASE, SS_LIVE) == 1 && !service_graph_nvertex(&r)) ;
    service_graph_free(&r) ;
}

static void test_end_graph_last(void)
{
    printf("the last end of a transaction that writes: the graph of the disk given, the cache written, the lock freed\n") ;

    service_graph_t g = SERVICE_GRAPH_ZERO ;

    held(&g) ;
    assert(service_graph_write_begin(0, BASE, SS_LIVE) == 1 && !cached()) ;
    assert(service_graph_write_end(&g) == 1) ;
    // the graph given is the empty system, not the one held
    assert(!service_graph_nvertex(&g)) ;
    assert(cached()) ;
    cache_is_the_system() ;
    released() ;

    service_graph_free(&g) ;
}

static void test_end_graph_nested(void)
{
    printf("a nested end of a transaction that writes: the graph given, the cache left to the last end\n") ;

    service_graph_t g = SERVICE_GRAPH_ZERO ;

    held(&g) ;
    assert(service_graph_write_begin(0, BASE, SS_LIVE) == 1 && service_graph_write_begin(0, BASE, SS_LIVE) == 1) ;
    assert(service_graph_write_end(&g) == 1 && !service_graph_nvertex(&g)) ;
    assert(!cached()) ;
    // the transaction still held: its last end writes the cache
    assert(service_graph_write_end(0) == 1 && cached()) ;
    released() ;

    // a try nested in a transaction that writes is a level of it
    held(&g) ;
    assert(service_graph_write_begin(0, BASE, SS_LIVE) == 1 && service_graph_write_try(0, BASE, SS_LIVE) == 1) ;
    assert(service_graph_write_end(&g) == 1 && !service_graph_nvertex(&g)) ;
    assert(!cached()) ;
    assert(service_graph_write_end(0) == 1 && cached()) ;
    released() ;

    service_graph_free(&g) ;
}

static void test_end_graph_try(void)
{
    printf("a hold of service_graph_write_try(): the graph given, the cache written by the last end\n") ;

    service_graph_t g = SERVICE_GRAPH_ZERO ;

    held(&g) ;
    assert(service_graph_cache_drop(0, SS_LIVE) == 1 && !cached()) ;
    assert(service_graph_write_try(0, BASE, SS_LIVE) == 1) ;
    assert(service_graph_write_end(&g) == 1 && !service_graph_nvertex(&g)) ;
    assert(cached()) ;
    cache_is_the_system() ;
    released() ;

    held(&g) ;
    assert(service_graph_cache_drop(0, SS_LIVE) == 1 && !cached()) ;
    assert(service_graph_write_try(0, BASE, SS_LIVE) == 1 && service_graph_write_try(0, BASE, SS_LIVE) == 1) ;
    assert(service_graph_write_end(&g) == 1 && !service_graph_nvertex(&g)) ;
    // only the last end writes the cache
    assert(!cached()) ;
    assert(service_graph_write_end(0) == 1 && cached()) ;
    released() ;

    // without @g, the end of a hold builds the cache as well
    assert(service_graph_cache_drop(0, SS_LIVE) == 1 && !cached()) ;
    assert(service_graph_write_try(0, BASE, SS_LIVE) == 1) ;
    assert(service_graph_write_end(0) == 1 && cached()) ;
    released() ;

    // a begin nested in the hold drops the cache, written by the last end
    assert(service_graph_write_try(0, BASE, SS_LIVE) == 1 && service_graph_write_end(&g) == 1 && cached()) ;
    assert(service_graph_write_try(0, BASE, SS_LIVE) == 1 && service_graph_write_begin(0, BASE, SS_LIVE) == 1 && !cached()) ;
    assert(service_graph_write_end(0) == 1 && !cached()) ;
    assert(service_graph_write_end(0) == 1 && cached()) ;
    released() ;

    service_graph_free(&g) ;
}

static void test_end_graph_reload(void)
{
    printf("service_graph_reload(): the graph of the disk, the cache written when the lock is free, left to the end inside a transaction\n") ;

    service_graph_t g = SERVICE_GRAPH_ZERO ;

    held(&g) ;
    assert(service_graph_cache_drop(0, SS_LIVE) == 1 && !cached()) ;
    assert(service_graph_reload(&g, 0, BASE, SS_LIVE) == 1 && !service_graph_nvertex(&g)) ;
    assert(cached()) ;
    released() ;

    held(&g) ;
    assert(service_graph_write_begin(0, BASE, SS_LIVE) == 1) ;
    assert(service_graph_reload(&g, 0, BASE, SS_LIVE) == 1 && !service_graph_nvertex(&g)) ;
    // the cache left to the end of the transaction, still held
    assert(!cached()) ;
    assert(service_graph_write_end(0) == 1 && cached()) ;
    released() ;

    service_graph_free(&g) ;
}

// the end that gives the graph, in a child mapped to root and chrooted in root
static void test_end_graph(void)
{
    int status = 0 ;

    fflush(stdout) ;
    pid_t pid = fork() ;
    assert(pid >= 0) ;

    if (!pid) {

        char map[64] ;
        uid_t uid = getuid() ;
        gid_t gid = getgid() ;

        assert(unshare(CLONE_NEWUSER) == 0) ;
        assert(file_write("/proc/self/setgroups", "deny", 4)) ;
        sprintf(map, "0 %u 1", (unsigned)uid) ;
        assert(file_write("/proc/self/uid_map", map, strlen(map))) ;
        sprintf(map, "0 %u 1", (unsigned)gid) ;
        assert(file_write("/proc/self/gid_map", map, strlen(map))) ;
        assert(chroot(root) == 0 && chdir("/") == 0) ;

        system_make(BASE) ;
        assert(dir_create_parent(SS_LIVE, 0755)) ;

        test_end_graph_last() ;
        test_end_graph_nested() ;
        test_end_graph_try() ;
        test_end_graph_reload() ;

        fflush(stdout) ;
        _exit(0) ;
    }

    assert(waitpid(pid, &status, 0) == pid) ;
    assert(WIFEXITED(status) && !WEXITSTATUS(status)) ;
}

int main(void)
{
    printf("Starting system write test...\n") ;

    // the message arrays of the log, which a test of a message reads
    PROG = "test" ;
    log_color_init() ;
    VERBOSITY = 0 ;
    assert(mkdtemp(root)) ;

    test_end_without_hold() ;
    test_begin_creates_and_empties() ;
    test_other_system_refused() ;
    test_try() ;
    test_contention() ;
    test_lock_released_on_exec() ;
    test_writes() ;
    test_tmpdir() ;
    test_end_graph() ;

    assert(dir_destroy(root)) ;

    printf("All tests passed!\n") ;
    return 0 ;
}
