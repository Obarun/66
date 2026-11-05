/*
 * parse_names.c
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

/** The parse of a command, by effect: parse_names() in memory, parse_check()
 * against the system, parse_dropped() on the graph of before, parse_write()
 * under the write transaction. Everything runs in a child that owns a user
 * namespace, mapped as root, and a chroot holding the frontend directories,
 * the configuration, the log directories and the system at /base/: nothing of
 * the host is read or written. Each case checks what reaches the disk, and that
 * a refusal leaves the disk as it was. */

#include <assert.h>
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <oblibs/types.h>
#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/directory.h>
#include <oblibs/files.h>
#include <oblibs/strbuf.h>
#include <oblibs/sbl.h>
#include <oblibs/bits.h>
#include <oblibs/hash.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/graph.h>
#include <66/parse.h>
#include <66/tree.h>
#include <66/ssexec.h>
#include <66/symlink.h>

#define CHECK(cond, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__) ; \
        fprintf(stderr, __VA_ARGS__) ; \
        fprintf(stderr, "\n") ; \
        abort() ; \
    } \
} while (0)

#define BASE "/base/"

static ssexec_t info = SSEXEC_ZERO ;

/**
 *
 * Fixtures, inside the chroot
 *
 * */

static void frontend(char const *name, char const *content)
{
    char path[SS_MAX_PATH_LEN] ;
    auto_strings(path, SS_SERVICE_ADMDIR, name) ;
    CHECK(file_write(path, content, strlen(content)), "write the frontend of %s", name) ;
}

static void frontend_drop(char const *name)
{
    char path[SS_MAX_PATH_LEN] ;
    auto_strings(path, SS_SERVICE_ADMDIR, name) ;
    CHECK(unlink(path) == 0, "remove the frontend of %s", name) ;
}

// a oneshot that runs /bin/true, @extra lines added to [Main]
static void oneshot(char const *name, char const *extra)
{
    char content[4096] ;
    auto_strings(content, "[Main]\nType = oneshot\n", extra, "[Start]\nExecute = ( /bin/true )\n") ;
    frontend(name, content) ;
}

static void make_tree(char const *tree)
{
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t w = { .type = DATA_TREE, .obj = &tres } ;

    resolve_init(&w) ;
    tres.name = resolve_add_string(&w, tree) ;
    tres.allow = resolve_add_string(&w, "0") ;
    tres.nallow = 1 ;

    CHECK(service_graph_write_begin(0, BASE, SS_LIVE) == 1, "hold the transaction") ;
    CHECK(resolve_write(&w, BASE, tree), "write the tree %s: errno %d", tree, errno) ;
    CHECK(service_graph_write_end(0) >= 0, "release the transaction") ;

    strbuf_free(&tres.sa) ;
}

static void setup_info(void)
{
    info.owner = 0 ;
    info.ownerstr[0] = '0' ;
    info.ownerstr[1] = 0 ;
    info.ownerlen = 1 ;
    info.opt_tree = 1 ;
    CHECK(auto_strbuf(&info.base, BASE), "base") ;
    CHECK(auto_strbuf(&info.live, SS_LIVE), "live") ;
    CHECK(auto_strbuf(&info.scandir, SS_LIVE "scandir/0"), "scandir") ;
    CHECK(auto_strbuf(&info.treename, SS_DEFAULT_TREENAME), "tree") ;
}

/**
 *
 * The command, step by step
 *
 * */

typedef struct run_s run_t ;
struct run_s
{
    hash_t hres ;
    service_graph_t g ;
    strbuf msg ;
} ;

static void run_init(run_t *run)
{
    run->g = (service_graph_t)SERVICE_GRAPH_ZERO ;
    run->msg = (strbuf)STRBUF_ZERO ;
    run->hres = (hash_t)HASH_ZERO ;
    CHECK(hash_init(&run->hres, 0, offsetof(struct resolve_hash_s, node)), "hash") ;
    CHECK(service_graph_load(&run->g, 0, BASE, SS_LIVE) == 1, "load the graph: errno %d", errno) ;
}

static void run_free(run_t *run)
{
    resolve_hash_free(&run->hres) ;
    service_graph_free(&run->g) ;
    strbuf_free(&run->msg) ;
}

static int names(run_t *run, uint32_t flags, char const *const *argv, int argc)
{
    run->msg.len = 0 ;
    return parse_names(&run->hres, &run->g, argv, argc, &info, flags, &run->msg) ;
}

// the whole parse of a command: in memory, checked, written
static int parse(run_t *run, uint32_t flags, char const *const *argv, int argc)
{
    int r = names(run, flags, argv, argc) ;
    if (r <= 0)
        return r ;

    if (!hash_count(&run->hres))
        return 1 ;

    r = parse_check(&run->hres, &info, &run->msg) ;
    if (r <= 0)
        return r ;

    CHECK(service_graph_write_begin(0, BASE, SS_LIVE) == 1, "hold the transaction") ;

    r = parse_check(&run->hres, &info, &run->msg) ;
    if (r > 0)
        r = parse_write(&run->hres, &info) ;

    CHECK(service_graph_write_end(0) >= 0, "release the transaction") ;

    return r ;
}

#define ARGV(...) (char const *const []){ __VA_ARGS__ }, (int)(sizeof((char const *const []){ __VA_ARGS__ }) / sizeof(char const *))

/**
 *
 * What the disk holds
 *
 * */

// 1 when @name has a resolve file that reads, its fields in @h when given
static int parsed(char const *name, struct resolve_hash_s *out)
{
    struct resolve_hash_s h = RESOLVE_HASH_ZERO ;
    int r = service_read(&h, BASE, name, SERVICE_READ_ALL) ;
    CHECK(r >= 0, "read %s: errno %d", name, errno) ;

    if (out && r)
        *out = h ;
    else
        service_read_free(&h) ;

    return r ;
}

static int enabled(char const *name)
{
    struct resolve_hash_s h = RESOLVE_HASH_ZERO ;
    CHECK(parsed(name, &h), "%s is parsed", name) ;
    int r = h.res.enabled ? 1 : 0 ;
    service_read_free(&h) ;
    return r ;
}

static void set_enabled(char const *name, int on)
{
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t w = { .type = DATA_SERVICE, .obj = &res } ;

    CHECK(resolve_read(&w, BASE, name) == 1, "read %s", name) ;
    res.enabled = on ? 1 : 0 ;

    CHECK(service_graph_write_begin(0, BASE, SS_LIVE) == 1, "hold the transaction") ;
    CHECK(resolve_write(&w, BASE, name), "write %s", name) ;
    CHECK(service_graph_write_end(0) >= 0, "release the transaction") ;

    strbuf_free(&res.sa) ;
}

// the Depends of @name on disk, "" when none
static void depends_are(char const *name, char const *want)
{
    struct resolve_hash_s h = RESOLVE_HASH_ZERO ;
    CHECK(parsed(name, &h), "%s is parsed", name) ;
    char const *got = h.dependencies.ndepends ? h.dependencies.sa.s + h.dependencies.depends : "" ;
    CHECK(!strcmp(got, want), "Depends of %s: '%s', expected '%s'", name, got, want) ;
    service_read_free(&h) ;
}

// the provider the link @name leads to, "" when none
static void provide_is(char const *name, char const *want)
{
    char got[SS_MAX_SERVICE_NAME + 1] ;
    int r = service_resolve_provide(got, name, 0, BASE) ;
    CHECK(r >= 0, "resolve %s", name) ;
    if (!r)
        got[0] = 0 ;
    CHECK(!strcmp(got, want), "%s leads to '%s', expected '%s'", name, got, want) ;
}

static int tree_holds(char const *tree, char const *name)
{
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t w = { .type = DATA_TREE, .obj = &tres } ;
    _cleanup_strbuf_ strbuf stk = STRBUF_ZERO ;

    CHECK(resolve_read(&w, BASE, tree) == 1, "read the tree %s", tree) ;
    int r = tres.ncontents && sbl_clean_string(&stk, tres.sa.s + tres.contents) && sbl_search(&stk, name) >= 0 ;
    strbuf_free(&tres.sa) ;
    return r ;
}

// @hres holds exactly the @n names of @want
static void holds(run_t *run, char const *const *want, int n)
{
    CHECK((int)hash_count(&run->hres) == n, "%zu services in memory, expected %d", hash_count(&run->hres), n) ;
    for (int i = 0 ; i < n ; i++)
        CHECK(resolve_hash_search(&run->hres, want[i]), "%s in memory", want[i]) ;
}

static void refused(int r, run_t *run, char const *want)
{
    CHECK(r == 0, "refused: got %d, errno %d", r, errno) ;
    CHECK(run->msg.len && !strcmp(run->msg.s, want), "message:\n  got  '%s'\n  want '%s'", run->msg.len ? run->msg.s : "", want) ;
}

/**
 *
 * Cases
 *
 * */

static void test_operands(void)
{
    printf("every operand is checked before anything is parsed, with the words of the contract\n") ;

    run_t run ;

    oneshot("op-a", "") ;

    run_init(&run) ;
    refused(names(&run, PARSE_FRONTEND, ARGV("op-a", "nobody")), &run,
        "service: nobody is unknown: no frontend and no service answers to the name -- is its provider enabled?") ;
    CHECK(!hash_count(&run.hres), "nothing parsed after a refused operand") ;
    run_free(&run) ;

    run_init(&run) ;
    refused(names(&run, PARSE_FRONTEND, ARGV("op-a", "foo-log")), &run,
        "service name: foo-log: ends with the suffix -log reserved to the loggers") ;
    CHECK(!hash_count(&run.hres), "nothing parsed after a reserved name") ;
    run_free(&run) ;

    run_init(&run) ;
    refused(names(&run, PARSE_FRONTEND, ARGV("mod:x")), &run,
        "service: mod:x is part of a module and cannot be parsed alone -- please parse the entire module instead using '66 parse mod'") ;
    run_free(&run) ;

    /** a frontend naming a reserved name is refused, the reason at verbosity 2:
     * before any lookup, the frontend of foo-log found or not */
    oneshot("foo-log", "") ;
    oneshot("op-b", "Depends = ( foo-log )\n") ;
    run_init(&run) ;
    refused(names(&run, PARSE_FRONTEND, ARGV("op-b")), &run, "unable to parse dependencies of service: op-b") ;
    run_free(&run) ;

    // a Depends nobody answers to
    oneshot("op-c", "Depends = ( ghost )\n") ;
    run_init(&run) ;
    refused(names(&run, PARSE_FRONTEND, ARGV("op-c")), &run, "unable to parse dependencies of service: op-c") ;
    run_free(&run) ;

    // an invalid frontend
    frontend("op-d", "[Main]\nType = bogus\n[Start]\nExecute = ( /bin/true )\n") ;
    run_init(&run) ;
    refused(names(&run, PARSE_FRONTEND, ARGV("op-d")), &run, "unable to parse service: op-d") ;
    run_free(&run) ;

    CHECK(!parsed("op-a", 0) && !parsed("op-b", 0), "a refusal wrote nothing") ;
}

static void test_closure(void)
{
    printf("the propagation: Depends parsed, never RequiredBy; -P parses the operand alone\n") ;

    run_t run ;

    oneshot("cl-a", "Depends = ( cl-b )\n") ;
    oneshot("cl-b", "Depends = ( cl-c )\n") ;
    oneshot("cl-c", "") ;
    oneshot("cl-r", "RequiredBy = ( cl-a )\n") ;

    run_init(&run) ;
    CHECK(names(&run, PARSE_FRONTEND|PARSE_NODEPS, ARGV("cl-a")) == 1, "parse -P: %s", run.msg.s) ;
    holds(&run, (char const *[]){ "cl-a" }, 1) ;
    run_free(&run) ;

    run_init(&run) ;
    CHECK(names(&run, PARSE_FRONTEND, ARGV("cl-a")) == 1, "parse: %s", run.msg.s) ;
    holds(&run, (char const *[]){ "cl-a", "cl-b", "cl-c" }, 3) ;
    run_free(&run) ;

    run_init(&run) ;
    CHECK(parse(&run, PARSE_FRONTEND, ARGV("cl-a")) == 1, "write: %s", run.msg.s) ;
    run_free(&run) ;

    CHECK(parsed("cl-a", 0) && parsed("cl-b", 0) && parsed("cl-c", 0), "the propagation is written") ;
    CHECK(!parsed("cl-r", 0), "a RequiredBy is never followed by a parse") ;
    CHECK(tree_holds(SS_DEFAULT_TREENAME, "cl-a") && tree_holds(SS_DEFAULT_TREENAME, "cl-c"), "the tree holds them") ;
    depends_are("cl-a", "cl-b") ;

    {
        char run_script[SS_MAX_PATH_LEN] ;
        auto_strings(run_script, BASE, SS_SYSTEM, SS_SERVICE, SS_SVC, "/cl-a/run") ;
        CHECK(!access(run_script, X_OK), "the service directory holds its run script") ;
    }

    // a RequiredBy naming a service never parsed: declaration kept, the service not parsed
    oneshot("cl-q", "RequiredBy = ( cl-x )\n") ;
    oneshot("cl-x", "") ;
    run_init(&run) ;
    CHECK(parse(&run, PARSE_FRONTEND, ARGV("cl-q")) == 1, "parse: %s", run.msg.s) ;
    holds(&run, (char const *[]){ "cl-q" }, 1) ;
    run_free(&run) ;
    CHECK(parsed("cl-q", 0) && !parsed("cl-x", 0), "the RequiredBy is not followed") ;
    {
        struct resolve_hash_s h = RESOLVE_HASH_ZERO ;
        CHECK(parsed("cl-q", &h) && h.dependencies.nrequiredby == 1 && !strcmp(h.dependencies.sa.s + h.dependencies.requiredby, "cl-x"), "the declaration is kept") ;
        service_read_free(&h) ;
    }

    // already parsed, without -f: nothing to write
    run_init(&run) ;
    CHECK(names(&run, PARSE_FRONTEND, ARGV("cl-a")) == 1 && !hash_count(&run.hres), "already parsed is ignored") ;
    run_free(&run) ;
}

static void test_missing(void)
{
    printf("PARSE_MISSING: an absent dependency met through the graph is parsed\n") ;

    run_t run ;

    oneshot("ms-a", "Depends = ( ms-b )\n") ;
    oneshot("ms-b", "Depends = ( ms-c )\n") ;
    oneshot("ms-c", "") ;

    // ms-a alone on disk: ms-b is absent
    run_init(&run) ;
    CHECK(parse(&run, PARSE_FRONTEND|PARSE_NODEPS, ARGV("ms-a")) == 1, "parse -P: %s", run.msg.s) ;
    run_free(&run) ;
    CHECK(parsed("ms-a", 0) && !parsed("ms-b", 0), "ms-b absent") ;

    run_init(&run) ;
    CHECK(names(&run, PARSE_MISSING, ARGV("ms-a")) == 1, "start on the fly: %s", run.msg.s) ;
    holds(&run, (char const *[]){ "ms-b", "ms-c" }, 2) ;
    run_free(&run) ;

    run_init(&run) ;
    CHECK(names(&run, PARSE_MISSING|PARSE_NODEPS, ARGV("ms-a")) == 1, "start -P: %s", run.msg.s) ;
    holds(&run, 0, 0) ;
    run_free(&run) ;

    // an absent name without frontend, met through the graph
    oneshot("ms-d", "Depends = ( ms-gone )\n") ;
    oneshot("ms-e", "Depends = ( ms-gone ms-f )\n") ;
    oneshot("ms-f", "") ;
    oneshot("ms-gone", "") ;
    run_init(&run) ;
    CHECK(parse(&run, PARSE_FRONTEND|PARSE_NODEPS, ARGV("ms-d", "ms-e")) == 1, "parse -P: %s", run.msg.s) ;
    run_free(&run) ;
    frontend_drop("ms-gone") ;

    run_init(&run) ;
    refused(names(&run, PARSE_MISSING, ARGV("ms-d")), &run,
        "service: ms-gone has neither a resolve file nor a frontend -- remove it with 66 remove ms-gone") ;
    run_free(&run) ;

    // a tree leaves it absent, the rest of what it prepares is parsed
    run_init(&run) ;
    CHECK(names(&run, PARSE_MISSING|PARSE_LEAVEGONE, ARGV("ms-d")) == 1 && !run.msg.len, "tree, gone left absent: %s", run.msg.s) ;
    holds(&run, 0, 0) ;
    run_free(&run) ;

    run_init(&run) ;
    CHECK(names(&run, PARSE_MISSING|PARSE_LEAVEGONE, ARGV("ms-e")) == 1 && !run.msg.len, "tree, the others parsed: %s", run.msg.s) ;
    holds(&run, (char const *[]){ "ms-f" }, 1) ;
    run_free(&run) ;
}

static void test_force(void)
{
    printf("-f: the service alone written again, a new Depends parsed, its activation kept\n") ;

    run_t run ;

    oneshot("fo-a", "Depends = ( fo-b )\n") ;
    oneshot("fo-b", "") ;
    oneshot("fo-n", "") ;

    run_init(&run) ;
    CHECK(parse(&run, PARSE_FRONTEND, ARGV("fo-a")) == 1, "parse: %s", run.msg.s) ;
    run_free(&run) ;

    set_enabled("fo-a", 1) ;

    oneshot("fo-a", "Depends = ( fo-b fo-n )\n") ;

    run_init(&run) ;
    CHECK(names(&run, PARSE_FRONTEND|PARSE_FORCE, ARGV("fo-a")) == 1, "parse -f: %s", run.msg.s) ;
    holds(&run, (char const *[]){ "fo-a", "fo-n" }, 2) ;
    run_free(&run) ;

    run_init(&run) ;
    CHECK(parse(&run, PARSE_FRONTEND|PARSE_FORCE, ARGV("fo-a")) == 1, "parse -f: %s", run.msg.s) ;
    run_free(&run) ;

    depends_are("fo-a", "fo-b fo-n") ;
    CHECK(parsed("fo-n", 0), "the new Depends is written") ;
    CHECK(enabled("fo-a"), "the activation is kept") ;
    CHECK(!enabled("fo-n"), "the new dependency is not enabled") ;

    // its frontend gone, a parsed service is invalid: -f refuses it
    oneshot("fo-g", "") ;
    run_init(&run) ;
    CHECK(parse(&run, PARSE_FRONTEND, ARGV("fo-g")) == 1, "parse: %s", run.msg.s) ;
    run_free(&run) ;
    frontend_drop("fo-g") ;
    run_init(&run) ;
    refused(names(&run, PARSE_FRONTEND|PARSE_FORCE, ARGV("fo-g")), &run,
        "service: fo-g has no frontend anywhere any more -- remove it with 66 remove fo-g") ;
    run_free(&run) ;
}

static void test_cycle(void)
{
    printf("a cycle a service of the parse closes is refused before anything is written\n") ;

    run_t run ;

    oneshot("cy-a", "Depends = ( cy-b )\n") ;
    oneshot("cy-b", "Depends = ( cy-a )\n") ;

    run_init(&run) ;
    refused(parse(&run, PARSE_FRONTEND, ARGV("cy-a")), &run, "cyclic dependencies detected -- cy-a -> cy-b -> cy-a") ;
    run_free(&run) ;
    CHECK(!parsed("cy-a", 0) && !parsed("cy-b", 0), "nothing written") ;

    // -f closes a cycle with a parsed service
    oneshot("cy-c", "Depends = ( cy-d )\n") ;
    oneshot("cy-d", "") ;
    run_init(&run) ;
    CHECK(parse(&run, PARSE_FRONTEND, ARGV("cy-c")) == 1, "parse: %s", run.msg.s) ;
    run_free(&run) ;

    oneshot("cy-d", "Depends = ( cy-c )\n") ;
    run_init(&run) ;
    int r = parse(&run, PARSE_FRONTEND|PARSE_FORCE, ARGV("cy-d")) ;
    CHECK(!r && strstr(run.msg.s, "cyclic"), "-f closing a cycle: %d %s", r, run.msg.s) ;
    run_free(&run) ;
    depends_are("cy-d", "") ;
}

static void test_provide(void)
{
    printf("Provide: held, carried by a parsed frontend, parsed along, taken back, released\n") ;

    run_t run ;

    oneshot("pv-p", "Provide = ( pv-alias )\n") ;

    run_init(&run) ;
    CHECK(parse(&run, PARSE_FRONTEND, ARGV("pv-p")) == 1, "parse: %s", run.msg.s) ;
    run_free(&run) ;
    provide_is("pv-alias", "pv-p") ;

    // a name a provider answers for designates it, outside PARSE_FRONTEND
    run_init(&run) ;
    CHECK(names(&run, PARSE_MISSING, ARGV("pv-alias")) == 1 && !hash_count(&run.hres), "pv-alias designates pv-p") ;
    run_free(&run) ;

    // a name no frontend carries is unknown to parse
    run_init(&run) ;
    refused(names(&run, PARSE_FRONTEND, ARGV("pv-alias")), &run,
        "service: pv-alias is unknown: no frontend and no service answers to the name -- is its provider enabled?") ;
    run_free(&run) ;

    // a frontend and its provider parsed together
    oneshot("pv-alias", "") ;
    oneshot("pv-q", "Provide = ( pv-alias2 )\n") ;
    oneshot("pv-alias2", "") ;
    run_init(&run) ;
    refused(parse(&run, PARSE_FRONTEND, ARGV("pv-q", "pv-alias2")), &run,
        "service: pv-q provides the name: pv-alias2, parsed along with it -- drop one of the two frontends") ;
    run_free(&run) ;
    run_init(&run) ;
    refused(parse(&run, PARSE_FRONTEND, ARGV("pv-alias2", "pv-q")), &run,
        "service: pv-q provides the name: pv-alias2, parsed along with it -- drop one of the two frontends") ;
    run_free(&run) ;

    // an enabled provider holds the name of the frontend
    set_enabled("pv-p", 1) ;
    run_init(&run) ;
    refused(names(&run, PARSE_FRONTEND, ARGV("pv-alias")), &run,
        "service: pv-alias cannot be parsed, its name is provided by: pv-p -- '66 disable -P pv-p' first") ;
    run_free(&run) ;

    // a provider of a name an enabled one holds
    oneshot("pv-r", "Provide = ( pv-alias )\n") ;
    run_init(&run) ;
    refused(parse(&run, PARSE_FRONTEND, ARGV("pv-r")), &run,
        "service: pv-r provides the name: pv-alias, already provided by the enabled: pv-p -- '66 disable -P pv-p' first") ;
    run_free(&run) ;

    // a provider of the name of a parsed service, refused while the service is enabled
    oneshot("pv-s", "Provide = ( cl-c )\n") ;
    set_enabled("cl-c", 1) ;
    run_init(&run) ;
    refused(parse(&run, PARSE_FRONTEND, ARGV("pv-s")), &run,
        "service: pv-s provides the name: cl-c, which is the name of the enabled service: cl-c -- '66 disable -P cl-c' first") ;
    run_free(&run) ;
    provide_is("cl-c", "") ;

    // disabled, the service hands its name over and stays parsed
    set_enabled("cl-c", 0) ;
    run_init(&run) ;
    CHECK(parse(&run, PARSE_FRONTEND, ARGV("pv-s")) == 1, "parse pv-s: %s", run.msg.s) ;
    run_free(&run) ;
    provide_is("cl-c", "pv-s") ;
    CHECK(parsed("cl-c", 0), "cl-c stays parsed") ;

    // a disabled provider gives the name back to the frontend that bears it
    oneshot("pv-t", "Provide = ( pv-own )\n") ;
    oneshot("pv-own", "") ;
    run_init(&run) ;
    CHECK(parse(&run, PARSE_FRONTEND, ARGV("pv-t")) == 1, "parse: %s", run.msg.s) ;
    run_free(&run) ;
    provide_is("pv-own", "pv-t") ;
    run_init(&run) ;
    CHECK(parse(&run, PARSE_FRONTEND, ARGV("pv-own")) == 1, "parse pv-own: %s", run.msg.s) ;
    run_free(&run) ;
    provide_is("pv-own", "") ;
    CHECK(parsed("pv-own", 0), "pv-own takes its name back") ;

    // -f drops a name a service depends on: refused, nothing written
    oneshot("pv-u", "Provide = ( pv-used )\n") ;
    oneshot("pv-c", "Depends = ( pv-used )\n") ;
    run_init(&run) ;
    CHECK(parse(&run, PARSE_FRONTEND, ARGV("pv-u", "pv-c")) == 1, "parse: %s", run.msg.s) ;
    run_free(&run) ;
    provide_is("pv-used", "pv-u") ;
    oneshot("pv-u", "") ;
    run_init(&run) ;
    refused(parse(&run, PARSE_FRONTEND|PARSE_FORCE, ARGV("pv-u")), &run,
        "service: pv-u no longer provides the name: pv-used, which service: pv-c depends on -- remove it from the Depends of: pv-c first") ;
    run_free(&run) ;
    provide_is("pv-used", "pv-u") ;

    // -f drops a name nobody depends on: released
    oneshot("pv-v", "Provide = ( pv-free )\n") ;
    run_init(&run) ;
    CHECK(parse(&run, PARSE_FRONTEND, ARGV("pv-v")) == 1, "parse: %s", run.msg.s) ;
    run_free(&run) ;
    provide_is("pv-free", "pv-v") ;
    oneshot("pv-v", "") ;
    run_init(&run) ;
    CHECK(parse(&run, PARSE_FRONTEND|PARSE_FORCE, ARGV("pv-v")) == 1, "parse -f: %s", run.msg.s) ;
    run_free(&run) ;
    provide_is("pv-free", "") ;
}

static void test_dropped(void)
{
    printf("parse_dropped: the logger a frontend drops, on the graph of before\n") ;

    run_t run ;
    bitset32_t dropped ;

    frontend("dr-a", "[Main]\nType = classic\n[Start]\nExecute = ( /bin/true )\n") ;

    run_init(&run) ;
    CHECK(parse(&run, PARSE_FRONTEND, ARGV("dr-a")) == 1, "parse: %s", run.msg.s) ;
    run_free(&run) ;
    CHECK(parsed("dr-a-log", 0), "the logger is written") ;
    depends_are("dr-a", "dr-a-log") ;

    // parsed again with its logger: nothing dropped
    run_init(&run) ;
    CHECK(names(&run, PARSE_FRONTEND|PARSE_FORCE, ARGV("dr-a")) == 1, "parse -f: %s", run.msg.s) ;
    CHECK(parse_dropped(&dropped, &run.hres, &run.g) == 1, "dropped") ;
    for (uint32_t i = 0 ; i < service_graph_nvertex(&run.g) ; i++)
        CHECK(!bitset32_isvalid(&dropped, i), "%s dropped", service_graph_name(&run.g, i)) ;
    run_free(&run) ;

    frontend("dr-a", "[Main]\nType = classic\nOptions = ( !log )\n[Start]\nExecute = ( /bin/true )\n") ;

    run_init(&run) ;
    CHECK(names(&run, PARSE_FRONTEND|PARSE_FORCE, ARGV("dr-a")) == 1, "parse -f: %s", run.msg.s) ;
    CHECK(parse_dropped(&dropped, &run.hres, &run.g) == 1, "dropped") ;
    for (uint32_t i = 0 ; i < service_graph_nvertex(&run.g) ; i++) {
        int want = !strcmp(service_graph_name(&run.g, i), "dr-a-log") ;
        // bitset32_isvalid() gives the mask of the bit, not 1
        CHECK((bitset32_isvalid(&dropped, i) != 0) == want, "%s dropped: %d", service_graph_name(&run.g, i), !want) ;
    }
    run_free(&run) ;
}

static void test_environ(void)
{
    printf("the configuration is written outside <base> only by the write, through <conf>/<service>/.tmp\n") ;

    run_t run ;
    char path[SS_MAX_PATH_LEN] ;
    char lnk[SS_MAX_PATH_LEN] ;
    _cleanup_strbuf_ strbuf content = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf list = STRBUF_ZERO ;
    char const *exclude[1] = { 0 } ;

    // refused by a cycle: nothing written, the configuration neither
    frontend("en-a", "[Main]\nType = oneshot\nDepends = ( en-b )\n[Start]\nExecute = ( /bin/true )\n[Environment]\nFOO=bar\n") ;
    oneshot("en-b", "Depends = ( en-a )\n") ;
    run_init(&run) ;
    int r = parse(&run, PARSE_FRONTEND, ARGV("en-a")) ;
    CHECK(!r && strstr(run.msg.s, "cyclic"), "refused: %d %s", r, run.msg.s) ;
    run_free(&run) ;
    auto_strings(path, SS_SERVICE_ADMCONFDIR, "en-a") ;
    CHECK(access(path, F_OK) < 0 && errno == ENOENT, "no configuration written on a refusal") ;

    // a configuration that cannot be written, a file in place of its version directory: the service is not written either
    frontend("en-c", "[Main]\nType = oneshot\n[Start]\nExecute = ( /bin/true )\n[Environment]\nFOO=bar\n") ;
    auto_strings(path, SS_SERVICE_ADMCONFDIR, "en-c") ;
    CHECK(dir_create_parent(path, 0755), "create %s", path) ;
    auto_strings(path, SS_SERVICE_ADMCONFDIR, "en-c/" SS_VERSION) ;
    CHECK(file_write(path, "", 0), "a file at %s", path) ;
    run_init(&run) ;
    r = parse(&run, PARSE_FRONTEND, ARGV("en-c")) ;
    CHECK(r < 0, "the write fails: %d", r) ;
    run_free(&run) ;
    CHECK(!parsed("en-c", 0), "en-c is not written without its configuration") ;
    CHECK(unlink(path) == 0, "remove %s", path) ;
    frontend_drop("en-c") ;

    oneshot("en-b", "") ;
    run_init(&run) ;
    CHECK(parse(&run, PARSE_FRONTEND, ARGV("en-a")) == 1, "parse: %s", run.msg.s) ;
    run_free(&run) ;

    auto_strings(path, SS_SERVICE_ADMCONFDIR, "en-a/" SS_VERSION "/.en-a") ;
    CHECK(strbuf_read_file(&content, path), "read %s", path) ;
    CHECK(strstr(content.s, "FOO=bar"), "the configuration holds the environment:\n%s", content.s) ;

    auto_strings(path, SS_SERVICE_ADMCONFDIR, "en-a/version") ;
    ssize_t len = readlink(path, lnk, sizeof(lnk) - 1) ;
    CHECK(len > 0, "the version link") ;
    lnk[len] = 0 ;
    CHECK(!strcmp(lnk, SS_SERVICE_ADMCONFDIR "en-a/" SS_VERSION), "the link leads to the version: %s", lnk) ;

    // no temporary is left: the configuration is written in its version directory, no .tmp is made
    auto_strings(path, SS_SERVICE_ADMCONFDIR, "en-a/.tmp") ;
    CHECK(access(path, F_OK) < 0 && errno == ENOENT, "no .tmp is left") ;
    auto_strings(path, SS_SERVICE_ADMCONFDIR, "en-a/" SS_VERSION) ;
    CHECK(sbl_dir_get(&list, path, exclude, FLAGS_REG|FLAGS_LNK|FLAGS_DIR) && sbl_count(&list) == 1, "the version directory holds the file of the service only") ;
}

static void module_file(char const *path, char const *content)
{
    char file[SS_MAX_PATH_LEN] ;
    char dir[SS_MAX_PATH_LEN] ;

    auto_strings(file, SS_SERVICE_ADMDIR, "mod/", path) ;
    CHECK(ob_dirname(dir, file) && dir_create_parent(dir, 0755), "create the directory of %s", file) ;
    CHECK(file_write(file, content, strlen(content)), "write %s", file) ;
}

static void test_module(void)
{
    printf("a module comes with its members, the members it no longer holds are dropped\n") ;

    run_t run ;
    bitset32_t dropped ;
    char path[SS_MAX_PATH_LEN] ;

    module_file("mod", "[Main]\nType = module\n") ;
    module_file("frontend/m1", "[Main]\nType = oneshot\n[Start]\nExecute = ( /bin/true )\n") ;
    module_file("frontend/m2", "[Main]\nType = oneshot\n[Start]\nExecute = ( /bin/true )\n") ;
    module_file("activated/m1", "") ;
    module_file("activated/m2", "") ;

    run_init(&run) ;
    CHECK(names(&run, PARSE_FRONTEND|PARSE_NODEPS, ARGV("mod")) == 1, "parse -P mod: %s", run.msg.s) ;
    holds(&run, (char const *[]){ "mod", "mod:m1", "mod:m2" }, 3) ;
    run_free(&run) ;

    run_init(&run) ;
    CHECK(parse(&run, PARSE_FRONTEND, ARGV("mod")) == 1, "parse mod: %s", run.msg.s) ;
    run_free(&run) ;

    {
        struct resolve_hash_s h = RESOLVE_HASH_ZERO ;
        CHECK(parsed("mod", &h), "mod is parsed") ;
        char const *c = h.dependencies.sa.s + h.dependencies.contents ;
        CHECK(h.dependencies.ncontents == 2 && (!strcmp(c, "mod:m1 mod:m2") || !strcmp(c, "mod:m2 mod:m1")), "contents of mod: %s", c) ;
        service_read_free(&h) ;
    }
    CHECK(parsed("mod:m1", 0) && parsed("mod:m2", 0), "the members are written") ;

    // a member named alone and parsed is no refusal on the fly
    run_init(&run) ;
    CHECK(names(&run, PARSE_MISSING, ARGV("mod:m1")) == 1 && !hash_count(&run.hres), "start of a parsed member") ;
    run_free(&run) ;

    auto_strings(path, SS_SERVICE_ADMDIR, "mod/activated/m2") ;
    CHECK(unlink(path) == 0, "deactivate m2") ;

    run_init(&run) ;
    CHECK(names(&run, PARSE_FRONTEND|PARSE_FORCE, ARGV("mod")) == 1, "parse -f mod: %s", run.msg.s) ;
    holds(&run, (char const *[]){ "mod", "mod:m1" }, 2) ;
    CHECK(parse_dropped(&dropped, &run.hres, &run.g) == 1, "dropped") ;
    for (uint32_t i = 0 ; i < service_graph_nvertex(&run.g) ; i++) {
        int want = !strcmp(service_graph_name(&run.g, i), "mod:m2") ;
        CHECK((bitset32_isvalid(&dropped, i) != 0) == want, "%s dropped: %d", service_graph_name(&run.g, i), !want) ;
    }
    run_free(&run) ;
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

    setup_info() ;
    make_tree(SS_DEFAULT_TREENAME) ;

    test_operands() ;
    test_closure() ;
    test_missing() ;
    test_force() ;
    test_cycle() ;
    test_provide() ;
    test_dropped() ;
    test_module() ;
    test_environ() ;

    ssexec_free(&info) ;

    // _exit() does not flush what printf() buffered
    fflush(stdout) ;
    _exit(0) ;
}

int main(void)
{
    printf("Starting parse test...\n") ;

    char root[64] = "/tmp/66-parse-root.XXXXXX" ;
    char path[SS_MAX_PATH_LEN] ;
    int status = 0 ;
    // the layout sanitize_system() makes: the tree resolve files live in <base>/system/.resolve
    char const *dirs[] = { SS_SERVICE_ADMDIR, SS_SERVICE_SYSDIR, SS_SERVICE_ADMCONFDIR, SS_LOGGER_SYSDIR, BASE SS_SYSTEM SS_RESOLVE SS_SERVICE SS_PROVIDE, SS_LIVE, "/tmp/", "/etc/" } ;

    PROG = "parse_names" ;
    VERBOSITY = 0 ;

    CHECK(mkdtemp(root), "make the root") ;

    for (size_t i = 0 ; i < sizeof(dirs) / sizeof(dirs[0]) ; i++) {
        auto_strings(path, root, dirs[i]) ;
        CHECK(dir_create_parent(path, 0755), "create %s", path) ;
    }

    // the runner of a logger, read by write_logger() and parse_clean_runas()
    int other = strcmp(SS_LOGGER_RUNNER, "root") ;
    char const *pw = other ? "root:x:0:0:root:/root:/bin/sh\n" SS_LOGGER_RUNNER ":x:999:999::/:/sbin/nologin\n" : "root:x:0:0:root:/root:/bin/sh\n" ;
    char const *gr = other ? "root:x:0:\n" SS_LOGGER_RUNNER ":x:999:\n" : "root:x:0:\n" ;
    auto_strings(path, root, "/etc/passwd") ;
    CHECK(file_write(path, pw, strlen(pw)), "passwd") ;
    auto_strings(path, root, "/etc/group") ;
    CHECK(file_write(path, gr, strlen(gr)), "group") ;

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
