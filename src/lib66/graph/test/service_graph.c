/*
 * service_graph.c
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

/** The graph of the system and the selections drawn in it, by effect. A
 * system is written under a private mkdtemp directory with the write
 * transaction and resolve_write(): a dependency chain, a RequiredBy, an
 * absent name, a cycle, unreadable resolve files, a reactor, a tick reactor, a
 * module, a logger, a Provide link, a dangling
 * link, optional dependencies. The graph built from it is checked vertex by
 * vertex and edge by edge, then the propagations and the messages of
 * an invalid service. The frontend lookups run in a child that owns a user
 * namespace and a chroot holding the frontend directories, so that nothing of
 * the host is read. */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/directory.h>
#include <oblibs/files.h>
#include <oblibs/strbuf.h>
#include <oblibs/bits.h>
#include <oblibs/hash.h>
#include <oblibs/io.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/enum_parser.h>
#include <66/event_rule.h>

#define CHECK(cond, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__) ; \
        fprintf(stderr, __VA_ARGS__) ; \
        fprintf(stderr, "\n") ; \
        abort() ; \
    } \
} while (0)

#define NO_EDGE 0xff

static char base[64] = "/tmp/66-service-graph.XXXXXX" ;

/**
 *
 * Fixtures
 *
 * */

static void make_layout(char const *b, char const *name)
{
    char entry[SS_MAX_PATH_LEN], svcdir[SS_MAX_PATH_LEN], svcres[SS_MAX_PATH_LEN], link[SS_MAX_PATH_LEN] ;
    auto_strings(entry, b, SS_SYSTEM, SS_RESOLVE, SS_SERVICE) ;
    auto_strings(svcdir, b, SS_SYSTEM, SS_SERVICE, SS_SVC, "/", name) ;
    auto_strings(svcres, svcdir, SS_RESOLVE) ;
    auto_strings(link, entry, "/", name) ;

    CHECK(dir_create_parent(entry, 0755), "create %s", entry) ;
    CHECK(dir_create_parent(svcres, 0755), "create %s", svcres) ;
    CHECK(symlink(svcdir, link) == 0, "link %s", link) ;
}

static uint32_t count_words(char const *list)
{
    uint32_t n = 0 ;
    for (char const *p = list ; *p ; p++)
        if (*p != ' ' && (p == list || p[-1] == ' '))
            n++ ;
    return n ;
}

typedef struct svc_s svc_t ;
struct svc_s
{
    char const *name ;
    uint32_t type ;
    uint32_t islog ;
    char const *depends ;
    char const *requiredby ;
    char const *optsdeps ;
    char const *contents ;
    char const *provide ;
} ;

#define SVC_ZERO { .name = 0, .type = E_PARSER_TYPE_CLASSIC, .islog = 0, .depends = "", .requiredby = "", .optsdeps = "", .contents = "", .provide = "" }

static void fill_dependencies(resolve_service_addon_dependencies_t *dep, svc_t const *s)
{
    resolve_wrapper_t w = { .type = DATA_SERVICE_DEPENDENCIES, .obj = dep } ;
    resolve_init(&w) ;
    dep->depends = resolve_add_string(&w, s->depends) ;
    dep->ndepends = count_words(s->depends) ;
    dep->requiredby = resolve_add_string(&w, s->requiredby) ;
    dep->nrequiredby = count_words(s->requiredby) ;
    dep->optsdeps = resolve_add_string(&w, s->optsdeps) ;
    dep->noptsdeps = count_words(s->optsdeps) ;
    dep->contents = resolve_add_string(&w, s->contents) ;
    dep->ncontents = count_words(s->contents) ;
    dep->provide = resolve_add_string(&w, s->provide) ;
    dep->nprovide = count_words(s->provide) ;
    dep->conflict = resolve_add_string(&w, "") ;
}

static void fill_core(resolve_service_t *res, svc_t const *s, char const *b)
{
    resolve_wrapper_t w = { .type = DATA_SERVICE, .obj = res } ;
    resolve_init(&w) ;
    res->name = resolve_add_string(&w, s->name) ;
    res->path.home = resolve_add_string(&w, b) ;
    res->type = s->type ;
    res->islog = s->islog ;
}

static void write_service(char const *b, svc_t const *s)
{
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t w = { .type = DATA_SERVICE, .obj = &res } ;

    make_layout(b, s->name) ;
    fill_core(&res, s, b) ;
    CHECK(resolve_write(&w, b, s->name) == 1, "write the resolve file of %s", s->name) ;
    strbuf_free(&res.sa) ;

    if (!*s->depends && !*s->requiredby && !*s->optsdeps && !*s->contents && !*s->provide)
        return ;

    resolve_service_addon_dependencies_t dep = RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
    resolve_wrapper_t wd = { .type = DATA_SERVICE_DEPENDENCIES, .obj = &dep } ;
    fill_dependencies(&dep, s) ;
    CHECK(resolve_write(&wd, b, s->name) == 1, "write the dependencies of %s", s->name) ;
    strbuf_free(&dep.sa) ;
}

static void write_event(char const *b, char const *name, uint32_t type, char const *from)
{
    resolve_service_addon_event_t ev = RESOLVE_SERVICE_ADDON_EVENT_ZERO ;
    resolve_wrapper_t w = { .type = DATA_SERVICE_EVENT, .obj = &ev } ;
    resolve_init(&w) ;
    ev.type = type ;
    ev.from = resolve_add_string(&w, from) ;
    ev.nfrom = count_words(from) ;
    CHECK(resolve_write(&w, b, name) == 1, "write the event addon of %s", name) ;
    strbuf_free(&ev.sa) ;
}

static void write_garbage(char const *b, char const *name)
{
    char file[SS_MAX_PATH_LEN] ;
    make_layout(b, name) ;
    auto_strings(file, b, SS_SYSTEM, SS_SERVICE, SS_SVC, "/", name, SS_RESOLVE, "/", name) ;
    CHECK(file_write(file, "garbage", 7), "write garbage at %s", file) ;
}

static void write_provide(char const *b, char const *name, char const *target)
{
    char dir[SS_MAX_PATH_LEN], link[SS_MAX_PATH_LEN] ;
    auto_strings(dir, b, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, SS_PROVIDE) ;
    auto_strings(link, dir, "/", name) ;
    CHECK(dir_create_parent(dir, 0755), "create %s", dir) ;
    CHECK(symlink(target, link) == 0, "link %s", link) ;
}

static void write_dangling(char const *b, char const *name)
{
    char entry[SS_MAX_PATH_LEN], link[SS_MAX_PATH_LEN] ;
    auto_strings(entry, b, SS_SYSTEM, SS_RESOLVE, SS_SERVICE) ;
    auto_strings(link, entry, "/", name) ;
    CHECK(dir_create_parent(entry, 0755), "create %s", entry) ;
    CHECK(symlink("/nonexistent/66-service-graph", link) == 0, "link %s", link) ;
}

static void write_system(char const *b)
{
    svc_t s[] = {
        { .name = "a" },
        { .name = "b", .depends = "a" },
        { .name = "c", .depends = "b ghost" },
        { .name = "q", .requiredby = "c" },
        { .name = "x", .depends = "y" },
        { .name = "y", .depends = "x" },
        { .name = "z", .depends = "x" },
        { .name = "w", .depends = "u" },
        { .name = "w2", .depends = "u" },
        { .name = "v", .depends = "u2", .requiredby = "u2" },
        { .name = "src" },
        { .name = "re" },
        { .name = "tk", .depends = "src" },
        { .name = "mod", .type = E_PARSER_TYPE_MODULE, .contents = "mod:m1 mod:m2" },
        { .name = "mod:m1" },
        { .name = "mod:m2" },
        { .name = "lg", .depends = "lg-log" },
        { .name = "lg-log", .islog = 1, .requiredby = "lg" },
        { .name = "pv" },
        { .name = "user1", .depends = "alias" },
        { .name = "o", .optsdeps = "a" },
        { .name = "dd", .depends = "a", .optsdeps = "a" },
    } ;

    for (size_t i = 0 ; i < sizeof(s) / sizeof(s[0]) ; i++) {
        svc_t one = SVC_ZERO ;
        one.name = s[i].name ;
        one.type = s[i].type ? s[i].type : (uint32_t)E_PARSER_TYPE_CLASSIC ;
        one.islog = s[i].islog ;
        one.depends = s[i].depends ? s[i].depends : "" ;
        one.requiredby = s[i].requiredby ? s[i].requiredby : "" ;
        one.optsdeps = s[i].optsdeps ? s[i].optsdeps : "" ;
        one.contents = s[i].contents ? s[i].contents : "" ;
        one.provide = s[i].provide ? s[i].provide : "" ;
        write_service(b, &one) ;
    }

    write_event(b, "re", EVENT_SOURCE_SERVICE, "src") ;
    write_event(b, "tk", EVENT_SOURCE_TIMER, "src") ;
    write_garbage(b, "u") ;
    write_garbage(b, "u2") ;
    write_garbage(b, "lone") ;
    write_provide(b, "alias", "../pv") ;
    write_dangling(b, "dang") ;
}

/**
 *
 * Helpers
 *
 * */

static uint32_t idx(service_graph_t const *g, char const *name)
{
    uint32_t i = service_graph_find(g, name) ;
    CHECK(i != SERVICE_GRAPH_NOINDEX, "vertex %s is in the graph", name) ;
    return i ;
}

// the origin of the edge @from depends on @to, NO_EDGE without one
static uint8_t dorigin(service_graph_t const *g, char const *from, char const *to)
{
    uint32_t f = idx(g, from), t = idx(g, to) ;
    uint32_t const *d = service_graph_depends(g, f) ;
    for (uint32_t pos = 0 ; pos < service_graph_vertex(g, f)->ndepends ; pos++)
        if (d[pos] == t)
            return service_graph_dorigin(g, f)[pos] ;
    return NO_EDGE ;
}

static uint8_t rorigin(service_graph_t const *g, char const *to, char const *from)
{
    uint32_t f = idx(g, from), t = idx(g, to) ;
    uint32_t const *r = service_graph_requiredby(g, t) ;
    for (uint32_t pos = 0 ; pos < service_graph_vertex(g, t)->nrequiredby ; pos++)
        if (r[pos] == f)
            return service_graph_rorigin(g, t)[pos] ;
    return NO_EDGE ;
}

static uint32_t sortpos(service_graph_t const *g, uint32_t index)
{
    for (uint32_t pos = 0 ; pos < service_graph_nsort(g) ; pos++)
        if (service_graph_sort(g, pos) == index)
            return pos ;
    return SERVICE_GRAPH_NOINDEX ;
}

static bitset32_t seeds_of(service_graph_t const *g, char const *const *names, size_t n)
{
    bitset32_t s = bitset32_init(SS_MAX_SERVICE) ;
    for (size_t i = 0 ; i < n ; i++)
        bitset32_set(&s, idx(g, names[i])) ;
    return s ;
}

// @sel holds exactly the members @names
static void members_are(selection_service_t const *sel, char const *const *names, uint32_t n, char const *what)
{
    service_graph_t const *g = sel->g ;
    uint32_t count = 0 ;

    for (uint32_t i = 0 ; i < n ; i++)
        CHECK(bitset32_isvalid(&sel->member, idx(g, names[i])), "%s: %s is a member", what, names[i]) ;

    for (uint32_t i = 0 ; i < service_graph_nvertex(g) ; i++)
        if (bitset32_isvalid(&sel->member, i))
            count++ ;

    CHECK(count == n, "%s: %u members, expected %u", what, count, n) ;
    CHECK(sel->member.count == n, "%s: n is %u, expected %u", what, sel->member.count, n) ;
}

static uint32_t count_bits(service_graph_t const *g, bitset32_t const *b)
{
    uint32_t count = 0 ;
    for (uint32_t i = 0 ; i < service_graph_nvertex(g) ; i++)
        if (bitset32_isvalid(b, i))
            count++ ;
    return count ;
}

static void propagate_names(selection_service_t *sel, service_graph_t const *g, char const *const *names, size_t n, uint8_t propagate, bool from, bitset32_t const *reactors, bool logger)
{
    bitset32_t seeds = seeds_of(g, names, n) ;
    selection_service_policy_t policy = { .propagate = propagate, .from = from, .reactors = reactors, .logger = logger } ;
    selection_service_propagate(sel, g, &seeds, &policy) ;
}

/**
 *
 * The graph
 *
 * */

static void test_empty(void)
{
    printf("a system without service is empty, a base without resolve directory is an error\n") ;

    char empty[96 + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + SS_PROVIDE_LEN] ;
    service_graph_t g = SERVICE_GRAPH_ZERO ;

    auto_strings(empty, base, "nothing/") ;
    errno = 0 ;
    CHECK(service_graph_build(&g, empty, 0) == -1 && errno == ENOENT, "no resolve directory: errno %d", errno) ;
    service_graph_free(&g) ;

    auto_strings(empty, base, "nothing/", SS_SYSTEM, SS_RESOLVE, SS_SERVICE, SS_PROVIDE) ;
    CHECK(dir_create_parent(empty, 0755), "create %s", empty) ;
    auto_strings(empty, base, "nothing/") ;
    CHECK(service_graph_build(&g, empty, 0) == 1, "build an empty system: errno %d", errno) ;
    CHECK(service_graph_nvertex(&g) == 0 && service_graph_nsort(&g) == 0, "no vertex, no order") ;
    service_graph_free(&g) ;
}

static void test_vertexes(service_graph_t const *g)
{
    printf("every resolve file and every declared name is a vertex, sorted by name\n") ;

    struct { char const *name ; uint8_t state ; } expect[] = {
        { "a", SERVICE_GRAPH_STATE_OK }, { "b", SERVICE_GRAPH_STATE_OK }, { "c", SERVICE_GRAPH_STATE_OK },
        { "dang", SERVICE_GRAPH_STATE_ABSENT }, { "dd", SERVICE_GRAPH_STATE_OK }, { "ghost", SERVICE_GRAPH_STATE_ABSENT },
        { "lg", SERVICE_GRAPH_STATE_OK }, { "lg-log", SERVICE_GRAPH_STATE_OK }, { "lone", SERVICE_GRAPH_STATE_UNREADABLE },
        { "mod", SERVICE_GRAPH_STATE_OK }, { "mod:m1", SERVICE_GRAPH_STATE_OK }, { "mod:m2", SERVICE_GRAPH_STATE_OK },
        { "o", SERVICE_GRAPH_STATE_OK },
        { "pv", SERVICE_GRAPH_STATE_OK }, { "q", SERVICE_GRAPH_STATE_OK }, { "re", SERVICE_GRAPH_STATE_OK },
        { "src", SERVICE_GRAPH_STATE_OK }, { "tk", SERVICE_GRAPH_STATE_OK }, { "u", SERVICE_GRAPH_STATE_UNREADABLE },
        { "u2", SERVICE_GRAPH_STATE_UNREADABLE }, { "user1", SERVICE_GRAPH_STATE_OK }, { "v", SERVICE_GRAPH_STATE_UNORDERED },
        { "w", SERVICE_GRAPH_STATE_OK }, { "w2", SERVICE_GRAPH_STATE_OK }, { "x", SERVICE_GRAPH_STATE_UNORDERED },
        { "y", SERVICE_GRAPH_STATE_UNORDERED }, { "z", SERVICE_GRAPH_STATE_UNORDERED },
    } ;
    uint32_t n = sizeof(expect) / sizeof(expect[0]) ;

    CHECK(service_graph_nvertex(g) == n, "%u vertexes, expected %u", service_graph_nvertex(g), n) ;

    for (uint32_t i = 0 ; i < n ; i++) {
        CHECK(!strcmp(service_graph_name(g, i), expect[i].name), "index %u is %s, expected %s", i, service_graph_name(g, i), expect[i].name) ;
        CHECK(service_graph_find(g, expect[i].name) == i, "find %s gives its rank %u", expect[i].name, i) ;
        CHECK(service_graph_state(g, i) == expect[i].state, "%s is in state %u, expected %u", expect[i].name, service_graph_state(g, i), expect[i].state) ;
    }

    // a provided name answers for its provider: it is no vertex
    CHECK(service_graph_find(g, "alias") == SERVICE_GRAPH_NOINDEX, "the provided name alias is no vertex") ;
    CHECK(service_graph_find(g, "") == SERVICE_GRAPH_NOINDEX, "the empty name is no vertex") ;
    CHECK(service_graph_find(g, "zz") == SERVICE_GRAPH_NOINDEX, "a name past the last is no vertex") ;
    CHECK(service_graph_find(g, "0") == SERVICE_GRAPH_NOINDEX, "a name before the first is no vertex") ;
}

static void test_edges(service_graph_t const *g)
{
    printf("the edges and their origins, requiredby the exact transpose of depends\n") ;

    uint8_t const D = SERVICE_GRAPH_EDGE_DECLARED, R = SERVICE_GRAPH_EDGE_REQUIREDBY, C = SERVICE_GRAPH_EDGE_CONTENTS, F = SERVICE_GRAPH_EDGE_FROM, L = SERVICE_GRAPH_EDGE_LOGGER ;

    struct { char const *from, *to ; uint8_t origin ; } expect[] = {
        { "b", "a", D }, { "c", "b", D }, { "c", "ghost", D }, { "c", "q", D|R },
        { "x", "y", D }, { "y", "x", D }, { "z", "x", D },
        { "w", "u", D }, { "w2", "u", D }, { "v", "u2", D }, { "u2", "v", D|R },
        { "re", "src", F }, { "tk", "src", D|F },
        { "mod", "mod:m1", C }, { "mod", "mod:m2", C },
        { "lg", "lg-log", D|R|L },
        { "user1", "pv", D }, { "o", "a", D }, { "dd", "a", D },
    } ;
    uint32_t n = sizeof(expect) / sizeof(expect[0]), total = 0, rtotal = 0 ;

    for (uint32_t i = 0 ; i < n ; i++) {
        uint8_t l = dorigin(g, expect[i].from, expect[i].to) ;
        CHECK(l == expect[i].origin, "%s -> %s marked %u, expected %u", expect[i].from, expect[i].to, l, expect[i].origin) ;
        l = rorigin(g, expect[i].to, expect[i].from) ;
        CHECK(l == expect[i].origin, "%s <- %s marked %u in requiredby, expected %u", expect[i].to, expect[i].from, l, expect[i].origin) ;
    }

    for (uint32_t v = 0 ; v < service_graph_nvertex(g) ; v++) {

        service_graph_vertex_t const *vx = service_graph_vertex(g, v) ;
        uint32_t const *d = service_graph_depends(g, v) ;
        uint32_t const *r = service_graph_requiredby(g, v) ;

        for (uint32_t pos = 1 ; pos < vx->ndepends ; pos++)
            CHECK(d[pos - 1] < d[pos], "the depends of %s are sorted by index, each once", service_graph_name(g, v)) ;

        for (uint32_t pos = 1 ; pos < vx->nrequiredby ; pos++)
            CHECK(r[pos - 1] < r[pos], "the requiredby of %s are sorted by index, each once", service_graph_name(g, v)) ;

        total += vx->ndepends ;
        rtotal += vx->nrequiredby ;
    }

    CHECK(total == n, "%u edges, expected %u: no edge besides the declared ones", total, n) ;
    CHECK(rtotal == n, "%u requiredby edges, expected %u", rtotal, n) ;
    CHECK(g->depends.len == n * sizeof(uint32_t) && g->requiredby.len == n * sizeof(uint32_t), "edge blocks hold %u edges", n) ;
    CHECK(g->dorigin.len == n && g->rorigin.len == n, "origin blocks hold %u origins", n) ;

    // an unreadable service has no edge of its own, its neighbours' are kept
    CHECK(service_graph_vertex(g, idx(g, "u"))->ndepends == 0, "u declares nothing") ;
    CHECK(service_graph_vertex(g, idx(g, "u"))->nrequiredby == 2, "w and w2 depend on u") ;
    CHECK(service_graph_vertex(g, idx(g, "dd"))->ndepends == 1, "dd depends on a once, Depends and OptsDepends merged") ;
}

static void test_order(service_graph_t const *g)
{
    printf("the start order holds every vertex that is not UNORDERED, a dependency first\n") ;

    uint32_t n = service_graph_nvertex(g), nsort = service_graph_nsort(g), unordered = 0 ;
    uint8_t seen[n] ;

    memset(seen, 0, n) ;

    for (uint32_t pos = 0 ; pos < nsort ; pos++) {
        uint32_t v = service_graph_sort(g, pos) ;
        CHECK(v < n, "position %u holds a vertex", pos) ;
        CHECK(!seen[v], "%s is once in the order", service_graph_name(g, v)) ;
        seen[v] = 1 ;
        CHECK(service_graph_state(g, v) != SERVICE_GRAPH_STATE_UNORDERED, "%s in the order is not UNORDERED", service_graph_name(g, v)) ;
    }

    for (uint32_t v = 0 ; v < n ; v++) {
        if (service_graph_state(g, v) == SERVICE_GRAPH_STATE_UNORDERED)
            unordered++ ;
        CHECK(seen[v] || service_graph_state(g, v) == SERVICE_GRAPH_STATE_UNORDERED, "%s is out of the order only when UNORDERED", service_graph_name(g, v)) ;
    }

    CHECK(nsort + unordered == n, "order of %u plus %u unordered is %u", nsort, unordered, n) ;
    CHECK(unordered == 4, "x, y, z and v are unordered: %u", unordered) ;

    // u2, unreadable on the cycle v declares through it, keeps its state and closes the order
    CHECK(service_graph_sort(g, nsort - 1) == idx(g, "u2"), "u2 closes the order") ;

    for (uint32_t pos = 0 ; pos + 1 < nsort ; pos++) {
        uint32_t v = service_graph_sort(g, pos) ;
        uint32_t const *d = service_graph_depends(g, v) ;
        for (uint32_t e = 0 ; e < service_graph_vertex(g, v)->ndepends ; e++)
            CHECK(sortpos(g, d[e]) < pos, "%s comes after its dependency %s", service_graph_name(g, v), service_graph_name(g, d[e])) ;
    }
}

static void test_accessors(service_graph_t const *g)
{
    printf("logger, parent of a logger, module\n") ;

    CHECK(service_graph_logger(g, idx(g, "lg")) == idx(g, "lg-log"), "lg-log is the logger of lg") ;
    CHECK(service_graph_logger(g, idx(g, "a")) == SERVICE_GRAPH_NOINDEX, "a has no logger") ;
    CHECK(service_graph_logger(g, idx(g, "x")) == SERVICE_GRAPH_NOINDEX, "x has no logger") ;
    CHECK(service_graph_logparent(g, idx(g, "lg-log")) == idx(g, "lg"), "lg-log is the logger of lg") ;
    CHECK(service_graph_logparent(g, idx(g, "lg")) == SERVICE_GRAPH_NOINDEX, "lg is no logger") ;
    CHECK(service_graph_logparent(g, idx(g, "ghost")) == SERVICE_GRAPH_NOINDEX, "ghost is no logger") ;
    CHECK(service_graph_module(g, idx(g, "mod:m1")) == idx(g, "mod"), "mod carries mod:m1") ;
    CHECK(service_graph_module(g, idx(g, "mod:m2")) == idx(g, "mod"), "mod carries mod:m2") ;
    CHECK(service_graph_module(g, idx(g, "a")) == SERVICE_GRAPH_NOINDEX, "no module carries a") ;
    CHECK(service_graph_module(g, idx(g, "mod")) == SERVICE_GRAPH_NOINDEX, "no module carries mod") ;
}

static void test_messages(service_graph_t const *g)
{
    printf("the line naming an invalid service, as the contract writes it\n") ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    struct { char const *name ; bool gone ; char const *line ; } expect[] = {
        { "u", false, "service: u has an unreadable resolve file, and is required by: w w2 -- repair it with 66 reconfigure -c u" },
        { "u", true, "service: u has an unreadable resolve file, and is required by: w w2 -- repair it with 66 reconfigure -c u" },
        { "u2", false, "service: u2 has an unreadable resolve file, and is required by: v -- repair it with 66 reconfigure -c u2" },
        { "lone", false, "service: lone has an unreadable resolve file -- repair it with 66 reconfigure -c lone" },
        { "x", false, "service: x cannot be ordered: it is part of, or depends on, a dependency cycle -- break the cycle in the frontends, then re-parse them with 66 parse -f" },
        { "z", true, "service: z cannot be ordered: it is part of, or depends on, a dependency cycle -- break the cycle in the frontends, then re-parse them with 66 parse -f" },
        { "a", true, "service: a has no frontend anywhere any more -- remove it with 66 remove a" },
        { "ghost", true, "service: ghost has neither a resolve file nor a frontend -- remove it with 66 remove ghost" },
    } ;

    for (size_t i = 0 ; i < sizeof(expect) / sizeof(expect[0]) ; i++) {
        CHECK(service_graph_invalid_message(&sa, g, idx(g, expect[i].name), expect[i].gone) == 1, "%s is invalid", expect[i].name) ;
        CHECK(strlen(sa.s) == sa.len, "the line of %s is a C string of its length", expect[i].name) ;
        CHECK(!strcmp(sa.s, expect[i].line), "line of %s:\n  got      %s\n  expected %s", expect[i].name, sa.s, expect[i].line) ;
    }

    // a valid service, and an absent one its frontend parses back: nothing written
    CHECK(strbuf_copys(&sa, "kept") && strbuf_uncounted(&sa), "seed the buffer") ;
    CHECK(service_graph_invalid_message(&sa, g, idx(g, "a"), false) == 0, "a is valid") ;
    CHECK(service_graph_invalid_message(&sa, g, idx(g, "ghost"), false) == 0, "ghost has a frontend") ;
    CHECK(sa.len == 4 && !strcmp(sa.s, "kept"), "nothing written for a valid vertex: %s", sa.s) ;

    // the dependents, appended: none for a vertex nothing depends on
    CHECK(service_graph_dependents(&sa, g, idx(g, "c")) == 1, "dependents of c: errno %d", errno) ;
    CHECK(sa.len == 4 && !strcmp(sa.s, "kept"), "nothing appended for c: %s", sa.s) ;
    CHECK(service_graph_dependents(&sa, g, idx(g, "a")) == 1, "dependents of a: errno %d", errno) ;
    CHECK(!strcmp(sa.s, "keptb dd o"), "the dependents of a, by index, after what was there: %s", sa.s) ;
}

static void test_overlay(char const *b)
{
    printf("the overlay replaces a resolve file, joins the system and provides first\n") ;

    service_graph_t g = SERVICE_GRAPH_ZERO ;
    hash_t overlay = HASH_ZERO ;
    svc_t sb = SVC_ZERO, sov = SVC_ZERO, su3 = SVC_ZERO ;

    sb.name = "b" ; sb.depends = "newdep" ;
    sov.name = "ov" ; sov.provide = "ovname alias" ;
    su3.name = "user3" ; su3.depends = "ovname" ;

    CHECK(hash_init(&overlay, 0, offsetof(struct resolve_hash_s, node)), "initialise the overlay") ;

    svc_t const *list[] = { &sb, &sov, &su3 } ;
    for (size_t i = 0 ; i < 3 ; i++) {
        resolve_service_t res = RESOLVE_SERVICE_ZERO ;
        fill_core(&res, list[i], b) ;
        CHECK(resolve_hash_add(&overlay, list[i]->name, res), "add %s to the overlay", list[i]->name) ;
        fill_dependencies(&resolve_hash_search(&overlay, list[i]->name)->dependencies, list[i]) ;
    }

    CHECK(service_graph_build(&g, b, &overlay) == 1, "build with an overlay: errno %d", errno) ;

    CHECK(dorigin(&g, "b", "newdep") == SERVICE_GRAPH_EDGE_DECLARED, "b depends on newdep, as the overlay says") ;
    CHECK(dorigin(&g, "b", "a") == NO_EDGE, "the disk Depends of b is replaced") ;
    CHECK(service_graph_state(&g, idx(&g, "newdep")) == SERVICE_GRAPH_STATE_ABSENT, "newdep is absent") ;
    CHECK(service_graph_state(&g, idx(&g, "ov")) == SERVICE_GRAPH_STATE_OK, "ov joins the system") ;
    CHECK(dorigin(&g, "user1", "ov") == SERVICE_GRAPH_EDGE_DECLARED, "alias resolves to ov of the overlay before its link") ;
    CHECK(dorigin(&g, "user1", "pv") == NO_EDGE, "the Provide link of alias is not followed") ;
    CHECK(dorigin(&g, "user3", "ov") == SERVICE_GRAPH_EDGE_DECLARED, "ovname resolves to ov") ;
    CHECK(service_graph_find(&g, "ovname") == SERVICE_GRAPH_NOINDEX, "ovname is no vertex") ;
    CHECK(service_graph_nvertex(&g) == 30, "27 vertexes, plus newdep, ov, user3, while alias leads to ov: %u", service_graph_nvertex(&g)) ;

    service_graph_free(&g) ;
    resolve_hash_free(&overlay) ;
}

static void test_overflow(void)
{
    printf("beyond SS_MAX_SERVICE names, the build refuses with EOVERFLOW\n") ;

    char b[96], big[SS_MAX_SERVICE * 5 + 1] ;
    service_graph_t g = SERVICE_GRAPH_ZERO ;
    svc_t s = SVC_ZERO ;

    auto_strings(b, base, "overflow/") ;
    CHECK(service_graph_write_end(0) == 1 && service_graph_write_begin(getuid(), b, b) == 1, "hold the transaction of %s", b) ;

    // SS_MAX_SERVICE - 1 names and big itself: the limit holds
    big[0] = 0 ;
    for (uint32_t i = 0 ; i < SS_MAX_SERVICE - 1 ; i++)
        sprintf(big + strlen(big), "%s%04u", i ? " " : "", i) ;

    s.name = "big" ;
    s.depends = big ;
    write_service(b, &s) ;

    CHECK(service_graph_build(&g, b, 0) == 1, "SS_MAX_SERVICE names build: errno %d", errno) ;
    CHECK(service_graph_nvertex(&g) == SS_MAX_SERVICE, "%u vertexes", service_graph_nvertex(&g)) ;
    CHECK(service_graph_vertex(&g, idx(&g, "big"))->ndepends == SS_MAX_SERVICE - 1, "big depends on every other name") ;
    service_graph_free(&g) ;

    // one more name
    char one[SS_MAX_PATH_LEN] ;
    svc_t more = SVC_ZERO ;
    auto_strings(one, "zzzz") ;
    more.name = one ;
    write_service(b, &more) ;

    errno = 0 ;
    CHECK(service_graph_build(&g, b, 0) == -1 && errno == EOVERFLOW, "SS_MAX_SERVICE + 1 names: -1 EOVERFLOW, errno %d", errno) ;
    CHECK(!g.vertex.s && !g.sort.s && !g.depends.s && !g.requiredby.s && !g.dorigin.s && !g.rorigin.s && !g.strings.s, "the graph is left empty") ;

    // the end builds the cache of the system it wrote: the overflow is said, the lock freed
    errno = 0 ;
    CHECK(service_graph_write_end(0) == 0 && errno == EOVERFLOW, "the end of an overflowed system: 0 EOVERFLOW, errno %d", errno) ;
    CHECK(service_graph_write_begin(getuid(), base, base) == 1, "hold the transaction of %s again", base) ;
}

static void test_cycle_logger(void)
{
    printf("a service on a dependency cycle keeps its logger: remove and reconfigure -c take it along\n") ;

    char b[96] ;
    service_graph_t g = SERVICE_GRAPH_ZERO ;
    svc_t cy = SVC_ZERO, log = SVC_ZERO ;

    auto_strings(b, base, "cyclelogger/") ;
    CHECK(service_graph_write_end(0) == 1 && service_graph_write_begin(getuid(), b, b) == 1, "hold the transaction of %s", b) ;

    cy.name = "cy" ;
    cy.depends = "cy cy-log" ;
    log.name = "cy-log" ;
    log.islog = 1 ;
    log.requiredby = "cy" ;
    write_service(b, &cy) ;
    write_service(b, &log) ;

    CHECK(service_graph_build(&g, b, 0) == 1, "build: errno %d", errno) ;
    CHECK(service_graph_state(&g, idx(&g, "cy")) == SERVICE_GRAPH_STATE_UNORDERED, "cy, on its own cycle, is UNORDERED") ;
    CHECK(service_graph_logger(&g, idx(&g, "cy")) == idx(&g, "cy-log"), "cy-log is the logger of cy, on a cycle or not") ;
    CHECK(service_graph_logparent(&g, idx(&g, "cy-log")) == idx(&g, "cy"), "cy is the service of cy-log") ;

    service_graph_free(&g) ;
    CHECK(service_graph_write_end(0) == 1 && service_graph_write_begin(getuid(), base, base) == 1, "hold the transaction of %s again", base) ;
}

static void test_edges_through_absent(void)
{
    printf("the edges between members go on through an absent vertex the propagation walked through\n") ;

    char b[96] ;
    service_graph_t g = SERVICE_GRAPH_ZERO ;
    selection_service_t sel = SELECTION_SERVICE_ZERO ;
    svc_t bottom = SVC_ZERO, top = SVC_ZERO ;
    uint32_t list[4] ;

    auto_strings(b, base, "absentorder/") ;
    CHECK(service_graph_write_end(0) == 1 && service_graph_write_begin(getuid(), b, b) == 1, "hold the transaction of %s", b) ;

    // the rows « absent b »: the bottom names them in its RequiredBy, the top depends on them and on the bottom
    bottom.name = "bot" ;
    bottom.requiredby = "gap gap2" ;
    top.name = "top" ;
    top.depends = "bot gap gap2" ;
    write_service(b, &bottom) ;
    write_service(b, &top) ;

    CHECK(service_graph_build(&g, b, 0) == 1, "build: errno %d", errno) ;
    CHECK(service_graph_state(&g, idx(&g, "gap")) == SERVICE_GRAPH_STATE_ABSENT && service_graph_state(&g, idx(&g, "gap2")) == SERVICE_GRAPH_STATE_ABSENT, "gap and gap2 are absent") ;

    propagate_names(&sel, &g, (char const *[]){ "bot" }, 1, SELECTION_SERVICE_PROPAGATE_REQUIREDBY, false, 0, false) ;
    CHECK(sel.member.count == 2 && bitset32_isvalid(&sel.met, idx(&g, "gap")) && bitset32_isvalid(&sel.met, idx(&g, "gap2")), "bot and top taken, gap and gap2 walked through") ;
    CHECK(selection_service_edges(&sel, idx(&g, "bot"), list, true) == 1 && list[0] == idx(&g, "top"), "top waits on bot through the absent ones, stored once") ;
    CHECK(selection_service_edges(&sel, idx(&g, "top"), list, false) == 1 && list[0] == idx(&g, "bot"), "bot is a dependency of top, stored once") ;

    service_graph_free(&g) ;
    auto_strings(b, base, "absentorder2/") ;
    CHECK(service_graph_write_end(0) == 1 && service_graph_write_begin(getuid(), b, b) == 1, "hold the transaction of %s", b) ;

    // the top through the absent one only
    top.depends = "gap" ;
    bottom.requiredby = "gap" ;
    write_service(b, &bottom) ;
    write_service(b, &top) ;
    CHECK(service_graph_build(&g, b, 0) == 1, "build: errno %d", errno) ;

    propagate_names(&sel, &g, (char const *[]){ "bot" }, 1, SELECTION_SERVICE_PROPAGATE_REQUIREDBY, false, 0, false) ;
    CHECK(sel.member.count == 2 && bitset32_isvalid(&sel.member, idx(&g, "top")), "top reached through gap") ;
    CHECK(selection_service_edges(&sel, idx(&g, "bot"), list, true) == 1 && list[0] == idx(&g, "top"), "top is behind gap") ;
    CHECK(selection_service_edges(&sel, idx(&g, "top"), list, false) == 1 && list[0] == idx(&g, "bot"), "bot is behind gap") ;

    // -P: gap is not walked through, no order across it
    propagate_names(&sel, &g, (char const *[]){ "bot", "top" }, 2, SELECTION_SERVICE_PROPAGATE_NONE, false, 0, false) ;
    CHECK(sel.member.count == 2 && !bitset32_isvalid(&sel.met, idx(&g, "gap")), "gap is not met under -P") ;
    CHECK(selection_service_edges(&sel, idx(&g, "bot"), list, true) == 0, "nothing goes through gap when it is not met") ;

    service_graph_free(&g) ;
    CHECK(service_graph_write_end(0) == 1 && service_graph_write_begin(getuid(), base, base) == 1, "hold the transaction of %s again", base) ;
}

static void test_bad_provide(void)
{
    printf("a damaged Provide entry is no provider: it is ignored\n") ;

    char b[96], file[SS_MAX_PATH_LEN], link[SS_MAX_PATH_LEN] ;
    service_graph_t g = SERVICE_GRAPH_ZERO ;
    svc_t s = SVC_ZERO ;
    bitset32_t named ;
    int unknown = -1 ;
    char const *argv[] = { "bad", "empty" } ;

    auto_strings(b, base, "badprovide/") ;
    CHECK(service_graph_write_end(0) == 1 && service_graph_write_begin(getuid(), b, b) == 1, "hold the transaction of %s", b) ;

    s.name = "s" ;
    s.depends = "bad empty" ;
    write_service(b, &s) ;
    auto_strings(file, b, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, SS_PROVIDE) ;
    CHECK(dir_create_parent(file, 0755), "create the Provide directory") ;
    auto_strings(link, file, "/empty") ;
    auto_strings(file + strlen(file), "/bad") ;
    CHECK(file_write(file, "x", 1), "write a regular file in the Provide directory") ;
    CHECK(!symlink("../", link), "write a link to nothing in the Provide directory") ;

    CHECK(service_graph_build(&g, b, 0) == 1, "the build goes on, errno %d", errno) ;
    CHECK(idx(&g, "bad") != SERVICE_GRAPH_NOINDEX && service_graph_state(&g, idx(&g, "bad")) == SERVICE_GRAPH_STATE_ABSENT, "bad is taken literally, absent") ;
    CHECK(idx(&g, "empty") != SERVICE_GRAPH_NOINDEX && service_graph_state(&g, idx(&g, "empty")) == SERVICE_GRAPH_STATE_ABSENT, "empty is taken literally, absent") ;

    CHECK(selection_service_names(&named, &g, b, argv, 2, true, &unknown) == 1, "the names answer for themselves") ;
    CHECK(bitset32_isvalid(&named, idx(&g, "bad")) && bitset32_isvalid(&named, idx(&g, "empty")), "both are named") ;

    service_graph_free(&g) ;
    CHECK(service_graph_write_end(0) == 1 && service_graph_write_begin(getuid(), base, base) == 1, "hold the transaction of %s again", base) ;
}

// @a and @b hold the same vertexes, order, edges and origins
static void same_graph(service_graph_t const *a, service_graph_t const *b, char const *what)
{
    CHECK(service_graph_nvertex(a) == service_graph_nvertex(b), "%s: same count of vertexes", what) ;
    CHECK(service_graph_nsort(a) == service_graph_nsort(b), "%s: same length of order", what) ;

    for (uint32_t i = 0 ; i < service_graph_nvertex(a) ; i++) {

        service_graph_vertex_t const *va = service_graph_vertex(a, i), *vb = service_graph_vertex(b, i) ;

        CHECK(!strcmp(service_graph_name(a, i), service_graph_name(b, i)), "%s: same name at %u", what, i) ;
        CHECK(va->state == vb->state && va->ndepends == vb->ndepends && va->nrequiredby == vb->nrequiredby, "%s: same vertex %u", what, i) ;
        CHECK(!memcmp(service_graph_depends(a, i), service_graph_depends(b, i), va->ndepends * sizeof(uint32_t)), "%s: same depends of %u", what, i) ;
        CHECK(!memcmp(service_graph_dorigin(a, i), service_graph_dorigin(b, i), va->ndepends), "%s: same origins of %u", what, i) ;
        CHECK(!memcmp(service_graph_requiredby(a, i), service_graph_requiredby(b, i), va->nrequiredby * sizeof(uint32_t)), "%s: same requiredby of %u", what, i) ;
        CHECK(!memcmp(service_graph_rorigin(a, i), service_graph_rorigin(b, i), va->nrequiredby), "%s: same rorigins of %u", what, i) ;
    }

    for (uint32_t pos = 0 ; pos < service_graph_nsort(a) ; pos++)
        CHECK(service_graph_sort(a, pos) == service_graph_sort(b, pos), "%s: same order at %u", what, pos) ;
}

static void test_load(service_graph_t const *built)
{
    printf("load and reload inside a write transaction build the same graph\n") ;

    service_graph_t g = SERVICE_GRAPH_ZERO ;

    CHECK(service_graph_load(&g, getuid(), base, base) == 1, "load") ;
    same_graph(&g, built, "load") ;

    CHECK(service_graph_reload(&g, getuid(), base, base) == 1, "reload over a loaded graph") ;
    same_graph(&g, built, "reload") ;

    service_graph_free(&g) ;
    CHECK(!g.vertex.s && !g.vertex.len && !g.sort.s && !g.depends.s && !g.requiredby.s && !g.dorigin.s && !g.rorigin.s && !g.strings.s, "free leaves SERVICE_GRAPH_ZERO") ;
}

/**
 *
 * The selection
 *
 * */

static void test_names(service_graph_t const *g)
{
    printf("names to indexes: a provided name, an unknown one\n") ;

    bitset32_t named ;
    int unknown = -1 ;
    char longname[SS_MAX_SERVICE_NAME + 2] ;

    char const *ok[] = { "c", "alias", "c" } ;
    CHECK(selection_service_names(&named, g, base, ok, 3, true, &unknown) == 1, "c and alias are known") ;
    CHECK(count_bits(g, &named) == 2 && bitset32_isvalid(&named, idx(g, "c")) && bitset32_isvalid(&named, idx(g, "pv")), "alias designates pv") ;
    CHECK(unknown == -1, "unknown untouched") ;

    char const *literal[] = { "c", "alias" } ;
    CHECK(selection_service_names(&named, g, base, literal, 2, false, &unknown) == 0, "alias taken literally is unknown") ;
    CHECK(unknown == 1, "argv[1] is the unknown one: %d", unknown) ;
    CHECK(count_bits(g, &named) == 1 && bitset32_isvalid(&named, idx(g, "c")), "named holds the operands before it") ;

    char const *none[] = { "nothing" } ;
    unknown = -1 ;
    CHECK(selection_service_names(&named, g, base, none, 1, true, &unknown) == 0 && unknown == 0, "nothing is unknown") ;

    char const *empty[] = { "" } ;
    unknown = -1 ;
    CHECK(selection_service_names(&named, g, base, empty, 1, true, &unknown) == 0 && unknown == 0, "the empty name is unknown") ;

    memset(longname, 'l', SS_MAX_SERVICE_NAME + 1) ;
    longname[SS_MAX_SERVICE_NAME + 1] = 0 ;
    char const *toolong[] = { longname } ;
    unknown = -1 ;
    CHECK(selection_service_names(&named, g, base, toolong, 1, true, &unknown) == 0 && unknown == 0, "a name beyond SS_MAX_SERVICE_NAME is unknown") ;

    CHECK(selection_service_names(&named, g, base, 0, 0, true, &unknown) == 1 && count_bits(g, &named) == 0, "no operand") ;
    CHECK(named.size == BITSET32_WORDS_SIZE(service_graph_nvertex(g)), "named is sized to the graph") ;
}

static void test_close(service_graph_t const *g)
{
    printf("propagations in the three directions\n") ;

    selection_service_t sel = SELECTION_SERVICE_ZERO ;
    char const *c[] = { "c" } ;

    propagate_names(&sel, g, c, 1, SELECTION_SERVICE_PROPAGATE_DEPENDS, false, 0, false) ;
    CHECK(sel.g == g, "the selection borrows the graph") ;
    members_are(&sel, (char const *[]){ "c", "b", "a", "q" }, 4, "depends of c") ;
    CHECK(count_bits(g, &sel.met) == 1 && bitset32_isvalid(&sel.met, idx(g, "ghost")), "ghost is walked through, never taken") ;
    CHECK(count_bits(g, &sel.named) == 1 && bitset32_isvalid(&sel.named, idx(g, "c")), "c is named") ;

    propagate_names(&sel, g, c, 1, SELECTION_SERVICE_PROPAGATE_NONE, false, 0, false) ;
    members_are(&sel, c, 1, "c alone") ;
    CHECK(count_bits(g, &sel.met) == 0, "nothing walked") ;

    char const *a[] = { "a" } ;
    propagate_names(&sel, g, a, 1, SELECTION_SERVICE_PROPAGATE_REQUIREDBY, false, 0, false) ;
    members_are(&sel, (char const *[]){ "a", "b", "c", "o", "dd" }, 5, "requiredby of a") ;

    // the seeds may be the named of the selection itself
    selection_service_policy_t policy = { .propagate = SELECTION_SERVICE_PROPAGATE_DEPENDS } ;
    bitset32_t seeds = seeds_of(g, (char const *[]){ "b" }, 1) ;
    selection_service_propagate(&sel, g, &seeds, &policy) ;
    selection_service_propagate(&sel, g, &sel.named, &policy) ;
    members_are(&sel, (char const *[]){ "b", "a" }, 2, "depends of b drawn again from its own named") ;
}

static void test_close_logger_module(service_graph_t const *g)
{
    printf("a logger by the policy, a module with its members, a member alone\n") ;

    selection_service_t sel = SELECTION_SERVICE_ZERO ;
    char const *lg[] = { "lg" } ;
    char const *mod[] = { "mod" } ;
    char const *m1[] = { "mod:m1" } ;

    propagate_names(&sel, g, lg, 1, SELECTION_SERVICE_PROPAGATE_NONE, false, 0, true) ;
    members_are(&sel, (char const *[]){ "lg", "lg-log" }, 2, "lg with its logger under -P") ;

    propagate_names(&sel, g, lg, 1, SELECTION_SERVICE_PROPAGATE_NONE, false, 0, false) ;
    members_are(&sel, lg, 1, "lg without the logger policy") ;

    propagate_names(&sel, g, mod, 1, SELECTION_SERVICE_PROPAGATE_NONE, false, 0, false) ;
    members_are(&sel, (char const *[]){ "mod", "mod:m1", "mod:m2" }, 3, "a module comes with its members under -P") ;

    propagate_names(&sel, g, m1, 1, SELECTION_SERVICE_PROPAGATE_REQUIREDBY, false, 0, false) ;
    members_are(&sel, m1, 1, "a member does not drag its module in") ;

    propagate_names(&sel, g, m1, 1, SELECTION_SERVICE_PROPAGATE_DEPENDS, false, 0, false) ;
    members_are(&sel, m1, 1, "a member depends on nothing") ;
}

static void test_close_from(service_graph_t const *g)
{
    printf("From edges by the policy, reactors of a source in the requiredby direction\n") ;

    selection_service_t sel = SELECTION_SERVICE_ZERO ;
    char const *re[] = { "re" } ;
    char const *tk[] = { "tk" } ;
    char const *src[] = { "src" } ;
    bitset32_t reactors = seeds_of(g, src, 1) ;

    propagate_names(&sel, g, re, 1, SELECTION_SERVICE_PROPAGATE_DEPENDS, false, 0, false) ;
    members_are(&sel, re, 1, "a reactor without the from policy") ;

    propagate_names(&sel, g, re, 1, SELECTION_SERVICE_PROPAGATE_DEPENDS, true, 0, false) ;
    members_are(&sel, (char const *[]){ "re", "src" }, 2, "a reactor brings its source under the from policy") ;

    propagate_names(&sel, g, tk, 1, SELECTION_SERVICE_PROPAGATE_DEPENDS, false, 0, false) ;
    members_are(&sel, (char const *[]){ "tk", "src" }, 2, "a declared From is a dependency") ;

    propagate_names(&sel, g, src, 1, SELECTION_SERVICE_PROPAGATE_REQUIREDBY, false, 0, false) ;
    members_are(&sel, (char const *[]){ "src", "tk" }, 2, "a source without reactors policy") ;

    propagate_names(&sel, g, src, 1, SELECTION_SERVICE_PROPAGATE_REQUIREDBY, false, &reactors, false) ;
    members_are(&sel, (char const *[]){ "src", "tk", "re" }, 3, "a source of the reactors set drags its reactors") ;

    propagate_names(&sel, g, src, 1, SELECTION_SERVICE_PROPAGATE_NONE, true, &reactors, false) ;
    members_are(&sel, src, 1, "under -P a source goes alone") ;

    propagate_names(&sel, g, src, 1, SELECTION_SERVICE_PROPAGATE_DEPENDS, true, &reactors, false) ;
    members_are(&sel, src, 1, "reactors is read under requiredby only") ;
}

static void test_close_invalid(service_graph_t const *g)
{
    printf("an invalid vertex is taken and walked, an absent seed is walked through\n") ;

    selection_service_t sel = SELECTION_SERVICE_ZERO ;

    propagate_names(&sel, g, (char const *[]){ "u" }, 1, SELECTION_SERVICE_PROPAGATE_REQUIREDBY, false, 0, false) ;
    members_are(&sel, (char const *[]){ "u", "w", "w2" }, 3, "what depends on an unreadable service") ;

    propagate_names(&sel, g, (char const *[]){ "w" }, 1, SELECTION_SERVICE_PROPAGATE_DEPENDS, false, 0, false) ;
    members_are(&sel, (char const *[]){ "w", "u" }, 2, "an unreadable dependency is taken") ;

    propagate_names(&sel, g, (char const *[]){ "z" }, 1, SELECTION_SERVICE_PROPAGATE_DEPENDS, false, 0, false) ;
    members_are(&sel, (char const *[]){ "z", "x", "y" }, 3, "a cycle is walked") ;

    propagate_names(&sel, g, (char const *[]){ "ghost" }, 1, SELECTION_SERVICE_PROPAGATE_REQUIREDBY, false, 0, false) ;
    members_are(&sel, (char const *[]){ "c" }, 1, "what depends on an absent seed") ;
    CHECK(count_bits(g, &sel.met) == 1 && bitset32_isvalid(&sel.met, idx(g, "ghost")), "the absent seed is met") ;
    CHECK(bitset32_isvalid(&sel.named, idx(g, "ghost")), "the absent seed is named") ;
}

static void test_edges_carried_remove(service_graph_t const *g)
{
    printf("edges inside a selection, a member carried by its module, removal\n") ;

    selection_service_t sel = SELECTION_SERVICE_ZERO ;
    uint32_t list[8] ;

    propagate_names(&sel, g, (char const *[]){ "c" }, 1, SELECTION_SERVICE_PROPAGATE_DEPENDS, false, 0, false) ;
    CHECK(selection_service_edges(&sel, idx(g, "c"), list, false) == 2, "c has two edges to members, ghost out") ;
    CHECK(list[0] == idx(g, "b") && list[1] == idx(g, "q"), "b then q, by index") ;
    CHECK(selection_service_edges(&sel, idx(g, "a"), list, true) == 1 && list[0] == idx(g, "b"), "of what depends on a, b only is a member") ;

    selection_service_remove(&sel, idx(g, "b")) ;
    CHECK(sel.member.count == 3 && !bitset32_isvalid(&sel.member, idx(g, "b")), "b removed") ;
    selection_service_remove(&sel, idx(g, "b")) ;
    CHECK(sel.member.count == 3, "a vertex no longer a member is left alone") ;
    selection_service_remove(&sel, idx(g, "x")) ;
    CHECK(sel.member.count == 3, "a vertex never a member is left alone") ;
    CHECK(selection_service_edges(&sel, idx(g, "c"), list, false) == 1 && list[0] == idx(g, "q"), "b is out of the edges") ;

    propagate_names(&sel, g, (char const *[]){ "mod" }, 1, SELECTION_SERVICE_PROPAGATE_NONE, false, 0, false) ;
    CHECK(selection_service_carried(&sel, idx(g, "mod:m1")) == 1, "mod carries mod:m1") ;
    CHECK(selection_service_carried(&sel, idx(g, "mod")) == 0, "nothing carries mod") ;
    CHECK(selection_service_carried(&sel, idx(g, "a")) == 0, "nothing carries a") ;

    propagate_names(&sel, g, (char const *[]){ "mod:m1" }, 1, SELECTION_SERVICE_PROPAGATE_NONE, false, 0, false) ;
    CHECK(selection_service_carried(&sel, idx(g, "mod:m1")) == 0, "mod is no member: it carries nothing") ;
}

/**
 *
 * Frontends, in a user namespace and a chroot
 *
 * */

static void frontend_child(service_graph_t const *g, char const *root)
{
    char path[SS_MAX_PATH_LEN], map[64] ;
    uid_t uid = getuid() ;
    gid_t gid = getgid() ;
    _cleanup_strbuf_ strbuf out = STRBUF_ZERO ;
    bitset32_t skip = bitset32_init(SS_MAX_SERVICE) ;

    CHECK(unshare(CLONE_NEWUSER) == 0, "unshare a user namespace: errno %d", errno) ;
    CHECK(file_write("/proc/self/setgroups", "deny", 4), "deny setgroups") ;
    sprintf(map, "0 %u 1", (unsigned)uid) ;
    CHECK(file_write("/proc/self/uid_map", map, strlen(map)), "map uid") ;
    sprintf(map, "0 %u 1", (unsigned)gid) ;
    CHECK(file_write("/proc/self/gid_map", map, strlen(map)), "map gid") ;
    CHECK(chroot(root) == 0 && chdir("/") == 0, "chroot to %s: errno %d", root, errno) ;

    // owner 0: SS_SERVICE_ADMDIR, then SS_SERVICE_SYSDIR, both empty but fe
    auto_strings(path, SS_SERVICE_ADMDIR, "a") ;
    CHECK(file_write(path, "", 0), "write the frontend of a") ;

    CHECK(service_graph_frontend_gone(g, idx(g, "a"), &(strbuf)STRBUF_ZERO) == 0, "the frontend of a is found") ;
    CHECK(service_graph_frontend_gone(g, idx(g, "b"), &(strbuf)STRBUF_ZERO) == 1, "the frontend of b is gone") ;
    CHECK(service_graph_frontend_gone(g, idx(g, "ghost"), &(strbuf)STRBUF_ZERO) == 1, "ghost has no frontend") ;
    CHECK(service_graph_frontend_gone(g, idx(g, "lg-log"), &(strbuf)STRBUF_ZERO) == 0, "a logger has no frontend of its own") ;
    CHECK(service_graph_frontend_gone(g, idx(g, "mod:m1"), &(strbuf)STRBUF_ZERO) == 0, "a member has no frontend of its own") ;

    // the warnings at verbosity 2, written on the standard error
    CHECK(file_write("/warn", "", 0), "create /warn") ;
    int fd = (int)io_open_mode("/warn", O_WRONLY | O_APPEND, 0) ;
    int saved = dup(2) ;
    CHECK(fd >= 0 && saved >= 0 && dup2(fd, 2) == 2, "redirect the standard error") ;
    VERBOSITY = 2 ;
    bitset32_set(&skip, idx(g, "u")) ;
    int r = service_graph_warn_invalid(g, &(strbuf)STRBUF_ZERO, &skip) ;
    VERBOSITY = 0 ;
    CHECK(dup2(saved, 2) == 2, "restore the standard error") ;
    CHECK(r == 1, "warn: errno %d", errno) ;
    CHECK(strbuf_read_file(&out, "/warn"), "read the warnings") ;

    char const *lines[] = {
        "service: dang has neither a resolve file nor a frontend -- remove it with 66 remove dang",
        "service: ghost has neither a resolve file nor a frontend -- remove it with 66 remove ghost",
        "service: lone has an unreadable resolve file -- repair it with 66 reconfigure -c lone",
        "service: u2 has an unreadable resolve file, and is required by: v -- repair it with 66 reconfigure -c u2",
        "service: v cannot be ordered: it is part of, or depends on, a dependency cycle -- break the cycle in the frontends, then re-parse them with 66 parse -f",
        "service: x cannot be ordered: it is part of, or depends on, a dependency cycle -- break the cycle in the frontends, then re-parse them with 66 parse -f",
        "service: y cannot be ordered: it is part of, or depends on, a dependency cycle -- break the cycle in the frontends, then re-parse them with 66 parse -f",
        "service: z cannot be ordered: it is part of, or depends on, a dependency cycle -- break the cycle in the frontends, then re-parse them with 66 parse -f",
    } ;
    size_t nlines = sizeof(lines) / sizeof(lines[0]), count = 0 ;
    char const *p = out.s ;

    for (size_t i = 0 ; i < nlines ; i++) {
        char const *at = strstr(p, lines[i]) ;
        CHECK(at, "warning %zu in order: %s\n--- got:\n%s", i, lines[i], out.s) ;
        p = at + strlen(lines[i]) ;
    }

    for (p = out.s ; (p = strstr(p, "service: ")) ; p++)
        count++ ;

    CHECK(count == nlines, "%zu warnings, expected %zu: the skipped u and the OK services say nothing\n%s", count, nlines, out.s) ;

    count = 0 ;
    for (p = out.s ; (p = strchr(p, '\n')) ; p++)
        count++ ;

    CHECK(count == nlines, "%zu lines, expected %zu: nothing else is said\n%s", count, nlines, out.s) ;

    _exit(0) ;
}

static void test_frontends(service_graph_t const *g)
{
    printf("frontend gone and the warnings, in a chroot of a user namespace\n") ;

    char root[64] = "/tmp/66-service-graph-root.XXXXXX" ;
    char path[SS_MAX_PATH_LEN] ;
    int status = 0 ;

    CHECK(mkdtemp(root), "make the root") ;
    auto_strings(path, root, SS_SERVICE_ADMDIR) ;
    CHECK(dir_create_parent(path, 0755), "create %s", path) ;
    auto_strings(path, root, SS_SERVICE_SYSDIR) ;
    CHECK(dir_create_parent(path, 0755), "create %s", path) ;

    fflush(stdout) ;
    pid_t pid = fork() ;
    CHECK(pid >= 0, "fork") ;

    if (!pid)
        frontend_child(g, root) ;

    CHECK(waitpid(pid, &status, 0) == pid, "wait the child") ;
    CHECK(WIFEXITED(status) && !WEXITSTATUS(status), "the child passed: status %d", status) ;
    CHECK(dir_destroy(root), "remove the root") ;
}

int main(void)
{
    printf("Starting service graph test...\n") ;

    service_graph_t g = SERVICE_GRAPH_ZERO ;

    PROG = "service_graph" ;
    VERBOSITY = 0 ;
    CHECK(mkdtemp(base), "make the base") ;
    strcat(base, "/") ;

    // resolve_write() writes the system: the test holds its write transaction
    CHECK(service_graph_write_begin(getuid(), base, base) == 1, "hold the transaction") ;

    test_empty() ;
    write_system(base) ;

    CHECK(service_graph_build(&g, base, 0) == 1, "build the system: errno %d", errno) ;

    test_vertexes(&g) ;
    test_edges(&g) ;
    test_order(&g) ;
    test_accessors(&g) ;
    test_messages(&g) ;
    test_load(&g) ;
    test_names(&g) ;
    test_close(&g) ;
    test_close_logger_module(&g) ;
    test_close_from(&g) ;
    test_close_invalid(&g) ;
    test_edges_carried_remove(&g) ;
    test_edges_through_absent() ;
    test_frontends(&g) ;
    test_overlay(base) ;
    test_overflow() ;
    test_bad_provide() ;
    test_cycle_logger() ;

    service_graph_free(&g) ;

    CHECK(service_graph_write_end(0) == 1, "release the transaction") ;
    CHECK(dir_destroy(base), "remove the base") ;

    printf("All tests passed!\n") ;
    return 0 ;
}
