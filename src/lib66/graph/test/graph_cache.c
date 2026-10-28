/*
 * graph_cache.c
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

/** The cache of the graph, by effect, in a child that owns a user namespace
 * and a chroot: SS_LIVE of the chroot, never the one of the host. A graph made
 * by hand is written and read back; each bound of the reading is shown a
 * cache that breaks it alone, and what is derived comes back derived; the directories, their owner and mode, the
 * format and the identity of the base; then the load and the write
 * transaction: a begin drops the cache, an end builds it again, a reader in a
 * transaction that writes leaves it to the end. */

#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/directory.h>
#include <oblibs/files.h>
#include <oblibs/strbuf.h>
#include <oblibs/cdb.h>
#include <oblibs/fd.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/graph.h>

#define CHECK(cond, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__) ; \
        fprintf(stderr, __VA_ARGS__) ; \
        fprintf(stderr, "\n") ; \
        abort() ; \
    } \
} while (0)

#define BASE "/base/"
#define OTHER "/other/"
#define LIVE "/live/" // a live directory of the caller's own, beside SS_LIVE
#define GRAPHDIR SS_LIVE SS_LIVEGRAPH
#define ROOTDIR GRAPHDIR "/0"
#define CACHE ROOTDIR "/" SS_LIVEGRAPH_FILE

#define D SERVICE_GRAPH_EDGE_DECLARED
#define C SERVICE_GRAPH_EDGE_CONTENTS
#define R SERVICE_GRAPH_EDGE_REQUIREDBY

/**
 *
 * A graph made by hand: a, b, c, d sorted by name; b depends on a, c on a
 * (declared in the RequiredBy of a) and on b (c is a module, b its member), d
 * absent; the start order a d b c, as Kahn gives it.
 *
 * */

static void vertex(service_graph_t *g, uint32_t name, uint32_t depends, uint32_t ndepends, uint32_t requiredby, uint32_t nrequiredby, uint8_t state)
{
    service_graph_vertex_t v = SERVICE_GRAPH_VERTEX_ZERO ;

    v.name = name ;
    v.depends = depends ;
    v.ndepends = ndepends ;
    v.requiredby = requiredby ;
    v.nrequiredby = nrequiredby ;
    v.state = state ;

    CHECK(strbuf_catb(&g->vertex, (char const *)&v, sizeof(v)), "vertex") ;
}

static void u32s(strbuf *sb, uint32_t const *v, size_t n)
{
    CHECK(strbuf_copyb(sb, (char const *)v, n * sizeof(uint32_t)), "block") ;
}

static void make(service_graph_t *g)
{
    service_graph_free(g) ;

    CHECK(strbuf_copyb(&g->strings, "a\0b\0c\0d\0", 8), "strings") ;
    vertex(g, 0, 0, 0, 0, 2, SERVICE_GRAPH_STATE_OK) ;
    vertex(g, 2, 0, 1, 2, 1, SERVICE_GRAPH_STATE_OK) ;
    vertex(g, 4, 1, 2, 3, 0, SERVICE_GRAPH_STATE_OK) ;
    vertex(g, 6, 3, 0, 3, 0, SERVICE_GRAPH_STATE_ABSENT) ;
    u32s(&g->depends, (uint32_t[]){ 0, 0, 1 }, 3) ;
    CHECK(strbuf_copyb(&g->dorigin, (char[]){ D, D | R, C }, 3), "dorigin") ;
    u32s(&g->requiredby, (uint32_t[]){ 1, 2, 2 }, 3) ;
    CHECK(strbuf_copyb(&g->rorigin, (char[]){ D, D | R, C }, 3), "rorigin") ;
    u32s(&g->sort, (uint32_t[]){ 0, 3, 1, 2 }, 4) ;
}

static service_graph_vertex_t *vx(service_graph_t *g, uint32_t index)
{
    return (service_graph_vertex_t *)g->vertex.s + index ;
}

static void same(service_graph_t const *a, service_graph_t const *b, char const *what)
{
    CHECK(service_graph_nvertex(a) == service_graph_nvertex(b) && service_graph_nsort(a) == service_graph_nsort(b), "%s: same counts", what) ;

    for (uint32_t i = 0 ; i < service_graph_nvertex(a) ; i++) {

        service_graph_vertex_t const *va = service_graph_vertex(a, i), *vb = service_graph_vertex(b, i) ;

        CHECK(!strcmp(service_graph_name(a, i), service_graph_name(b, i)), "%s: same name at %u", what, i) ;
        CHECK(va->state == vb->state && va->depends == vb->depends && va->ndepends == vb->ndepends &&
            va->requiredby == vb->requiredby && va->nrequiredby == vb->nrequiredby, "%s: same vertex %u", what, i) ;
        CHECK(!memcmp(service_graph_depends(a, i), service_graph_depends(b, i), va->ndepends * sizeof(uint32_t)) &&
            !memcmp(service_graph_dorigin(a, i), service_graph_dorigin(b, i), va->ndepends) &&
            !memcmp(service_graph_requiredby(a, i), service_graph_requiredby(b, i), va->nrequiredby * sizeof(uint32_t)) &&
            !memcmp(service_graph_rorigin(a, i), service_graph_rorigin(b, i), va->nrequiredby), "%s: same edges of %u", what, i) ;
    }

    for (uint32_t pos = 0 ; pos < service_graph_nsort(a) ; pos++)
        CHECK(service_graph_sort(a, pos) == service_graph_sort(b, pos), "%s: same order at %u", what, pos) ;
}

// @g written as the cache of root, then read: 1 and the same graph, or 0 with @err
static void roundtrip(service_graph_t const *g, int expect, int err, char const *what)
{
    service_graph_t r = SERVICE_GRAPH_ZERO ;

    CHECK(service_graph_cache_write(g, 0, BASE, SS_LIVE) == 1, "%s: write: errno %d", what, errno) ;

    errno = 0 ;
    int got = service_graph_cache_read(&r, 0, BASE, SS_LIVE) ;

    CHECK(got == expect, "%s: read gave %d, expected %d (errno %d)", what, got, expect, errno) ;

    if (expect) {
        same(&r, g, what) ;
    } else {
        CHECK(errno == err, "%s: errno %d, expected %d", what, errno, err) ;
        CHECK(!r.vertex.s && !r.strings.s && !r.sort.s, "%s: the graph left empty", what) ;
    }

    service_graph_free(&r) ;
}

// a cdb at CACHE of the blocks of @g under the key format of @format, read: @expect, EINVAL on 0
static void forged_format(service_graph_t const *g, char const *format, uint32_t len, int expect, char const *what)
{
    char tmp[] = ROOTDIR "/.forgedXXXXXX" ;
    char const *key[SERVICE_GRAPH_CACHE_NBLOCK] = SERVICE_GRAPH_CACHE_KEYS ;
    strbuf const *block[SERVICE_GRAPH_CACHE_NBLOCK] = SERVICE_GRAPH_CACHE_BLOCKS(g) ;
    ocdbmaker c = OCDBMAKER_ZERO ;
    service_graph_t r = SERVICE_GRAPH_ZERO ;
    int fd = mkstemp(tmp) ;

    CHECK(fd >= 0 && ocdb_make_start(&c, fd), "%s: start the cdb", what) ;
    if (format)
        CHECK(ocdb_make_add(&c, SERVICE_GRAPH_CACHE_KEY_FORMAT, sizeof SERVICE_GRAPH_CACHE_KEY_FORMAT - 1, format, len), "%s: format", what) ;
    for (uint32_t i = 0 ; i < SERVICE_GRAPH_CACHE_NBLOCK ; i++)
        CHECK(ocdb_make_add(&c, key[i], (uint32_t)strlen(key[i]), block[i]->s, (uint32_t)block[i]->len), "%s: block", what) ;
    CHECK(ocdb_make_finish(&c), "%s: finish", what) ;
    close_fd(fd) ;
    CHECK(rename(tmp, CACHE) == 0, "%s: rename", what) ;

    errno = 0 ;
    int got = service_graph_cache_read(&r, 0, BASE, SS_LIVE) ;

    CHECK(got == expect && (expect || errno == EINVAL), "%s: read gave %d, expected %d, errno %d", what, got, expect, errno) ;

    if (expect)
        same(&r, g, what) ;

    service_graph_free(&r) ;
}

static void test_roundtrip(void)
{
    printf("written then read: the same graph, the empty one included; the directories made by root\n") ;

    service_graph_t g = SERVICE_GRAPH_ZERO, empty = SERVICE_GRAPH_ZERO ;
    struct stat st ;

    make(&g) ;
    roundtrip(&g, 1, 0, "the graph made by hand") ;
    roundtrip(&empty, 1, 0, "the empty graph") ;

    CHECK(!lstat(GRAPHDIR, &st) && S_ISDIR(st.st_mode) && !st.st_uid && (st.st_mode & 07777) == 01777, "graph: root, 1777, mode %o", st.st_mode & 07777) ;
    CHECK(!lstat(ROOTDIR, &st) && S_ISDIR(st.st_mode) && !st.st_uid && (st.st_mode & 07777) == 0755, "graph/0: root, 0755, mode %o", st.st_mode & 07777) ;
    CHECK(!lstat(CACHE, &st) && S_ISREG(st.st_mode) && !st.st_uid, "graph/0/system: a regular file of root") ;

    service_graph_free(&g) ;
}

static void test_checks(void)
{
    printf("each bound of the reading refuses a cache that breaks it alone: 0, EINVAL; the rest is derived again\n") ;

    service_graph_t g = SERVICE_GRAPH_ZERO, fresh = SERVICE_GRAPH_ZERO, r = SERVICE_GRAPH_ZERO ;

    // 1. the lengths
    make(&g) ; CHECK(strbuf_catb(&g.vertex, "x", 1), "grow") ;
    roundtrip(&g, 0, EINVAL, "a vertex block of a broken length") ;
    make(&g) ; g.dorigin.len-- ;
    roundtrip(&g, 0, EINVAL, "an origin short") ;

    // 2. the names
    make(&g) ; g.strings.s[g.strings.len - 1] = 'x' ;
    roundtrip(&g, 0, EINVAL, "names not zero ended") ;
    make(&g) ; vx(&g, 3)->name = (uint32_t)g.strings.len ;
    roundtrip(&g, 0, EINVAL, "a name outside its block") ;

    // 3. the edges
    make(&g) ; ((uint32_t *)g.depends.s)[2] = 4 ;
    roundtrip(&g, 0, EINVAL, "an edge outside the graph") ;
    make(&g) ; vx(&g, 2)->ndepends = 3 ;
    roundtrip(&g, 0, EINVAL, "more ndepends than edges") ;
    make(&g) ; vx(&g, 2)->ndepends = 1 ;
    roundtrip(&g, 0, EINVAL, "fewer ndepends than edges") ;
    make(&g) ; vx(&g, 0)->ndepends = UINT32_MAX ; vx(&g, 3)->ndepends = 1 ;
    roundtrip(&g, 0, EINVAL, "ndepends that add up to the edges past a wrap") ;

    // 4. what is derived is never read: written wrong, it comes back as the build makes it
    make(&fresh) ;
    make(&g) ; vx(&g, 2)->depends = 0 ; vx(&g, 0)->requiredby = 3 ; vx(&g, 1)->nrequiredby = 0 ; vx(&g, 0)->state = SERVICE_GRAPH_STATE_UNORDERED ;
    u32s(&g.requiredby, (uint32_t[]){ 3, 3, 3 }, 3) ; g.rorigin.s[2] = D ; u32s(&g.sort, (uint32_t[]){ 3, 2, 1, 0 }, 4) ;
    CHECK(service_graph_cache_write(&g, 0, BASE, SS_LIVE) == 1, "write the derived blocks wrong") ;
    CHECK(service_graph_cache_read(&r, 0, BASE, SS_LIVE) == 1, "read") ;
    same(&r, &fresh, "the derived blocks derived again") ;
    service_graph_free(&r) ;

    // a vertex that depends on itself, written behind 66: a cycle the build keeps
    make(&g) ; vx(&g, 3)->ndepends = 1 ; vx(&g, 3)->nrequiredby = 1 ; vx(&g, 3)->state = SERVICE_GRAPH_STATE_UNORDERED ;
    u32s(&g.depends, (uint32_t[]){ 0, 0, 1, 3 }, 4) ; CHECK(strbuf_catb(&g.dorigin, (char[]){ D }, 1), "dorigin") ;
    u32s(&g.requiredby, (uint32_t[]){ 1, 2, 2, 3 }, 4) ; CHECK(strbuf_catb(&g.rorigin, (char[]){ D }, 1), "rorigin") ;
    u32s(&g.sort, (uint32_t[]){ 0, 1, 2 }, 3) ;
    roundtrip(&g, 1, 0, "a vertex on a cycle of its own") ;

    service_graph_free(&g) ;
    service_graph_free(&fresh) ;
}

static void test_format_base(void)
{
    printf("another format, another base, no format: absent for the reader\n") ;

    service_graph_t g = SERVICE_GRAPH_ZERO, r = SERVICE_GRAPH_ZERO ;
    char format[SERVICE_GRAPH_CACHE_FORMAT_LEN] ;
    struct stat st ;
    uint32_t old = SERVICE_GRAPH_CACHE_FORMAT - 1 ;

    make(&g) ;
    CHECK(!stat(BASE, &st), "stat the base") ;

    service_graph_cache_format(format, &st) ;
    memcpy(format, &old, sizeof(old)) ;
    forged_format(&g, format, sizeof(format), 0, "the format before") ;
    forged_format(&g, 0, 0, 0, "no format") ;
    service_graph_cache_format(format, &st) ;
    forged_format(&g, format, sizeof(format) - 1, 0, "a format cut") ;

    // the premise: forged with the right format, it reads
    forged_format(&g, format, sizeof(format), 1, "the right format") ;

    CHECK(service_graph_cache_write(&g, 0, BASE, SS_LIVE) == 1, "write") ;
    errno = 0 ;
    CHECK(service_graph_cache_read(&r, 0, OTHER, SS_LIVE) == 0 && errno == EINVAL, "another base: refused, errno %d", errno) ;
    CHECK(service_graph_cache_read(&r, 0, BASE, SS_LIVE) == 1, "its own base: read") ;
    service_graph_free(&r) ;

    service_graph_free(&g) ;
}

static void test_place(void)
{
    printf("the place: a link for the file, a directory of another owner or open to others, the drop\n") ;

    service_graph_t g = SERVICE_GRAPH_ZERO, r = SERVICE_GRAPH_ZERO ;
    struct stat st ;

    make(&g) ;
    CHECK(service_graph_cache_write(&g, 0, BASE, SS_LIVE) == 1, "write") ;

    // a link in place of the file, toward a cache that reads
    CHECK(rename(CACHE, ROOTDIR "/real") == 0 && symlink("real", CACHE) == 0, "link the cache") ;
    CHECK(service_graph_cache_read(&r, 0, BASE, SS_LIVE) == 0, "a link: no cache") ;
    CHECK(unlink(CACHE) == 0 && rename(ROOTDIR "/real", CACHE) == 0, "unlink") ;
    CHECK(service_graph_cache_read(&r, 0, BASE, SS_LIVE) == 1, "the premise: the file itself reads") ;
    service_graph_free(&r) ;

    // the directory of another owner: 1000 made by root, holding a cache of root that reads
    CHECK(dir_create_parent(GRAPHDIR "/1000", 0755), "make graph/1000") ;
    CHECK(link(CACHE, GRAPHDIR "/1000/" SS_LIVEGRAPH_FILE) == 0, "the cache of root in it") ;
    errno = 0 ;
    CHECK(service_graph_cache_read(&r, 1000, BASE, SS_LIVE) == 0 && errno == EINVAL, "a directory of root for 1000: refused, errno %d", errno) ;
    CHECK(service_graph_cache_write(&g, 1000, BASE, SS_LIVE) == 0, "not the process of 1000: not written") ;
    CHECK(service_graph_cache_drop(1000, SS_LIVE) == 1, "drop: nothing of 1000 there") ;
    CHECK(!lstat(GRAPHDIR "/1000/" SS_LIVEGRAPH_FILE, &st), "the file of root left in place") ;

    // a directory others can write
    CHECK(chmod(ROOTDIR, 0777) == 0, "open graph/0") ;
    errno = 0 ;
    CHECK(service_graph_cache_read(&r, 0, BASE, SS_LIVE) == 0 && errno == EINVAL, "graph/0 writable by others: refused, errno %d", errno) ;
    CHECK(service_graph_cache_write(&g, 0, BASE, SS_LIVE) == 0, "graph/0 writable by others: not written") ;
    CHECK(service_graph_cache_drop(0, SS_LIVE) == 1 && !lstat(CACHE, &st), "graph/0 writable by others: nothing removed") ;
    CHECK(chmod(ROOTDIR, 0755) == 0, "close graph/0") ;

    // the drop, then a drop on an absent cache
    CHECK(service_graph_cache_drop(0, SS_LIVE) == 1 && lstat(CACHE, &st) < 0 && errno == ENOENT, "drop: the cache gone") ;
    CHECK(service_graph_cache_drop(0, SS_LIVE) == 1, "drop of an absent cache") ;
    errno = 0 ;
    CHECK(service_graph_cache_read(&r, 0, BASE, SS_LIVE) == 0 && errno == ENOENT, "absent: 0, ENOENT, errno %d", errno) ;
    CHECK(dir_destroy(GRAPHDIR "/1000"), "remove graph/1000") ;

    service_graph_free(&g) ;
}

static void test_transaction(void)
{
    printf("the load reads the cache, a begin drops it, the end builds it again, a reader inside leaves it\n") ;

    service_graph_t g = SERVICE_GRAPH_ZERO, r = SERVICE_GRAPH_ZERO ;
    struct stat st ;

    // a cache that is not the system: the load reads it, it does not build
    make(&g) ;
    CHECK(service_graph_cache_write(&g, 0, BASE, SS_LIVE) == 1, "write the graph made by hand") ;
    CHECK(service_graph_load(&r, 0, BASE, SS_LIVE) == 1, "load") ;
    same(&r, &g, "the load reads the cache") ;
    service_graph_free(&r) ;

    CHECK(service_graph_write_begin(0, BASE, SS_LIVE) == 1, "begin") ;
    CHECK(lstat(CACHE, &st) < 0 && errno == ENOENT, "the begin dropped the cache") ;

    // a reader inside the transaction builds, never writes
    CHECK(service_graph_load(&r, 0, BASE, SS_LIVE) == 1 && !service_graph_nvertex(&r), "the load inside builds the empty system") ;
    CHECK(lstat(CACHE, &st) < 0 && errno == ENOENT, "the load inside left the cache to the end") ;
    service_graph_free(&r) ;

    CHECK(service_graph_write_end(0) == 1, "end") ;
    CHECK(service_graph_cache_read(&r, 0, BASE, SS_LIVE) == 1 && !service_graph_nvertex(&r), "the end built the cache of the system, empty") ;
    service_graph_free(&r) ;

    // a reader without cache and with the lock free writes what it builds
    CHECK(service_graph_cache_drop(0, SS_LIVE) == 1, "drop") ;
    CHECK(service_graph_load(&r, 0, BASE, SS_LIVE) == 1 && !lstat(CACHE, &st), "the reader wrote the cache it built") ;
    service_graph_free(&r) ;

    service_graph_free(&g) ;
}

static void test_live(void)
{
    printf("the live directory the caller gives: the cache under it, nowhere else\n") ;

    service_graph_t g = SERVICE_GRAPH_ZERO, r = SERVICE_GRAPH_ZERO ;
    struct stat st ;

    make(&g) ;
    CHECK(service_graph_cache_drop(0, SS_LIVE) == 1 && lstat(CACHE, &st) < 0, "no cache under SS_LIVE") ;
    CHECK(service_graph_cache_write(&g, 0, BASE, LIVE) == 1, "written under " LIVE) ;
    CHECK(!lstat(LIVE SS_LIVEGRAPH "/0/" SS_LIVEGRAPH_FILE, &st) && S_ISREG(st.st_mode), "the file under " LIVE) ;
    CHECK(lstat(CACHE, &st) < 0 && errno == ENOENT, "nothing under SS_LIVE") ;
    errno = 0 ;
    CHECK(service_graph_cache_read(&r, 0, BASE, SS_LIVE) == 0 && errno == ENOENT, "none read under SS_LIVE, errno %d", errno) ;
    CHECK(service_graph_cache_read(&r, 0, BASE, LIVE) == 1, "read under " LIVE) ;
    service_graph_free(&r) ;
    CHECK(service_graph_cache_drop(0, LIVE) == 1 && lstat(LIVE SS_LIVEGRAPH "/0/" SS_LIVEGRAPH_FILE, &st) < 0, "dropped under " LIVE) ;

    service_graph_free(&g) ;
}

static void child(char const *root)
{
    char map[64] ;
    uid_t uid = getuid() ;
    gid_t gid = getgid() ;

    CHECK(unshare(CLONE_NEWUSER) == 0, "unshare a user namespace: errno %d", errno) ;
    CHECK(file_write("/proc/self/setgroups", "deny", 4), "deny setgroups") ;
    sprintf(map, "0 %u 1", (unsigned)uid) ;
    CHECK(file_write("/proc/self/uid_map", map, strlen(map)), "map uid") ;
    sprintf(map, "0 %u 1", (unsigned)gid) ;
    CHECK(file_write("/proc/self/gid_map", map, strlen(map)), "map gid") ;
    CHECK(chroot(root) == 0 && chdir("/") == 0, "chroot to %s: errno %d", root, errno) ;

    CHECK(dir_create_parent(BASE SS_SYSTEM SS_RESOLVE SS_SERVICE SS_PROVIDE, 0755) && dir_create_parent(OTHER, 0755) && dir_create_parent(SS_LIVE, 0755), "make the system of the base, another, SS_LIVE") ;

    test_roundtrip() ;
    test_checks() ;
    test_format_base() ;
    test_place() ;
    test_transaction() ;
    test_live() ;

    fflush(stdout) ;
    _exit(0) ;
}

int main(void)
{
    printf("Starting graph cache test...\n") ;

    char root[64] = "/tmp/66-graph-cache.XXXXXX" ;
    int status = 0 ;

    PROG = "graph_cache" ;
    VERBOSITY = 0 ;
    CHECK(mkdtemp(root), "make the root") ;

    fflush(stdout) ;
    pid_t pid = fork() ;
    CHECK(pid >= 0, "fork") ;

    if (!pid)
        child(root) ;

    CHECK(waitpid(pid, &status, 0) == pid, "wait the child") ;
    CHECK(WIFEXITED(status) && !WEXITSTATUS(status), "the child passed: status %d", status) ;
    CHECK(dir_destroy(root), "remove the root") ;

    printf("All tests passed!\n") ;
    return 0 ;
}
