/*
 * command_refuse.c
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

/** The refusals the commands share, by effect: the exit code and the line of
 * the contract for a name no vertex answers to (unknown, not parsed, provided
 * by another under remove) and for a selection that meets an absent or an
 * invalid service, its frontend gone included; nothing said and nothing
 * refused out of the refusals asked. A system is written under a private
 * mkdtemp directory; each refusal runs in a child that owns a user namespace
 * and a chroot holding the frontend directories, so that nothing of the host
 * is read, and dies there. */

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
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
#include <oblibs/io.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/command.h>

#define CHECK(cond, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__) ; \
        fprintf(stderr, __VA_ARGS__) ; \
        fprintf(stderr, "\n") ; \
        abort() ; \
    } \
} while (0)

static char root[64] = "/tmp/66-command-refuse.XXXXXX" ;
static char base[SS_MAX_PATH_LEN] ;

/**
 *
 * Fixtures
 *
 * */

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

static void write_service(char const *name, char const *depends)
{
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t w = { .type = DATA_SERVICE, .obj = &res } ;

    make_layout(name) ;
    resolve_init(&w) ;
    res.name = resolve_add_string(&w, name) ;
    res.path.home = resolve_add_string(&w, base) ;
    CHECK(resolve_write(&w, base, name) == 1, "write the resolve file of %s", name) ;
    strbuf_free(&res.sa) ;

    if (!*depends)
        return ;

    resolve_service_addon_dependencies_t dep = RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
    resolve_wrapper_t wd = { .type = DATA_SERVICE_DEPENDENCIES, .obj = &dep } ;
    uint32_t n = 1 ;

    for (char const *p = depends ; *p ; p++)
        n += *p == ' ' ;

    resolve_init(&wd) ;
    dep.depends = resolve_add_string(&wd, depends) ;
    dep.ndepends = n ;
    dep.requiredby = resolve_add_string(&wd, "") ;
    dep.optsdeps = resolve_add_string(&wd, "") ;
    dep.contents = resolve_add_string(&wd, "") ;
    dep.provide = resolve_add_string(&wd, "") ;
    dep.conflict = resolve_add_string(&wd, "") ;
    CHECK(resolve_write(&wd, base, name) == 1, "write the dependencies of %s", name) ;
    strbuf_free(&dep.sa) ;
}

static void write_system(void)
{
    char file[SS_MAX_PATH_LEN], dir[SS_MAX_PATH_LEN] ;

    write_service("a", "") ;
    write_service("b", "a ab") ;
    write_service("c", "gone") ;
    write_service("d", "") ;
    write_service("w", "u") ;
    write_service("x", "y") ;
    write_service("y", "x") ;
    write_service("z", "x") ;
    write_service("m", "x zgone") ;
    write_service("pv-conn", "") ;

    make_layout("u") ;
    auto_strings(file, base, SS_SYSTEM, SS_SERVICE, SS_SVC, "/u", SS_RESOLVE, "/u") ;
    CHECK(file_write(file, "garbage", 7), "write garbage at %s", file) ;

    auto_strings(dir, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, SS_PROVIDE) ;
    auto_strings(file, dir, "/pv-net") ;
    CHECK(dir_create_parent(dir, 0755), "create %s", dir) ;
    CHECK(symlink("../pv-conn", file) == 0, "link %s", file) ;

    // the frontends of owner 0, every service but d and the absent gone
    char const *fe[] = { "a", "ab", "b", "c", "w", "u", "x", "y", "z", "m", "pv-conn", "free-fe" } ;
    auto_strings(dir, root, SS_SERVICE_ADMDIR) ;
    CHECK(dir_create_parent(dir, 0755), "create %s", dir) ;
    auto_strings(dir, root, SS_SERVICE_SYSDIR) ;
    CHECK(dir_create_parent(dir, 0755), "create %s", dir) ;

    for (size_t i = 0 ; i < sizeof(fe) / sizeof(fe[0]) ; i++) {
        auto_strings(file, root, SS_SERVICE_ADMDIR, fe[i]) ;
        CHECK(file_write(file, "", 0), "write the frontend of %s", fe[i]) ;
    }
}

static uint32_t idx(service_graph_t const *g, char const *name)
{
    uint32_t i = service_graph_find(g, name) ;
    CHECK(i != SERVICE_GRAPH_NOINDEX, "vertex %s is in the graph", name) ;
    return i ;
}

static void draw(selection_service_t *sel, service_graph_t const *g, char const *name, uint8_t propagate)
{
    bitset32_t seeds = bitset32_init(SS_MAX_SERVICE) ;
    selection_service_policy_t policy = SELECTION_SERVICE_POLICY_ZERO ;

    policy.propagate = propagate ;
    bitset32_set(&seeds, idx(g, name)) ;
    selection_service_propagate(sel, g, &seeds, &policy) ;
}

/**
 *
 * Inside the namespace
 *
 * */

typedef struct case_s case_t ;
struct case_s
{
    char const *what ;
    int unknown ; // command_refuse_unknown() of @name, else command_refuse_selection() of @name propagated by @propagate
    char const *name ;
    int literal ;
    uint8_t propagate ;
    uint32_t refusals ;
    int code ; // the exit code, 0 when the call returns
    char const *line ; // the line said, 0 when nothing is
} ;

// run @c in a child, the standard error in /err: its exit code and what it said
static void run_case(case_t const *c, service_graph_t const *g)
{
    int status = 0 ;
    _cleanup_strbuf_ strbuf err = STRBUF_ZERO ;

    CHECK(file_write("/err", "", 0), "empty /err") ;

    pid_t pid = fork() ;
    CHECK(pid >= 0, "fork") ;

    if (!pid) {

        int fd = (int)io_open_mode("/err", O_WRONLY | O_APPEND, 0) ;
        CHECK(fd >= 0 && dup2(fd, 2) == 2, "redirect the standard error") ;

        if (c->unknown) {

            command_refuse_unknown(c->name, "/base/", 0, c->literal) ;

        } else {

            selection_service_t sel ;
            draw(&sel, g, c->name, c->propagate) ;
            command_refuse_selection(&sel, &(strbuf)STRBUF_ZERO, c->refusals) ;
        }

        _exit(0) ;
    }

    CHECK(waitpid(pid, &status, 0) == pid, "wait the case") ;
    CHECK(strbuf_read_file(&err, "/err"), "read /err") ;
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == c->code, "%s: exit %d, expected %d\n%s", c->what, WIFEXITED(status) ? WEXITSTATUS(status) : -1, c->code, err.len ? err.s : "") ;

    if (c->line) {
        CHECK(err.len && strstr(err.s, c->line), "%s: says %s\n--- got:\n%s", c->what, c->line, err.len ? err.s : "") ;
        CHECK(strchr(err.s, '\n') == err.s + err.len - 1, "%s: one line only\n%s", c->what, err.s) ;
    } else
        CHECK(!err.len, "%s: says nothing\n%s", c->what, err.s) ;
}

static void refuse_child(service_graph_t const *g)
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

    uint32_t const NA = SELECTION_SERVICE_REFUSE_NAMED_ABSENT, MA = SELECTION_SERVICE_REFUSE_MET_ABSENT ;
    uint32_t const NI = SELECTION_SERVICE_REFUSE_NAMED_INVALID, MI = SELECTION_SERVICE_REFUSE_MET_INVALID ;
    uint8_t const NONE = SELECTION_SERVICE_PROPAGATE_NONE, DEP = SELECTION_SERVICE_PROPAGATE_DEPENDS ;

    case_t cases[] = {
        { "an unknown name", 1, "nosuch", 0, 0, 0, 100,
            "service: nosuch is unknown: no frontend and no service answers to the name -- is its provider enabled?" },
        { "an empty name", 1, "", 0, 0, 0, 100,
            "service:  is unknown: no frontend and no service answers to the name -- is its provider enabled?" },
        { "a frontend never parsed", 1, "free-fe", 0, 0, 0, 100,
            "service: free-fe is not parsed -- parse it with 66 parse free-fe" },
        { "a provided name taken literally", 1, "pv-net", 1, 0, 0, 100,
            "name: pv-net is provided by: pv-conn -- '66 remove pv-conn' instead" },
        { "an unknown name taken literally", 1, "nosuch", 1, 0, 0, 100,
            "service: nosuch is unknown: no frontend and no service answers to the name -- is its provider enabled?" },
        { "an absent service named", 0, "ab", 0, NONE, NA, 100,
            "service: ab is not parsed -- parse it with 66 parse ab" },
        { "an absent service met", 0, "b", 0, DEP, MA, 100,
            "service: ab is not parsed -- parse it with 66 parse ab" },
        { "an absent service met, only named ones refused", 0, "b", 0, DEP, NA | NI | MI, 0, 0 },
        { "an absent service met without frontend", 0, "c", 0, DEP, MA, 100,
            "service: gone has neither a resolve file nor a frontend -- remove it with 66 remove gone" },
        { "an unreadable service met", 0, "w", 0, DEP, MI, 100,
            "service: u has an unreadable resolve file, and is required by: w -- repair it with 66 reconfigure -c u" },
        { "an unreadable service met, only named ones refused", 0, "w", 0, DEP, NI, 0, 0 },
        { "a service on a cycle named", 0, "x", 0, NONE, NI, 100,
            "service: x cannot be ordered: it is part of, or depends on, a dependency cycle -- break the cycle in the frontends, then re-parse them with 66 parse -f" },
        { "a frontend gone, named", 0, "d", 0, NONE, NI, 100,
            "service: d has no frontend anywhere any more -- remove it with 66 remove d" },
        { "a frontend gone, out of the refusals asked", 0, "d", 0, NONE, MI | NA | MA, 0, 0 },
        { "a valid selection", 0, "b", 0, NONE, NA | MA | NI | MI, 0, 0 },
        { "an absent service named is not met", 0, "ab", 0, NONE, MA, 0, 0 },
        { "out of the start order, the first by index", 0, "z", 0, DEP, MI, 100,
            "service: x cannot be ordered: it is part of, or depends on, a dependency cycle -- break the cycle in the frontends, then re-parse them with 66 parse -f" },
        { "the start order before the index", 0, "m", 0, DEP, MA | MI, 100,
            "service: zgone has neither a resolve file nor a frontend -- remove it with 66 remove zgone" },
    } ;

    for (size_t i = 0 ; i < sizeof(cases) / sizeof(cases[0]) ; i++)
        run_case(&cases[i], g) ;

    _exit(0) ;
}

int main(void)
{
    printf("Starting command refuse test...\n") ;

    service_graph_t g = SERVICE_GRAPH_ZERO ;
    int status = 0 ;

    PROG = "command_refuse" ;
    VERBOSITY = 1 ;
    CHECK(mkdtemp(root), "make the root") ;
    auto_strings(base, root, "/base/") ;

    CHECK(service_graph_write_begin(getuid(), base, base) == 1, "hold the transaction") ;
    write_system() ;
    CHECK(service_graph_write_end(0) >= 0, "end the transaction") ;

    CHECK(service_graph_load(&g, getuid(), base, base) == 1, "load the graph: errno %d", errno) ;
    CHECK(service_graph_state(&g, idx(&g, "ab")) == SERVICE_GRAPH_STATE_ABSENT, "ab is absent") ;
    CHECK(service_graph_state(&g, idx(&g, "u")) == SERVICE_GRAPH_STATE_UNREADABLE, "u does not read") ;
    CHECK(service_graph_state(&g, idx(&g, "x")) == SERVICE_GRAPH_STATE_UNORDERED, "x is out of the order") ;
    CHECK(service_graph_state(&g, idx(&g, "zgone")) == SERVICE_GRAPH_STATE_ABSENT && idx(&g, "x") < idx(&g, "zgone"), "zgone is absent, after x by index") ;

    printf("unknown names and selections refused with the line of the contract, in a chroot of a user namespace\n") ;

    fflush(stdout) ;
    pid_t pid = fork() ;
    CHECK(pid >= 0, "fork") ;

    if (!pid)
        refuse_child(&g) ;

    CHECK(waitpid(pid, &status, 0) == pid, "wait the child") ;
    CHECK(WIFEXITED(status) && !WEXITSTATUS(status), "the child passed: status %d", status) ;

    service_graph_free(&g) ;
    CHECK(dir_destroy(root), "remove the root") ;

    printf("All tests passed!\n") ;

    return 0 ;
}
