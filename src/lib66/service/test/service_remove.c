/*
 * service_remove.c
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

/** tree_find_service() and service_remove_one(), by effect. The whole test
 * runs in a child that owns a user namespace (uid 0) and a chroot: the default
 * places of a log directory and of an environment directory, absolute paths of
 * the host, are those of the chroot. */

#include <assert.h>
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
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

#include <66/config.h>
#include <66/constants.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/graph.h>
#include <66/tree.h>
#include <66/enum_parser.h>

#define CHECK(cond, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__) ; \
        fprintf(stderr, __VA_ARGS__) ; \
        fprintf(stderr, "\n") ; \
        abort() ; \
    } \
} while (0)

static char const *base = "/base/" ;

static int exists(char const *path)
{
    struct stat st ;
    return lstat(path, &st) == 0 ;
}

static void mkdirs(char const *path)
{
    CHECK(dir_create_parent(path, 0755), "create %s: errno %d", path, errno) ;
}

static void write_tree(char const *name, char const *contents, uint32_t n)
{
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_TREE, &tres) ;
    resolve_init(&w) ;
    tres.name = resolve_add_string(&w, name) ;
    tres.contents = resolve_add_string(&w, contents) ;
    tres.ncontents = n ;
    CHECK(resolve_write(&w, base, name) == 1, "write the tree %s", name) ;
    resolve_free_obj(&w) ;
}

static void write_master(char const *trees, uint32_t n)
{
    resolve_tree_master_t mres = RESOLVE_TREE_MASTER_ZERO ;
    resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_TREE_MASTER, &mres) ;
    resolve_init(&w) ;
    mres.name = resolve_add_string(&w, SS_MASTER + 1) ;
    mres.contents = resolve_add_string(&w, trees) ;
    mres.ncontents = n ;
    CHECK(resolve_write(&w, base, SS_MASTER + 1) == 1, "write the Master") ;
    resolve_free_obj(&w) ;
}

static void contents_is(char const *tree, char const *want)
{
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_TREE, &tres) ;
    CHECK(resolve_read(&w, base, tree) == 1, "read the tree %s", tree) ;
    CHECK(!strcmp(tres.sa.s + tres.contents, want), "tree %s holds '%s', expected '%s'", tree, tres.sa.s + tres.contents, want) ;
    resolve_free_obj(&w) ;
}

static void servicedir(char *dir, char const *name)
{
    auto_strings(dir, base, SS_SYSTEM, SS_SERVICE, SS_SVC, "/", name) ;
}

static void resolvelink(char *link, char const *name)
{
    auto_strings(link, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;
}

// the service directory of @name and the link the graph lists
static void layout(char const *name)
{
    char dir[SS_MAX_PATH_LEN], res[SS_MAX_PATH_LEN], link[SS_MAX_PATH_LEN], entry[SS_MAX_PATH_LEN] ;

    servicedir(dir, name) ;
    auto_strings(res, dir, SS_RESOLVE) ;
    auto_strings(entry, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE) ;
    resolvelink(link, name) ;

    mkdirs(res) ;
    mkdirs(entry) ;
    CHECK(symlink(dir, link) == 0, "link %s", link) ;
}

typedef struct one_s one_t ;
struct one_s
{
    char const *name ;
    char const *tree ;
    uint32_t islog ;
    uint32_t logger ;
    uint32_t type ;
    char const *logdir ; // 0: no io addon
    char const *envdir ; // 0: no environ addon
    char const *contents ; // 0: no dependencies addon
} ;

static void write_one(one_t const *o)
{
    char dir[SS_MAX_PATH_LEN] ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_SERVICE, &res) ;

    layout(o->name) ;
    servicedir(dir, o->name) ;

    resolve_init(&w) ;
    res.name = resolve_add_string(&w, o->name) ;
    res.treename = resolve_add_string(&w, o->tree) ;
    res.path.home = resolve_add_string(&w, base) ;
    res.path.servicedir = resolve_add_string(&w, dir) ;
    res.owner = 0 ;
    res.type = o->type ;
    res.islog = o->islog ;
    res.logger = o->logger ;
    res.has_io = o->logdir ? 1 : 0 ;
    res.has_environ = o->envdir ? 1 : 0 ;
    res.has_dependencies = o->contents ? 1 : 0 ;
    CHECK(resolve_write(&w, base, o->name) == 1, "write %s", o->name) ;
    resolve_free_obj(&w) ;

    if (o->logdir) {
        resolve_service_addon_io_t io = RESOLVE_SERVICE_ADDON_IO_ZERO ;
        resolve_wrapper_t wio = RESOLVE_WRAPPER(DATA_SERVICE_IO, &io) ;
        resolve_init(&wio) ;
        io.fdout.type = E_PARSER_IO_TYPE_66LOG ;
        io.fdout.destination = resolve_add_string(&wio, o->logdir) ;
        CHECK(resolve_write(&wio, base, o->name) == 1, "write the io of %s", o->name) ;
        resolve_free_obj(&wio) ;
        mkdirs(o->logdir) ;
    }

    if (o->envdir) {
        resolve_service_addon_environ_t env = RESOLVE_SERVICE_ADDON_ENVIRON_ZERO ;
        resolve_wrapper_t wenv = RESOLVE_WRAPPER(DATA_SERVICE_ENVIRON, &env) ;
        resolve_init(&wenv) ;
        env.envdir = resolve_add_string(&wenv, o->envdir) ;
        CHECK(resolve_write(&wenv, base, o->name) == 1, "write the environ of %s", o->name) ;
        resolve_free_obj(&wenv) ;
        mkdirs(o->envdir) ;
    }

    if (o->contents) {
        resolve_service_addon_dependencies_t dep = RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
        resolve_wrapper_t wdep = RESOLVE_WRAPPER(DATA_SERVICE_DEPENDENCIES, &dep) ;
        resolve_init(&wdep) ;
        dep.contents = resolve_add_string(&wdep, o->contents) ;
        dep.ncontents = 1 ;
        CHECK(resolve_write(&wdep, base, o->name) == 1, "write the dependencies of %s", o->name) ;
        resolve_free_obj(&wdep) ;
    }
}

// a resolve file that does not read
static void write_garbage(char const *name)
{
    char file[SS_MAX_PATH_LEN] ;
    layout(name) ;
    auto_strings(file, base, SS_SYSTEM, SS_SERVICE, SS_SVC, "/", name, SS_RESOLVE, "/", name) ;
    CHECK(file_write(file, "garbage", 7), "write garbage at %s", file) ;
}

static void provide(char const *name, char const *holder)
{
    char dir[SS_MAX_PATH_LEN], link[SS_MAX_PATH_LEN], target[SS_MAX_PATH_LEN] ;
    auto_strings(dir, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, SS_PROVIDE) ;
    auto_strings(link, dir, "/", name) ;
    auto_strings(target, "../", holder) ;
    mkdirs(dir) ;
    CHECK(symlink(target, link) == 0, "link %s", link) ;
}

static void provide_path(char *link, char const *name)
{
    auto_strings(link, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, SS_PROVIDE, "/", name) ;
}

// nothing of @name is left in <base>/system
static void gone(char const *name)
{
    char path[SS_MAX_PATH_LEN] ;
    servicedir(path, name) ;
    CHECK(!exists(path), "the service directory of %s is gone", name) ;
    resolvelink(path, name) ;
    CHECK(!exists(path), "the resolve link of %s is gone", name) ;
}

static void kept(char const *name)
{
    char path[SS_MAX_PATH_LEN] ;
    servicedir(path, name) ;
    CHECK(exists(path), "the service directory of %s is kept", name) ;
    resolvelink(path, name) ;
    CHECK(exists(path), "the resolve link of %s is kept", name) ;
}

static void test_tree_find_service(void)
{
    printf("tree_find_service: the tree whose contents name the service, without its resolve file\n") ;

    _cleanup_strbuf_ strbuf tree = STRBUF_ZERO ;

    // no Master: no tree to look in
    CHECK(tree_find_service(&tree, base, "a") == 0 && !tree.len, "no Master, no tree") ;

    write_master("t1 t2 t3", 3) ;
    write_tree("t1", "a u", 2) ;
    write_tree("t2", "b x", 2) ;
    // t3 is listed and has no resolve file: it names nobody

    CHECK(tree_find_service(&tree, base, "x") == 1 && !strcmp(tree.s, "t2"), "x is in t2") ;
    tree.len = 0 ;
    CHECK(tree_find_service(&tree, base, "u") == 1 && !strcmp(tree.s, "t1"), "u is in t1") ;
    tree.len = 0 ;
    CHECK(tree_find_service(&tree, base, "nobody") == 0 && !tree.len, "nobody is in no tree") ;
}

static void test_readable(void)
{
    printf("remove a readable service: its directories, its logger, its tree, its Provide names, nobody else\n") ;

    char path[SS_MAX_PATH_LEN] ;
    one_t x = { .name = "x", .tree = "t2", .logger = 1, .type = E_PARSER_TYPE_CLASSIC, .logdir = "/var/log/66/x", .envdir = "/etc/66/conf/x", .contents = 0 } ;
    one_t xlog = { .name = "x-log", .tree = "t2", .islog = 1, .type = E_PARSER_TYPE_CLASSIC, .logdir = "/var/log/66/x", .envdir = 0, .contents = 0 } ;
    one_t b = { .name = "b", .tree = "t2", .type = E_PARSER_TYPE_CLASSIC } ;

    write_one(&x) ;
    write_one(&xlog) ;
    write_one(&b) ;
    provide("px", "x") ;
    provide("pb", "b") ;

    CHECK(service_remove_one(base, 0, "x") == 1, "remove x: errno %d", errno) ;

    gone("x") ;
    gone("x-log") ;
    kept("b") ;
    CHECK(!exists("/var/log/66/x"), "the log directory of x is gone") ;
    CHECK(!exists("/etc/66/conf/x"), "the environment directory of x is gone") ;
    contents_is("t2", "b") ;
    provide_path(path, "px") ;
    CHECK(!exists(path), "the name x provided is released") ;
    provide_path(path, "pb") ;
    CHECK(exists(path), "the name b provides stays") ;

    // already gone: nothing to do, no failure
    CHECK(service_remove_one(base, 0, "x") == 1, "remove x again: errno %d", errno) ;
}

static void test_unreadable(void)
{
    printf("remove an unreadable service: its tree by its contents, its files at their default places\n") ;

    char path[SS_MAX_PATH_LEN] ;

    write_garbage("u") ;
    provide("pu", "u") ;
    mkdirs(SS_LOGGER_SYSDIR "u") ;
    mkdirs(SS_SERVICE_ADMCONFDIR "u") ;

    CHECK(service_remove_one(base, 0, "u") == 1, "remove u: errno %d", errno) ;

    gone("u") ;
    contents_is("t1", "a") ;
    CHECK(!exists(SS_LOGGER_SYSDIR "u"), "the default log directory of u is gone") ;
    CHECK(!exists(SS_SERVICE_ADMCONFDIR "u"), "the default environment directory of u is gone") ;
    provide_path(path, "pu") ;
    CHECK(!exists(path), "the name u held is released") ;
}

static void test_loggers(void)
{
    printf("the logger of an unreadable service is the vertex named <name>-log; a service under that name is none\n") ;

    one_t r = { .name = "r", .tree = "t1", .type = E_PARSER_TYPE_CLASSIC } ;
    one_t rlog = { .name = "r-log", .tree = "t1", .type = E_PARSER_TYPE_CLASSIC } ;
    one_t vlog = { .name = "v-log", .tree = "t1", .islog = 1, .type = E_PARSER_TYPE_CLASSIC } ;

    // both unreadable: the logger goes with its service
    write_garbage("w") ;
    write_garbage("w-log") ;
    CHECK(service_remove_one(base, 0, "w") == 1, "remove w: errno %d", errno) ;
    gone("w") ;
    gone("w-log") ;

    // r readable says no logger: r-log, parsed before 0.9.2.0, stays
    write_one(&r) ;
    write_one(&rlog) ;
    CHECK(service_remove_one(base, 0, "r") == 1, "remove r: errno %d", errno) ;
    gone("r") ;
    kept("r-log") ;

    // v unreadable, v-log reads as a logger: it goes
    write_garbage("v") ;
    write_one(&vlog) ;
    CHECK(service_remove_one(base, 0, "v") == 1, "remove v: errno %d", errno) ;
    gone("v") ;
    gone("v-log") ;
}

static void test_module(void)
{
    printf("a module comes with its members; a member alone goes alone\n") ;

    one_t m = { .name = "m", .tree = "t1", .type = E_PARSER_TYPE_MODULE, .contents = "m:a" } ;
    one_t ma = { .name = "m:a", .tree = "t1", .type = E_PARSER_TYPE_CLASSIC } ;
    one_t n = { .name = "n", .tree = "t1", .type = E_PARSER_TYPE_MODULE, .contents = "n:a" } ;
    one_t na = { .name = "n:a", .tree = "t1", .type = E_PARSER_TYPE_CLASSIC } ;

    write_one(&m) ;
    write_one(&ma) ;
    CHECK(service_remove_one(base, 0, "m") == 1, "remove m: errno %d", errno) ;
    gone("m") ;
    gone("m:a") ;

    write_one(&n) ;
    write_one(&na) ;
    CHECK(service_remove_one(base, 0, "n:a") == 1, "remove n:a: errno %d", errno) ;
    gone("n:a") ;
    kept("n") ;
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

    // resolve_write() writes the system: the test holds its write transaction
    CHECK(service_graph_write_begin(0, base, SS_LIVE) == 1, "hold the transaction: errno %d", errno) ;

    {
        char dir[SS_MAX_PATH_LEN] ;
        auto_strings(dir, base, SS_SYSTEM, SS_RESOLVE) ;
        mkdirs(dir) ;
    }

    test_tree_find_service() ;
    test_readable() ;
    test_unreadable() ;
    test_loggers() ;
    test_module() ;

    CHECK(service_graph_write_end(0) >= 0, "release the transaction") ;

    fflush(stdout) ;
    _exit(0) ;
}

int main(void)
{
    printf("Starting service remove test...\n") ;

    char root[64] = "/tmp/66-service-remove.XXXXXX" ;
    int status = 0 ;

    PROG = "service_remove" ;
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
