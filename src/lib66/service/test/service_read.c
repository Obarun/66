/*
 * service_read.c
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

/** service_read(), the one reading of a service and the one definition of an
 * unreadable one, and the store a command reads its services into, by effect:
 * a missing resolve file is 0 ENOENT, a resolve file present that does not
 * read (empty, damaged) or a declared addon missing or damaged is 0 EINVAL,
 * the addons are read once and on demand, the store gives one slot per vertex
 * and says a stale graph by 0. Everything happens under a private mkdtemp
 * directory, inside a write transaction for the writes. */

#include <assert.h>
#include <stddef.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/directory.h>
#include <oblibs/files.h>
#include <oblibs/strbuf.h>
#include <oblibs/bits.h>
#include <oblibs/hash.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/constants.h>

static char base[64] = "/tmp/66-service-read.XXXXXX" ;

static void resolve_file(char *file, char const *name, char const *suffix)
{
    auto_strings(file, base, SS_SYSTEM, SS_SERVICE, SS_SVC, "/", name, SS_RESOLVE, "/", name, suffix) ;
}

static void make_layout(char const *name)
{
    char entry[SS_MAX_PATH_LEN], svcdir[SS_MAX_PATH_LEN], svcres[SS_MAX_PATH_LEN], link[SS_MAX_PATH_LEN] ;
    auto_strings(entry, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE) ;
    auto_strings(svcdir, base, SS_SYSTEM, SS_SERVICE, SS_SVC, "/", name) ;
    auto_strings(svcres, svcdir, SS_RESOLVE) ;
    auto_strings(link, entry, "/", name) ;

    assert(dir_create_parent(entry, 0755)) ;
    assert(dir_create_parent(svcres, 0755)) ;
    assert(symlink(svcdir, link) == 0) ;
}

static void write_core(char const *name, uint32_t has_dependencies)
{
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE, &res) ;
    resolve_init(w) ;
    res.name = resolve_add_string(w, name) ;
    res.path.home = resolve_add_string(w, base) ;
    res.has_dependencies = has_dependencies ;
    assert(resolve_write(w, base, name) == 1) ;
    resolve_free(w) ;
}

// the execute addon, written through resolve_write(), which declares it in the resolve file
static void write_execute(char const *name, uint32_t notify)
{
    resolve_service_addon_execute_t ex = RESOLVE_SERVICE_ADDON_EXECUTE_ZERO ;
    resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE_EXECUTE, &ex) ;
    resolve_init(w) ;
    ex.notify = notify ;
    ex.chdir = resolve_add_string(w, "/srv") ;
    assert(resolve_write(w, base, name) == 1) ;
    resolve_free(w) ;
}

static void test_absent(void)
{
    printf("a service without resolve file: 0 ENOENT\n") ;

    struct resolve_hash_s h = RESOLVE_HASH_ZERO ;
    errno = 0 ;
    assert(service_read(&h, base, "nobody", SERVICE_READ_ALL) == 0) ;
    assert(errno == ENOENT) ;
    assert(!h.name[0] && !h.read && !h.res.sa.len) ;
    service_read_free(&h) ;
}

static void test_additive(void)
{
    printf("the addons are read on demand, once\n") ;

    make_layout("plain") ;
    write_core("plain", 0) ;
    write_execute("plain", 3) ;

    struct resolve_hash_s h = RESOLVE_HASH_ZERO ;

    // the resolve file alone
    assert(service_read(&h, base, "plain", 0) == 1) ;
    assert(!strcmp(h.name, "plain")) ;
    assert(!strcmp(h.res.sa.s + h.res.name, "plain")) ;
    assert(h.res.has_execute && !h.res.has_dependencies) ;
    assert(!h.read) ;
    assert(!h.execute.sa.len) ;

    // the execute addon it declares, then read
    assert(service_read(&h, base, "plain", SERVICE_READ_EXECUTE) == 1) ;
    assert(h.read == SERVICE_READ_EXECUTE) ;
    assert(h.execute.notify == 3) ;
    assert(!strcmp(h.execute.sa.s + h.execute.chdir, "/srv")) ;

    // an addon it does not declare: left zero, counted as read
    assert(service_read(&h, base, "plain", SERVICE_READ_DEPENDENCIES | SERVICE_READ_LOGGER) == 1) ;
    assert(h.read == (SERVICE_READ_EXECUTE | SERVICE_READ_DEPENDENCIES | SERVICE_READ_LOGGER)) ;
    assert(!h.dependencies.sa.len && !h.dependencies.ndepends) ;

    // what was read is not read again: the disk no longer has the addon
    char file[SS_MAX_PATH_LEN] ;
    resolve_file(file, "plain", SS_ADDON_EXECUTE_SUFFIX) ;
    file_tryunlink(file) ;
    assert(access(file, F_OK) < 0) ;
    assert(service_read(&h, base, "plain", SERVICE_READ_ALL) == 1) ;
    assert(h.read == SERVICE_READ_ALL) ;
    assert(h.execute.notify == 3) ;

    // a second name on the same entry is a fault of the caller
    errno = 0 ;
    assert(service_read(&h, base, "other", 0) == -1 && errno == EINVAL) ;

    // addons outside SERVICE_READ_ALL as well
    errno = 0 ;
    assert(service_read(&h, base, "plain", SERVICE_READ_ALL + 1) == -1 && errno == EINVAL) ;

    service_read_free(&h) ;
    assert(!h.name[0] && !h.read && !h.res.sa.s && !h.execute.sa.s) ;

    // the addon is gone from the disk now: a fresh read with it is unreadable
    errno = 0 ;
    assert(service_read(&h, base, "plain", SERVICE_READ_EXECUTE) == 0) ;
    assert(errno == EINVAL) ;
    assert(!(h.read & SERVICE_READ_EXECUTE) && !h.execute.sa.len) ;
    service_read_free(&h) ;
}

static void test_unreadable(void)
{
    printf("a resolve file or a declared addon that does not read: 0 EINVAL\n") ;

    struct resolve_hash_s h = RESOLVE_HASH_ZERO ;
    char file[SS_MAX_PATH_LEN] ;

    // a declared addon missing
    make_layout("nodeps") ;
    write_core("nodeps", 1) ;
    errno = 0 ;
    assert(service_read(&h, base, "nodeps", SERVICE_READ_DEPENDENCIES) == 0) ;
    assert(errno == EINVAL) ;
    // the resolve file itself reads: asked without the addon, it is read
    service_read_free(&h) ;
    assert(service_read(&h, base, "nodeps", 0) == 1) ;
    service_read_free(&h) ;

    // a declared addon damaged
    make_layout("baddep") ;
    write_core("baddep", 1) ;
    resolve_file(file, "baddep", SS_ADDON_DEPENDENCIES_SUFFIX) ;
    assert(file_write(file, "garbage", 7)) ;
    errno = 0 ;
    assert(service_read(&h, base, "baddep", SERVICE_READ_DEPENDENCIES) == 0) ;
    assert(errno == EINVAL) ;
    service_read_free(&h) ;

    // a resolve file damaged
    make_layout("damaged") ;
    resolve_file(file, "damaged", "") ;
    assert(file_write(file, "garbage", 7)) ;
    errno = 0 ;
    assert(service_read(&h, base, "damaged", 0) == 0) ;
    assert(errno == EINVAL) ;
    assert(!h.name[0] && !h.res.sa.len) ;
    service_read_free(&h) ;

    // a resolve file truncated to nothing
    make_layout("empty") ;
    resolve_file(file, "empty", "") ;
    assert(file_write(file, "", 0)) ;
    errno = 0 ;
    assert(service_read(&h, base, "empty", 0) == 0) ;
    assert(errno == EINVAL) ;
    service_read_free(&h) ;
}

// a graph of @n vertexes named @names, in @states, made by hand
static void make_graph(service_graph_t *g, char const *const *names, uint8_t const *states, uint32_t n)
{
    for (uint32_t i = 0 ; i < n ; i++) {
        service_graph_vertex_t v = SERVICE_GRAPH_VERTEX_ZERO ;
        v.name = (uint32_t)g->strings.len ;
        v.state = states[i] ;
        assert(strbuf_catb(&g->strings, names[i], strlen(names[i]) + 1)) ;
        assert(strbuf_catb(&g->vertex, (char const *)&v, sizeof(v))) ;
    }
}

static void free_graph(service_graph_t *g)
{
    strbuf_free(&g->vertex) ;
    strbuf_free(&g->strings) ;
}

static void test_store(void)
{
    printf("the store: a slot per vertex, a stale graph said by 0\n") ;

    char const *names[] = { "alpha", "ghost", "damaged", "gone", "cycle" } ;
    uint8_t const states[] = { SERVICE_GRAPH_STATE_OK, SERVICE_GRAPH_STATE_ABSENT, SERVICE_GRAPH_STATE_UNREADABLE, SERVICE_GRAPH_STATE_OK, SERVICE_GRAPH_STATE_UNORDERED } ;
    service_graph_t g = SERVICE_GRAPH_ZERO ;
    service_graph_resolve_t store = SERVICE_GRAPH_RESOLVE_ZERO ;
    struct resolve_hash_s *h, *again ;

    make_layout("alpha") ;
    write_core("alpha", 0) ;
    write_execute("alpha", 4) ;
    make_layout("cycle") ;
    write_core("cycle", 0) ;

    make_graph(&g, names, states, 5) ;
    assert(service_graph_resolve_init(&store, &g, base) == 1) ;
    assert(store.g == &g && store.base == base && store.slot) ;
    for (uint32_t i = 0 ; i < 5 ; i++)
        assert(!store.slot[i]) ;

    // a readable vertex: read once, then completed
    assert(service_graph_resolve_read(&store, 0, 0, &h) == 1) ;
    assert(h && !strcmp(h->name, "alpha") && !h->read) ;
    assert(service_graph_resolve_read(&store, 0, SERVICE_READ_EXECUTE, &again) == 1) ;
    assert(again == h && h->execute.notify == 4) ;
    assert(store.slot[0] == h) ;

    // an unordered vertex reads as well
    assert(service_graph_resolve_read(&store, 4, SERVICE_READ_ALL, &h) == 1) ;
    assert(!strcmp(h->name, "cycle") && h->read == SERVICE_READ_ALL) ;

    // no readable vertex, or no vertex: a fault of the caller
    errno = 0 ;
    assert(service_graph_resolve_read(&store, 1, 0, &h) == -1 && errno == EINVAL && !h) ;
    errno = 0 ;
    assert(service_graph_resolve_read(&store, 2, 0, &h) == -1 && errno == EINVAL) ;
    errno = 0 ;
    assert(service_graph_resolve_read(&store, 5, 0, &h) == -1 && errno == EINVAL) ;
    errno = 0 ;
    assert(service_graph_resolve_read(&store, SERVICE_GRAPH_NOINDEX, 0, &h) == -1 && errno == EINVAL) ;

    // a vertex the graph says readable that is gone: the graph is stale
    errno = 0 ;
    assert(service_graph_resolve_read(&store, 3, 0, &h) == 0 && !h) ;
    assert(errno == ENOENT) ;

    service_graph_resolve_free(&store) ;
    assert(!store.g && !store.base && !store.slot) ;

    // the load reads the readable members only, an absent and an unreadable one aside
    selection_service_t sel = SELECTION_SERVICE_ZERO ;
    sel.g = &g ;
    sel.member = bitset32_init(5) ;
    bitset32_set(&sel.member, 0) ;
    bitset32_set(&sel.member, 1) ;
    bitset32_set(&sel.member, 2) ;
    bitset32_set(&sel.member, 4) ;

    assert(service_graph_resolve_init(&store, &g, base) == 1) ;
    assert(service_graph_resolve_load(&store, &sel, SERVICE_READ_EXECUTE) == 1) ;
    assert(store.slot[0] && store.slot[0]->read == SERVICE_READ_EXECUTE && store.slot[0]->execute.notify == 4) ;
    assert(!store.slot[1] && !store.slot[2] && !store.slot[3]) ;
    assert(store.slot[4] && store.slot[4]->read == SERVICE_READ_EXECUTE) ;

    // a stale member stops the load with 0
    bitset32_set(&sel.member, 3) ;
    assert(service_graph_resolve_load(&store, &sel, 0) == 0) ;
    service_graph_resolve_free(&store) ;

    // a selection of another graph is a fault of the caller
    service_graph_t other = SERVICE_GRAPH_ZERO ;
    assert(service_graph_resolve_init(&store, &other, base) == 1) ;
    assert(!store.slot) ;
    errno = 0 ;
    assert(service_graph_resolve_load(&store, &sel, 0) == -1 && errno == EINVAL) ;
    service_graph_resolve_free(&store) ;

    free_graph(&g) ;
}

/* resolve_read() and resolve_check(): 0 only for a file missing (ENOENT) or
 * that does not read (EINVAL); a path that cannot be checked is a system
 * error, and service_read() says it as it */
static void test_system_error(void)
{
    printf("a check of the path that fails is -1, not an absent service\n") ;

    char fbase[SS_MAX_PATH_LEN] ;
    struct resolve_hash_s h = RESOLVE_HASH_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_SERVICE, &res) ;

    // a base whose component is a regular file: access() gives ENOTDIR
    auto_strings(fbase, base, "afile") ;
    assert(file_write(fbase, "", 0)) ;
    auto_strings(fbase, base, "afile/") ;

    errno = 0 ;
    assert(resolve_check(&w, fbase, "x") == -1 && errno == ENOTDIR) ;
    errno = 0 ;
    assert(resolve_read(&w, fbase, "x") == -1 && errno == ENOTDIR) ;
    errno = 0 ;
    assert(service_read(&h, fbase, "x", 0) == -1 && errno == ENOTDIR) ;
    service_read_free(&h) ;

    // missing: 0 ENOENT, as before
    errno = 0 ;
    assert(resolve_read(&w, base, "nobody") == 0 && errno == ENOENT) ;

    // a type without file of its own is a fault of the caller
    resolve_wrapper_t wl = RESOLVE_WRAPPER(DATA_SERVICE_LOGGER, &res) ;
    errno = 0 ;
    assert(resolve_check(&wl, base, "plain") == -1 && errno == EINVAL) ;
    assert(resolve_addon_suffix(DATA_SERVICE_LOGGER) == 0) ;
    assert(!strcmp(resolve_addon_suffix(DATA_SERVICE_EVENT), SS_ADDON_EVENT_SUFFIX)) ;
    assert(!strcmp(resolve_addon_suffix(DATA_TREE), "")) ;
}

// service_read_free() and resolve_hash_free() leave errno as they found it
static void test_free_errno(void)
{
    printf("a release keeps errno\n") ;

    struct resolve_hash_s h = RESOLVE_HASH_ZERO ;
    hash_t hres = HASH_ZERO ;

    assert(service_read(&h, base, "plain", 0) == 1) ;
    errno = EDOM ;
    service_read_free(&h) ;
    assert(errno == EDOM && !h.name[0]) ;

    assert(hash_init(&hres, 0, offsetof(struct resolve_hash_s, node))) ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_SERVICE, &res) ;
    resolve_init(&w) ;
    res.name = resolve_add_string(&w, "inhash") ;
    assert(resolve_hash_add(&hres, "inhash", res)) ;
    errno = EDOM ;
    resolve_hash_free(&hres) ;
    assert(errno == EDOM) ;

    // the table owns what res held
    res = (resolve_service_t)RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t *wm = resolve_set_struct(DATA_SERVICE, &res) ;
    assert(wm) ;
    resolve_init(wm) ;
    assert(resolve_add_string(wm, "payload") >= 0 && res.sa.len) ;
    errno = EDOM ;
    resolve_free_obj(wm) ;
    assert(errno == EDOM && !res.sa.len && !res.sa.s) ;
    resolve_free(wm) ;
}

static void provide_link(char const *name, char const *target)
{
    char dir[SS_MAX_PATH_LEN], lnk[SS_MAX_PATH_LEN] ;
    auto_strings(dir, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, SS_PROVIDE) ;
    auto_strings(lnk, dir, "/", name) ;
    assert(dir_create_parent(dir, 0755)) ;
    assert(symlink(target, lnk) == 0) ;
}

/* service_resolve_provide(): the one designation of a name, the overlay of a
 * parse first, 1 only when another service holds the name */
static void test_provide(void)
{
    printf("a name designates one service: itself, or the one that holds it\n") ;

    char dst[SS_MAX_SERVICE_NAME + 1] ;
    char longname[SS_MAX_SERVICE_NAME + 2] ;
    hash_t hres = HASH_ZERO ;

    provide_link("held", "../pv") ;
    provide_link("self", "../self") ;
    provide_link("empty", "../") ;

    char damaged[SS_MAX_PATH_LEN] ;
    auto_strings(damaged, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, SS_PROVIDE, "/damaged") ;
    assert(file_write(damaged, "", 0)) ;

    // nobody holds it: 0, @dst untouched
    memcpy(dst, "untouched", 10) ;
    assert(service_resolve_provide(dst, "free", 0, base) == 0 && !strcmp(dst, "untouched")) ;

    assert(service_resolve_provide(dst, "held", 0, base) == 1 && !strcmp(dst, "pv")) ;

    // a link to the name itself, a link to nothing, an entry that is no link: no other holder
    assert(service_resolve_provide(dst, "self", 0, base) == 0) ;
    assert(service_resolve_provide(dst, "empty", 0, base) == 0) ;
    assert(service_resolve_provide(dst, "damaged", 0, base) == 0) ;

    // a name no link can bear, empty or too long
    memset(longname, 'a', SS_MAX_SERVICE_NAME + 1) ;
    longname[SS_MAX_SERVICE_NAME + 1] = 0 ;
    memcpy(dst, "untouched", 10) ;
    assert(service_resolve_provide(dst, longname, 0, base) == 0 && !strcmp(dst, "untouched")) ;
    assert(service_resolve_provide(dst, "", 0, base) == 0) ;

    // a system error
    char fbase[SS_MAX_PATH_LEN] ;
    auto_strings(fbase, base, "afile/") ;
    errno = 0 ;
    assert(service_resolve_provide(dst, "held", 0, fbase) == -1 && errno == ENOTDIR) ;

    /** the overlay of a parse: a frontend parsed under its own name takes it
     * back, a service parsed that provides a name holds it before any link */
    assert(hash_init(&hres, 0, offsetof(struct resolve_hash_s, node))) ;
    {
        resolve_service_t res = RESOLVE_SERVICE_ZERO ;
        resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_SERVICE, &res) ;
        resolve_init(&w) ;
        res.name = resolve_add_string(&w, "held") ;
        assert(resolve_hash_add(&hres, "held", res)) ;
    }
    {
        resolve_service_t res = RESOLVE_SERVICE_ZERO ;
        resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_SERVICE, &res) ;
        resolve_init(&w) ;
        res.name = resolve_add_string(&w, "pz") ;
        assert(resolve_hash_add(&hres, "pz", res)) ;

        struct resolve_hash_s *c = resolve_hash_search(&hres, "pz") ;
        resolve_wrapper_t wd = RESOLVE_WRAPPER(DATA_SERVICE_DEPENDENCIES, &c->dependencies) ;
        resolve_init(&wd) ;
        c->dependencies.provide = resolve_add_string(&wd, "free self") ;
        c->dependencies.nprovide = 2 ;
    }

    memcpy(dst, "untouched", 10) ;
    assert(service_resolve_provide(dst, "held", &hres, base) == 0 && !strcmp(dst, "untouched")) ;
    assert(service_resolve_provide(dst, "free", &hres, base) == 1 && !strcmp(dst, "pz")) ;
    assert(service_resolve_provide(dst, "self", &hres, base) == 1 && !strcmp(dst, "pz")) ;
    // not in the overlay: the link
    assert(service_resolve_provide(dst, "empty", &hres, base) == 0) ;

    resolve_hash_free(&hres) ;
}

int main(void)
{
    printf("Starting service read test...\n") ;

    VERBOSITY = 0 ;
    assert(mkdtemp(base)) ;
    strcat(base, "/") ;

    // resolve_write() writes the system: the test holds its write transaction
    assert(service_graph_write_begin(getuid(), base, base) == 1) ;

    test_absent() ;
    test_additive() ;
    test_unreadable() ;
    test_store() ;
    test_system_error() ;
    test_free_errno() ;
    test_provide() ;

    assert(service_graph_write_end(0) == 1) ;
    assert(dir_destroy(base)) ;

    printf("All tests passed!\n") ;
    return 0 ;
}
