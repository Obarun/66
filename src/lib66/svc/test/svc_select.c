/*
 * svc_select.c
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

/** The state of a service and the steps of a command that do not reach a
 * supervisor, by effect: supervised and running as the contract defines
 * them, the filters drawn from them, the lot by stage, the conflict in its two
 * modes, the context of the launcher (order, edges, state). A system is
 * written under a private mkdtemp directory with the write transaction and
 * resolve_write(); the state and status records a supervisor would write are
 * written by hand at the live paths the resolve files name. Nothing runs:
 * svc_send() is only called on what it refuses or has nothing to drive. */

#include <errno.h>
#include <fcntl.h>
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
#include <oblibs/io.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/state.h>
#include <66/status.h>
#include <66/svc.h>
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

static char base[64] = "/tmp/66-svc-select.XXXXXX" ;

/**
 *
 * Fixtures
 *
 * */

typedef struct fixture_s fixture_t ;
struct fixture_s
{
    char const *name ;
    uint32_t type ;
    uint32_t earlier ;
    uint32_t enabled ;
    char const *depends ;
    char const *contents ;
    char const *conflict ;
    uint32_t docmd ; // EVENT_DO_*: an event addon, EVENT_DO_NONE for none
} ;

static uint32_t count_words(char const *list)
{
    uint32_t n = 0 ;
    for (char const *p = list ; *p ; p++)
        if (*p != ' ' && (p == list || p[-1] == ' '))
            n++ ;
    return n ;
}

// the live paths of @name: the state file the parse writes, the live state directory, the supervise directory
static void live_paths(char const *name, char *status, char *statedir, char *supervisedir)
{
    auto_strings(status, base, "state/", name) ;
    auto_strings(statedir, base, "live/", name, "/state") ;
    auto_strings(supervisedir, base, "sup/", name) ;
}

static void make_layout(char const *name)
{
    char entry[SS_MAX_PATH_LEN], svcdir[SS_MAX_PATH_LEN], svcres[SS_MAX_PATH_LEN], link[SS_MAX_PATH_LEN] ;
    auto_strings(entry, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE) ;
    auto_strings(svcdir, base, SS_SYSTEM, SS_SERVICE, SS_SVC, "/", name) ;
    auto_strings(svcres, svcdir, SS_RESOLVE) ;
    auto_strings(link, entry, "/", name) ;

    CHECK(dir_create_parent(entry, 0755), "create %s", entry) ;
    CHECK(dir_create_parent(svcres, 0755), "create %s", svcres) ;
    CHECK(symlink(svcdir, link) == 0, "link %s", link) ;
}

static void write_service(fixture_t const *f)
{
    char status[SS_MAX_PATH_LEN], statedir[SS_MAX_PATH_LEN], supervisedir[SS_MAX_PATH_LEN] ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t w = { .type = DATA_SERVICE, .obj = &res } ;

    make_layout(f->name) ;
    live_paths(f->name, status, statedir, supervisedir) ;

    resolve_init(&w) ;
    res.name = resolve_add_string(&w, f->name) ;
    res.path.home = resolve_add_string(&w, base) ;
    res.type = f->type ;
    res.earlier = f->earlier ;
    res.enabled = f->enabled ;
    res.live.status = resolve_add_string(&w, status) ;
    res.live.statedir = resolve_add_string(&w, statedir) ;
    res.live.supervisedir = resolve_add_string(&w, supervisedir) ;
    CHECK(resolve_write(&w, base, f->name) == 1, "write the resolve file of %s", f->name) ;
    strbuf_free(&res.sa) ;

    if (*f->depends || *f->contents || *f->conflict) {

        resolve_service_addon_dependencies_t dep = RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
        resolve_wrapper_t wd = { .type = DATA_SERVICE_DEPENDENCIES, .obj = &dep } ;

        resolve_init(&wd) ;
        dep.depends = resolve_add_string(&wd, f->depends) ;
        dep.ndepends = count_words(f->depends) ;
        dep.requiredby = resolve_add_string(&wd, "") ;
        dep.optsdeps = resolve_add_string(&wd, "") ;
        dep.contents = resolve_add_string(&wd, f->contents) ;
        dep.ncontents = count_words(f->contents) ;
        dep.provide = resolve_add_string(&wd, "") ;
        dep.conflict = resolve_add_string(&wd, f->conflict) ;
        dep.nconflict = count_words(f->conflict) ;
        CHECK(resolve_write(&wd, base, f->name) == 1, "write the dependencies of %s", f->name) ;
        strbuf_free(&dep.sa) ;
    }

    if (f->docmd != EVENT_DO_NONE) {

        resolve_service_addon_event_t ev = RESOLVE_SERVICE_ADDON_EVENT_ZERO ;
        resolve_wrapper_t we = { .type = DATA_SERVICE_EVENT, .obj = &ev } ;

        resolve_init(&we) ;
        ev.type = EVENT_SOURCE_USER ;
        ev.from = resolve_add_string(&we, "") ;
        ev.docmd = f->docmd ;
        CHECK(resolve_write(&we, base, f->name) == 1, "write the event addon of %s", f->name) ;
        strbuf_free(&ev.sa) ;
    }
}

static void write_garbage(char const *name)
{
    char file[SS_MAX_PATH_LEN] ;
    make_layout(name) ;
    auto_strings(file, base, SS_SYSTEM, SS_SERVICE, SS_SVC, "/", name, SS_RESOLVE, "/", name) ;
    CHECK(file_write(file, "garbage", 7), "write garbage at %s", file) ;
}

static void write_provide(char const *name, char const *target)
{
    char dir[SS_MAX_PATH_LEN], link[SS_MAX_PATH_LEN] ;
    auto_strings(dir, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, SS_PROVIDE) ;
    auto_strings(link, dir, "/", name) ;
    CHECK(dir_create_parent(dir, 0755), "create %s", dir) ;
    CHECK(symlink(target, link) == 0, "link %s", link) ;
}

/* the state record of @name as the parse writes it in <base>, or under @live
 * the copy in its live state directory */
static void write_state(char const *name, uint32_t issupervised, int live)
{
    char status[SS_MAX_PATH_LEN], statedir[SS_MAX_PATH_LEN], supervisedir[SS_MAX_PATH_LEN], file[SS_MAX_PATH_LEN] ;
    char pack[STATE_STATE_SIZE] ;
    ss_state_t sta = STATE_ZERO ;

    live_paths(name, status, statedir, supervisedir) ;

    if (live) {
        CHECK(dir_create_parent(statedir, 0755), "create %s", statedir) ;
        auto_strings(file, statedir, "/", SS_STATUS) ;
    } else
        auto_strings(file, status) ;

    sta.issupervised = issupervised ;
    state_pack(pack, &sta) ;
    CHECK(file_write(file, pack, STATE_STATE_SIZE), "write the state of %s", name) ;
}

// the status record a supervisor writes for @name
static void write_status(char const *name, uint8_t state, uint32_t pid)
{
    char status[SS_MAX_PATH_LEN], statedir[SS_MAX_PATH_LEN], supervisedir[SS_MAX_PATH_LEN], file[SS_MAX_PATH_LEN] ;
    service_status_t st = STATUS_ZERO ;

    live_paths(name, status, statedir, supervisedir) ;
    CHECK(dir_create_parent(supervisedir, 0755), "create %s", supervisedir) ;
    auto_strings(file, supervisedir, "/", SS_STATUS) ;

    st.state = state ;
    st.pid = pid ;
    CHECK(status_write(&st, file), "write the status of %s", name) ;
}

static void write_system(void)
{
    fixture_t s[] = {
        { .name = "a" },
        { .name = "b", .depends = "a" },
        { .name = "c", .depends = "b" },
        { .name = "mod", .type = E_PARSER_TYPE_MODULE, .contents = "mod:e1 mod:m2" },
        { .name = "mod:e1", .earlier = 1 },
        { .name = "mod:m2" },
        { .name = "early", .type = E_PARSER_TYPE_MODULE, .contents = "early:e" },
        { .name = "early:e", .earlier = 1 },
        { .name = "boot1", .earlier = 1 },
        { .name = "pc", .conflict = "pb" },
        { .name = "pb" },
        { .name = "pn", .conflict = "pv-net" },
        { .name = "pv-conn", .enabled = 1 },
        { .name = "pu", .conflict = "u" },
        { .name = "pg", .conflict = "nosuch" },
        { .name = "react", .type = E_PARSER_TYPE_ONESHOT, .docmd = EVENT_DO_START },
        { .name = "rcl", .docmd = EVENT_DO_RESTART },
        { .name = "rstop", .docmd = EVENT_DO_STOP },
        { .name = "rfree", .docmd = EVENT_DO_START },
        { .name = "rmod", .type = E_PARSER_TYPE_MODULE, .docmd = EVENT_DO_START },
        { .name = "one", .type = E_PARSER_TYPE_ONESHOT },
        { .name = "loop" },
        { .name = "bad" },
        { .name = "lv" },
        { .name = "badst" },
        { .name = "x", .depends = "y" },
        { .name = "y", .depends = "x" },
        { .name = "z", .depends = "x a" },
    } ;

    char dir[SS_MAX_PATH_LEN] ;
    auto_strings(dir, base, "state") ;
    CHECK(dir_create_parent(dir, 0755), "create %s", dir) ;

    for (size_t i = 0 ; i < sizeof(s) / sizeof(s[0]) ; i++) {
        fixture_t f = s[i] ;
        f.type = f.type ? f.type : (uint32_t)E_PARSER_TYPE_CLASSIC ;
        f.depends = f.depends ? f.depends : "" ;
        f.contents = f.contents ? f.contents : "" ;
        f.conflict = f.conflict ? f.conflict : "" ;
        write_service(&f) ;
    }

    write_garbage("u") ;
    write_provide("pv-net", "../pv-conn") ;

    // a classic up, a classic down, one never supervised
    write_state("a", STATE_FLAGS_TRUE, 0) ;
    write_status("a", STATUS_STATE_UP, 123) ;
    write_state("b", STATE_FLAGS_TRUE, 0) ;
    write_status("b", STATUS_STATE_DOWN, 0) ;
    write_state("c", STATE_FLAGS_FALSE, 0) ;
    // a module done, a member in a crash loop, a member never started
    write_state("mod", STATE_FLAGS_TRUE, 0) ;
    write_status("mod", STATUS_STATE_DONE, 0) ;
    write_state("mod:m2", STATE_FLAGS_TRUE, 0) ;
    write_status("mod:m2", STATUS_STATE_RESTARTING, 0) ;
    write_state("mod:e1", STATE_FLAGS_TRUE, 0) ;
    // the other of a conflict runs, a provider enabled at rest
    write_state("pb", STATE_FLAGS_TRUE, 0) ;
    write_status("pb", STATUS_STATE_UP, 7) ;
    write_state("pv-conn", STATE_FLAGS_TRUE, 0) ;
    // a reactor armed, oneshot and classic, a reactor whose Do stops
    write_state("react", STATE_FLAGS_TRUE, 0) ;
    write_status("react", STATUS_STATE_WAITING, 0) ;
    write_state("rcl", STATE_FLAGS_TRUE, 0) ;
    write_status("rcl", STATUS_STATE_DOWN, 0) ;
    write_state("rstop", STATE_FLAGS_TRUE, 0) ;
    write_status("rstop", STATUS_STATE_DOWN, 0) ;
    // a classic reactor freed: no longer supervised, no status record
    write_state("rfree", STATE_FLAGS_FALSE, 0) ;
    // a module reactor whose members ran on its event
    write_state("rmod", STATE_FLAGS_TRUE, 0) ;
    write_status("rmod", STATUS_STATE_DONE, 0) ;
    // a oneshot done, a classic wanted down with no process
    write_state("one", STATE_FLAGS_TRUE, 0) ;
    write_status("one", STATUS_STATE_DONE, 0) ;
    write_state("loop", STATE_FLAGS_TRUE, 0) ;
    write_status("loop", STATUS_STATE_FINISHING, 0) ;
    // a live copy that says supervised over a parse copy that does not
    write_state("lv", STATE_FLAGS_FALSE, 0) ;
    write_state("lv", STATE_FLAGS_TRUE, 1) ;

    // a supervised service whose status record is cut short
    write_state("badst", STATE_FLAGS_TRUE, 0) ;
    {
        char status[SS_MAX_PATH_LEN], statedir[SS_MAX_PATH_LEN], supervisedir[SS_MAX_PATH_LEN], file[SS_MAX_PATH_LEN] ;
        live_paths("badst", status, statedir, supervisedir) ;
        CHECK(dir_create_parent(supervisedir, 0755), "create %s", supervisedir) ;
        auto_strings(file, supervisedir, "/", SS_STATUS) ;
        CHECK(file_write(file, "abc", 3), "write a short status for badst") ;
    }

    // a state record cut short
    {
        char status[SS_MAX_PATH_LEN], statedir[SS_MAX_PATH_LEN], supervisedir[SS_MAX_PATH_LEN] ;
        live_paths("bad", status, statedir, supervisedir) ;
        CHECK(file_write(status, "abc", 3), "write a short state for bad") ;
    }
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

// the selection of @names (a space separated list) closed by @propagate, without logger
static void draw(selection_service_t *sel, service_graph_t const *g, char const *names, uint8_t propagate)
{
    bitset32_t seeds = bitset32_init(SS_MAX_SERVICE) ;
    selection_service_policy_t policy = SELECTION_SERVICE_POLICY_ZERO ;
    char copy[256] ;

    policy.propagate = propagate ;
    auto_strings(copy, names) ;

    for (char *p = strtok(copy, " ") ; p ; p = strtok(0, " "))
        bitset32_set(&seeds, idx(g, p)) ;

    selection_service_propagate(sel, g, &seeds, &policy) ;
}

// whether @sel holds exactly @names, a space separated list
static int holds(selection_service_t const *sel, char const *names)
{
    char copy[256] ;
    uint32_t n = 0 ;

    auto_strings(copy, names) ;

    for (char *p = strtok(copy, " ") ; p ; p = strtok(0, " ")) {
        if (!bitset32_isvalid(&sel->member, idx(sel->g, p)))
            return 0 ;
        n++ ;
    }

    return n == sel->member.count ;
}

static struct resolve_hash_s *readone(struct resolve_hash_s *h, char const *name, uint32_t addons)
{
    *h = (struct resolve_hash_s)RESOLVE_HASH_ZERO ;
    CHECK(service_read(h, base, name, addons) == 1, "read %s: errno %d", name, errno) ;
    return h ;
}

/**
 *
 * Tests
 *
 * */

static void test_supervised(void)
{
    printf("supervised: the strict flag, the live copy first, no state is no supervisor, a short one an error\n") ;

    struct resolve_hash_s h ;
    struct { char const *name ; int want ; } cases[] = {
        { "a", 1 }, { "b", 1 }, { "c", 0 }, { "boot1", 0 }, { "lv", 1 },
    } ;

    for (size_t i = 0 ; i < sizeof(cases) / sizeof(cases[0]) ; i++) {
        readone(&h, cases[i].name, 0) ;
        int r = svc_issupervised(&h.res) ;
        CHECK(r == cases[i].want, "svc_issupervised(%s) = %d, expected %d", cases[i].name, r, cases[i].want) ;
        service_read_free(&h) ;
    }

    readone(&h, "bad", 0) ;
    errno = 0 ;
    CHECK(svc_issupervised(&h.res) == -1 && errno == EINVAL, "a short state record is an error: errno %d", errno) ;
    service_read_free(&h) ;
}

static void test_running(void)
{
    printf("running: process or crash loop, done, armed reactor, never without supervisor, a status that does not read an error\n") ;

    struct resolve_hash_s h ;
    struct { char const *name ; int want ; } cases[] = {
        { "a", 1 },         // a living process
        { "b", 0 },         // down
        { "c", 0 },         // no supervisor
        { "mod:m2", 1 },    // wanted up by its supervisor between two runs
        { "loop", 0 },      // finishing, no process: not wanted up as far as the record tells
        { "mod", 1 },       // a module done
        { "one", 1 },       // a oneshot done
        { "react", 1 },     // a oneshot reactor armed
        { "rcl", 1 },       // a classic reactor armed, down for its supervisor
        { "rstop", 0 },     // a reactor whose Do stops is no reactor at rest
        { "boot1", 0 },     // no state at all, no status
    } ;

    for (size_t i = 0 ; i < sizeof(cases) / sizeof(cases[0]) ; i++) {
        readone(&h, cases[i].name, SERVICE_READ_EVENT) ;
        int r = svc_isrunning(&h) ;
        CHECK(r == cases[i].want, "svc_isrunning(%s) = %d, expected %d", cases[i].name, r, cases[i].want) ;
        service_read_free(&h) ;
    }

    readone(&h, "bad", SERVICE_READ_EVENT) ;
    CHECK(svc_isrunning(&h) == -1, "a state that does not read is an error") ;
    service_read_free(&h) ;

    // a status record that does not read is no record at all: an error, never down
    readone(&h, "badst", SERVICE_READ_EVENT) ;
    errno = 0 ;
    CHECK(svc_isrunning(&h) == -1 && errno == EINVAL, "a status that does not read is an error: errno %d", errno) ;
    service_read_free(&h) ;

    // the event addon is read in the store: not asked, not judged
    readone(&h, "rcl", 0) ;
    errno = 0 ;
    CHECK(svc_reactor_armed_idle(&h) == -1 && errno == EINVAL, "an event addon not read: errno %d", errno) ;
    CHECK(svc_isrunning(&h) == -1, "running needs the event addon of a reactor") ;
    service_read_free(&h) ;

    readone(&h, "a", 0) ;
    CHECK(!svc_reactor_armed_idle(&h), "a service without event addon is no reactor") ;
    service_read_free(&h) ;

    // the display of a classic reactor at rest
    service_status_t st = STATUS_ZERO ;
    readone(&h, "rcl", SERVICE_READ_EVENT) ;
    CHECK(svc_status_effective(&h, &st) == STATUS_STATE_WAITING, "a classic reactor down is shown waiting") ;
    service_read_free(&h) ;
    readone(&h, "rstop", SERVICE_READ_EVENT) ;
    CHECK(svc_status_effective(&h, &st) == STATUS_STATE_DOWN, "a reactor that stops is shown down") ;
    service_read_free(&h) ;
    readone(&h, "rfree", SERVICE_READ_EVENT) ;
    CHECK(svc_status_effective(&h, &st) == STATUS_STATE_DOWN, "a reactor freed is no longer armed, it is shown down") ;
    service_read_free(&h) ;
    service_status_t done = STATUS_ZERO ;
    done.state = STATUS_STATE_DONE ;
    readone(&h, "rmod", SERVICE_READ_EVENT) ;
    CHECK(svc_status_effective(&h, &done) == STATUS_STATE_WAITING, "a module reactor done is shown waiting") ;
    service_read_free(&h) ;
}

static void test_filters(service_graph_t const *g)
{
    printf("the supervised and running members of a selection, its named set kept\n") ;

    selection_service_t sel, out ;
    service_graph_resolve_t store = SERVICE_GRAPH_RESOLVE_ZERO ;

    draw(&sel, g, "a b c mod u", SELECTION_SERVICE_PROPAGATE_NONE) ;
    CHECK(service_graph_resolve_init(&store, g, base) == 1, "store") ;

    // nothing read yet: a fault of the caller
    errno = 0 ;
    CHECK(selection_svc_supervised(&out, &sel, &store) == -1 && errno == EINVAL, "a member not loaded: errno %d", errno) ;

    CHECK(service_graph_resolve_load(&store, &sel, SERVICE_READ_ALL) == 1, "load") ;

    CHECK(selection_svc_supervised(&out, &sel, &store) == 1, "select supervised: errno %d", errno) ;
    CHECK(holds(&out, "a b mod mod:e1 mod:m2"), "supervised: %u members", out.member.count) ;
    CHECK(!memcmp(&out.named, &sel.named, sizeof(sel.named)), "the named set is kept") ;

    CHECK(selection_svc_filter(&out, &sel, &store, SERVICE_READ_EVENT, svc_isrunning) == 1, "select running: errno %d", errno) ;
    CHECK(holds(&out, "a mod mod:m2"), "running: %u members", out.member.count) ;

    // in place: @out may be @sel
    out = sel ;
    CHECK(selection_svc_filter(&out, &out, &store, SERVICE_READ_EVENT, svc_isrunning) == 1 && holds(&out, "a mod mod:m2"), "in place") ;

    // the store says an unreadable member by 0, the one guard of the steps
    {
        struct resolve_hash_s *h = (struct resolve_hash_s *)1 ;
        errno = 0 ;
        CHECK(service_graph_resolve_get(&store, idx(g, "u"), 0, &h) == 0 && !h && errno == EINVAL, "u unreadable: 0") ;
        CHECK(service_graph_resolve_get(&store, idx(g, "a"), SERVICE_READ_EVENT, &h) == 1 && h, "a read") ;
        errno = 0 ;
        CHECK(service_graph_resolve_get(&store, service_graph_nvertex(g), 0, &h) == -1 && errno == EINVAL, "out of bounds: -1") ;
    }

    service_graph_resolve_free(&store) ;
}

static void test_lot(service_graph_t const *g)
{
    printf("the lot by stage: earlier members left to the boot, a module of earlier services only left out and said\n") ;

    selection_service_t sel, lot ;
    service_graph_resolve_t store = SERVICE_GRAPH_RESOLVE_ZERO ;
    char out[] = "/tmp/66-svc-select-warn.XXXXXX" ;
    _cleanup_strbuf_ strbuf warn = STRBUF_ZERO ;

    CHECK(service_graph_resolve_init(&store, g, base) == 1, "store") ;

    draw(&sel, g, "mod early boot1 a u", SELECTION_SERVICE_PROPAGATE_NONE) ;

    errno = 0 ;
    CHECK(svc_lot(&lot, &sel, &store, SVC_LOT_COMMAND) == -1 && errno == EINVAL, "a member not loaded: errno %d", errno) ;

    CHECK(service_graph_resolve_load(&store, &sel, 0) == 1, "load") ;

    errno = 0 ;
    CHECK(svc_lot(&lot, &sel, &store, SVC_LOT_EARLIER + 1) == -1 && errno == EINVAL, "an unknown stage: errno %d", errno) ;

    // the warning at verbosity 2, on the standard error
    int fd = mkstemp(out) ;
    int saved = dup(2) ;
    CHECK(fd >= 0 && saved >= 0 && dup2(fd, 2) == 2, "redirect the standard error") ;
    VERBOSITY = 2 ;
    int r = svc_lot(&lot, &sel, &store, SVC_LOT_COMMAND) ;
    VERBOSITY = 0 ;
    CHECK(dup2(saved, 2) == 2, "restore the standard error") ;
    close(fd) ;
    close(saved) ;
    CHECK(r == 1, "lot: errno %d", errno) ;
    CHECK(strbuf_read_file(&warn, out), "read the warning") ;
    unlink(out) ;

    CHECK(holds(&lot, "mod mod:m2 boot1 a u"), "command: %u members", lot.member.count) ;
    CHECK(strstr(warn.s, "the module: early holds only earlier services -- nothing to act on outside the boot stage"), "the warning:\n%s", warn.s) ;
    CHECK(!strstr(warn.s, "the module: mod "), "a module that keeps a member is not said:\n%s", warn.s) ;
    CHECK(!memcmp(&lot.named, &sel.named, sizeof(sel.named)), "the named set is kept") ;

    CHECK(svc_lot(&lot, &sel, &store, SVC_LOT_BOOT) == 1 && holds(&lot, "mod mod:e1 mod:m2 early early:e boot1 a u"), "boot: every member") ;

    CHECK(svc_lot(&lot, &sel, &store, SVC_LOT_EARLIER) == 1 && holds(&lot, "mod:e1 early:e boot1"), "earlier: the earlier members only, %u", lot.member.count) ;

    // an earlier member named alone, without its module: the command acts on it
    draw(&sel, g, "mod:e1", SELECTION_SERVICE_PROPAGATE_NONE) ;
    CHECK(service_graph_resolve_load(&store, &sel, 0) == 1, "load") ;
    CHECK(svc_lot(&lot, &sel, &store, SVC_LOT_COMMAND) == 1 && holds(&lot, "mod:e1"), "a member named alone stays") ;

    service_graph_resolve_free(&store) ;
}

static void test_conflict(service_graph_t const *g)
{
    printf("the conflict: running or enabled, through a provided name, an unreadable one conflicts, an unknown name never\n") ;

    uint32_t member, other ;
    selection_service_t sel ;
    service_graph_resolve_t store = SERVICE_GRAPH_RESOLVE_ZERO ;

    CHECK(service_graph_resolve_init(&store, g, base) == 1, "store") ;

    draw(&sel, g, "a pc", SELECTION_SERVICE_PROPAGATE_NONE) ;

    errno = 0 ;
    CHECK(svc_conflict(&sel, &store, base, SVC_CONFLICT_RUNNING, &member, &other) == -1 && errno == EINVAL, "not loaded: errno %d", errno) ;

    CHECK(service_graph_resolve_load(&store, &sel, SERVICE_READ_DEPENDENCIES) == 1, "load") ;

    errno = 0 ;
    CHECK(svc_conflict(&sel, &store, base, SVC_CONFLICT_ENABLED + 1, &member, &other) == -1 && errno == EINVAL, "an unknown mode: errno %d", errno) ;

    CHECK(svc_conflict(&sel, &store, base, SVC_CONFLICT_RUNNING, &member, &other) == 1, "pb runs") ;
    CHECK(member == idx(g, "pc") && other == idx(g, "pb"), "pc against pb: %u %u", member, other) ;
    CHECK(svc_conflict(&sel, &store, base, SVC_CONFLICT_ENABLED, &member, &other) == 1, "pb is not enabled") ;
    CHECK(member == SERVICE_GRAPH_NOINDEX && other == SERVICE_GRAPH_NOINDEX, "nothing named without a conflict") ;

    // the name answers for the service its provider is
    draw(&sel, g, "pn", SELECTION_SERVICE_PROPAGATE_NONE) ;
    CHECK(service_graph_resolve_load(&store, &sel, SERVICE_READ_DEPENDENCIES) == 1, "load") ;
    CHECK(svc_conflict(&sel, &store, base, SVC_CONFLICT_RUNNING, &member, &other) == 1 && member == SERVICE_GRAPH_NOINDEX, "pv-conn does not run") ;
    CHECK(svc_conflict(&sel, &store, base, SVC_CONFLICT_ENABLED, &member, &other) == 1, "pv-conn is enabled") ;
    CHECK(member == idx(g, "pn") && other == idx(g, "pv-conn"), "pn against the provider pv-conn: %u %u", member, other) ;

    // a resolve file that does not read cannot be judged: a conflict, in both modes
    draw(&sel, g, "pu", SELECTION_SERVICE_PROPAGATE_NONE) ;
    CHECK(service_graph_resolve_load(&store, &sel, SERVICE_READ_DEPENDENCIES) == 1, "load") ;
    CHECK(svc_conflict(&sel, &store, base, SVC_CONFLICT_RUNNING, &member, &other) == 1 && other == idx(g, "u"), "u does not read") ;
    CHECK(service_graph_state(g, other) == SERVICE_GRAPH_STATE_UNREADABLE, "the other is named by the graph as unreadable") ;
    CHECK(svc_conflict(&sel, &store, base, SVC_CONFLICT_ENABLED, &member, &other) == 1 && other == idx(g, "u"), "u does not read, enabled") ;

    // a name without resolve file conflicts with nothing
    draw(&sel, g, "pg", SELECTION_SERVICE_PROPAGATE_NONE) ;
    CHECK(service_graph_resolve_load(&store, &sel, SERVICE_READ_DEPENDENCIES) == 1, "load") ;
    CHECK(svc_conflict(&sel, &store, base, SVC_CONFLICT_RUNNING, &member, &other) == 1 && member == SERVICE_GRAPH_NOINDEX && other == SERVICE_GRAPH_NOINDEX, "nosuch is nobody") ;

    service_graph_resolve_free(&store) ;

    /** the other is read through the store: its resolve file gone since the
     * graph was built, the graph is stale, the command loads it again */
    {
        char file[SS_MAX_PATH_LEN + 1], away[SS_MAX_PATH_LEN + 1] ;
        auto_strings(file, base, SS_SYSTEM, SS_SERVICE, SS_SVC, "/pb", SS_RESOLVE, "/pb") ;
        auto_strings(away, base, "/pb.away") ;
        CHECK(rename(file, away) == 0, "move the resolve file of pb away") ;

        CHECK(service_graph_resolve_init(&store, g, base) == 1, "store") ;
        draw(&sel, g, "a pc", SELECTION_SERVICE_PROPAGATE_NONE) ;
        CHECK(service_graph_resolve_load(&store, &sel, SERVICE_READ_DEPENDENCIES) == 1, "load") ;
        CHECK(svc_conflict(&sel, &store, base, SVC_CONFLICT_RUNNING, &member, &other) == 0 && member == SERVICE_GRAPH_NOINDEX, "pb gone: the graph is stale") ;
        service_graph_resolve_free(&store) ;

        CHECK(rename(away, file) == 0, "move the resolve file of pb back") ;
    }
}

static void test_ctx(service_graph_t const *g)
{
    printf("the context of the launcher: walk order, edges inside the lot, the state read once, out of the order first in a stop\n") ;

    selection_service_t lot ;
    service_graph_resolve_t store = SERVICE_GRAPH_RESOLVE_ZERO ;
    svc_request_t req = SVC_REQUEST_ZERO ;
    svc_ctx_t asvc[8] ;

    CHECK(service_graph_resolve_init(&store, g, base) == 1, "store") ;

    draw(&lot, g, "c", SELECTION_SERVICE_PROPAGATE_DEPENDS) ;
    CHECK(holds(&lot, "a b c"), "the chain") ;

    req.action = SVC_ACTION_START ;
    errno = 0 ;
    CHECK(svc_init_ctx(asvc, &lot, &store, &req) == -1 && errno == EINVAL, "not loaded: errno %d", errno) ;

    // the execute and event addons are what the launcher reads
    CHECK(service_graph_resolve_load(&store, &lot, SERVICE_READ_DEPENDENCIES) == 1, "load") ;
    errno = 0 ;
    CHECK(svc_init_ctx(asvc, &lot, &store, &req) == -1 && errno == EINVAL, "addons not read: errno %d", errno) ;

    CHECK(service_graph_resolve_load(&store, &lot, SERVICE_READ_EXECUTE | SERVICE_READ_EVENT) == 1, "load") ;
    CHECK(svc_init_ctx(asvc, &lot, &store, &req) == 1, "start: errno %d", errno) ;

    char const *start[] = { "a", "b", "c" } ;
    uint32_t sdep[] = { 0, 1, 1 }, sreq[] = { 1, 1, 0 } ;
    for (int i = 0 ; i < 3 ; i++) {
        CHECK(asvc[i].index == idx(g, start[i]), "start %d is %s", i, start[i]) ;
        CHECK(asvc[i].ndepends == sdep[i] && asvc[i].nrequiredby == sreq[i], "start %s: %u %u", start[i], asvc[i].ndepends, asvc[i].nrequiredby) ;
        CHECK(asvc[i].h == store.slot[asvc[i].index], "%s is the service of the store", start[i]) ;
    }
    CHECK(asvc[0].state == SVC_FLAGS_UP && asvc[1].state == SVC_FLAGS_DOWN && asvc[2].state == SVC_FLAGS_DOWN, "a runs, b and c do not") ;

    req.action = SVC_ACTION_STOP ;
    CHECK(svc_init_ctx(asvc, &lot, &store, &req) == 1, "stop: errno %d", errno) ;
    char const *stop[] = { "c", "b", "a" } ;
    for (int i = 0 ; i < 3 ; i++) {
        CHECK(asvc[i].index == idx(g, stop[i]), "stop %d is %s", i, stop[i]) ;
        CHECK(asvc[i].ndepends == sreq[2 - i] && asvc[i].nrequiredby == sdep[2 - i], "stop %s: %u %u", stop[i], asvc[i].ndepends, asvc[i].nrequiredby) ;
    }

    // a signal walks like a stop: what depends on a service first
    req.action = SVC_ACTION_SIGNAL ;
    CHECK(svc_init_ctx(asvc, &lot, &store, &req) == 1 && asvc[0].index == idx(g, "c"), "signal: backwards") ;

    // out of the order: first in a stop, waiting for nothing, waited for by nothing
    draw(&lot, g, "z a", SELECTION_SERVICE_PROPAGATE_DEPENDS) ;
    CHECK(holds(&lot, "x y z a"), "the cycle and a") ;
    CHECK(service_graph_resolve_load(&store, &lot, SERVICE_READ_ALL) == 1, "load") ;

    req.action = SVC_ACTION_STOP ;
    CHECK(svc_init_ctx(asvc, &lot, &store, &req) == 1, "stop with members out of the order: errno %d", errno) ;
    char const *unordered[] = { "x", "y", "z" } ;
    for (int i = 0 ; i < 3 ; i++) {
        CHECK(asvc[i].index == idx(g, unordered[i]), "position %d is %s", i, unordered[i]) ;
        CHECK(!asvc[i].ndepends && !asvc[i].nrequiredby, "%s is out of the order", unordered[i]) ;
    }
    CHECK(asvc[3].index == idx(g, "a") && !asvc[3].ndepends && !asvc[3].nrequiredby, "a last: z out of the order waits for nothing, a waits for no z") ;

    // a start never holds one: the command refused it
    req.action = SVC_ACTION_START ;
    errno = 0 ;
    CHECK(svc_init_ctx(asvc, &lot, &store, &req) == -1 && errno == EINVAL, "a start with a member out of the order: errno %d", errno) ;

    service_graph_resolve_free(&store) ;
}

static void test_send(service_graph_t const *g)
{
    printf("svc_send: a request not filled is refused, an empty lot reached its target\n") ;

    selection_service_t lot = SELECTION_SERVICE_ZERO ;
    service_graph_resolve_t store = SERVICE_GRAPH_RESOLVE_ZERO ;
    svc_request_t req = SVC_REQUEST_ZERO ;

    CHECK(service_graph_resolve_init(&store, g, base) == 1, "store") ;
    lot.g = g ;
    lot.member = bitset32_init(SS_MAX_SERVICE) ;

    errno = 0 ;
    CHECK(svc_send(&lot, &store, &req, 0) == -1 && errno == EINVAL, "no target: errno %d", errno) ;

    req.target = SVC_TARGET_UP ;
    errno = 0 ;
    CHECK(svc_send(&lot, &store, &req, 0) == -1 && errno == EINVAL, "no signal: errno %d", errno) ;

    req.signal = "-u" ;
    CHECK(svc_send(&lot, &store, &req, 0) == 1, "nothing to drive") ;

    service_graph_resolve_free(&store) ;
}

int main(void)
{
    printf("Starting svc select test...\n") ;

    service_graph_t g = SERVICE_GRAPH_ZERO ;

    PROG = "svc_select" ;
    VERBOSITY = 0 ;
    CHECK(mkdtemp(base), "make the base") ;
    strcat(base, "/") ;

    CHECK(service_graph_write_begin(getuid(), base, base) == 1, "hold the transaction") ;
    write_system() ;
    CHECK(service_graph_write_end(0) >= 0, "end the transaction") ;

    CHECK(service_graph_load(&g, getuid(), base, base) == 1, "load the graph: errno %d", errno) ;
    CHECK(service_graph_state(&g, idx(&g, "x")) == SERVICE_GRAPH_STATE_UNORDERED, "x is out of the order") ;
    CHECK(service_graph_state(&g, idx(&g, "u")) == SERVICE_GRAPH_STATE_UNREADABLE, "u does not read") ;

    test_supervised() ;
    test_running() ;
    test_filters(&g) ;
    test_lot(&g) ;
    test_conflict(&g) ;
    test_ctx(&g) ;
    test_send(&g) ;

    service_graph_free(&g) ;
    CHECK(dir_destroy(base), "remove the base") ;

    printf("All tests passed!\n") ;

    return 0 ;
}
