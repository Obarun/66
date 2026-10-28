/*
 * tree_contents.c
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

/* tree_contents_edit(), the one edit of the contents of a tree,
 * tree_find_current(), the one choice of the tree of a command without
 * -t, and tree_get_permissions(), the allow list of a tree, by effect, on resolve files written under a private mkdtemp directory
 * inside a write transaction. */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <oblibs/string.h>
#include <oblibs/directory.h>
#include <oblibs/strbuf.h>
#include <oblibs/sbl.h>
#include <oblibs/types.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/resolve.h>
#include <66/graph.h>
#include <66/tree.h>

static char base[64] = "/tmp/66-tree-contents.XXXXXX" ;

static void write_tree(char const *name)
{
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_TREE, &tres) ;
    resolve_init(&w) ;
    tres.name = resolve_add_string(&w, name) ;
    tres.contents = resolve_add_string(&w, "") ;
    assert(resolve_write(&w, base, name) == 1) ;
    resolve_free_obj(&w) ;
}

// a tree @name whose allow list is @allow, @nallow uids
static void write_tree_allow(char const *name, char const *allow, uint32_t nallow)
{
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_TREE, &tres) ;
    resolve_init(&w) ;
    tres.name = resolve_add_string(&w, name) ;
    tres.contents = resolve_add_string(&w, "") ;
    tres.allow = resolve_add_string(&w, allow) ;
    tres.nallow = nallow ;
    assert(resolve_write(&w, base, name) == 1) ;
    resolve_free_obj(&w) ;
}

static void write_master(char const *current)
{
    resolve_tree_master_t mres = RESOLVE_TREE_MASTER_ZERO ;
    resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_TREE_MASTER, &mres) ;
    resolve_init(&w) ;
    mres.name = resolve_add_string(&w, SS_MASTER + 1) ;
    mres.current = current ? resolve_add_string(&w, current) : 0 ;
    assert(resolve_write(&w, base, SS_MASTER + 1) == 1) ;
    resolve_free_obj(&w) ;
}

// the contents of @tree on disk as one string, separated by spaces
static void contents_is(char const *tree, char const *want, uint32_t n)
{
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_TREE, &tres) ;
    assert(resolve_read(&w, base, tree) == 1) ;
    assert(tres.ncontents == n) ;
    assert(!strcmp(tres.sa.s + tres.contents, want)) ;
    resolve_free_obj(&w) ;
}

static void test_edit(void)
{
    printf("tree_contents_edit: add, add again, remove, a tree gone\n") ;

    write_tree("t") ;

    assert(tree_contents_edit(base, "t", "a", true) == 1) ;
    contents_is("t", "a", 1) ;
    assert(tree_contents_edit(base, "t", "b", true) == 1) ;
    contents_is("t", "a b", 2) ;

    // already there: nothing written, the same contents
    assert(tree_contents_edit(base, "t", "a", true) == 1) ;
    contents_is("t", "a b", 2) ;

    assert(tree_contents_edit(base, "t", "a", false) == 1) ;
    contents_is("t", "b", 1) ;
    // not there: nothing to remove
    assert(tree_contents_edit(base, "t", "a", false) == 1) ;
    contents_is("t", "b", 1) ;
    assert(tree_contents_edit(base, "t", "b", false) == 1) ;
    contents_is("t", "", 0) ;

    errno = 0 ;
    assert(tree_contents_edit(base, "gone", "a", true) == 0 && errno == ENOENT) ;
    errno = 0 ;
    assert(tree_service_remove(base, "gone", "a") == 1) ;
}

static void test_current(void)
{
    printf("tree_find_current: the current tree, else the default one\n") ;

    char tree[SS_MAX_TREENAME + 1] ;

    // no Master: it does not read
    assert(tree_find_current(tree, base) == -1) ;

    write_master(0) ;
    assert(tree_find_current(tree, base) == 1 && !strcmp(tree, SS_DEFAULT_TREENAME)) ;

    write_master("t") ;
    assert(tree_find_current(tree, base) == 1 && !strcmp(tree, "t")) ;
}

static void test_permissions(void)
{
    printf("tree_get_permissions: allowed, not allowed, no allow list, a tree gone\n") ;

    char me[U32_FMT], allow[3 * U32_FMT + 3] ;
    uint32_t other = getuid() + 1 ;
    me[u32_fmt(me, getuid())] = 0 ;

    char o[U32_FMT] ;
    o[u32_fmt(o, other)] = 0 ;

    // the user of the process is listed, after another one
    auto_strings(allow, o, " ", me) ;
    write_tree_allow("allowed", allow, 2) ;
    assert(tree_get_permissions(base, "allowed") == 1) ;

    // another user only: not allowed, an answer, not an error
    write_tree_allow("other", o, 1) ;
    assert(tree_get_permissions(base, "other") == 0) ;

    // no allow list: nobody is allowed
    write_tree_allow("nobody", "", 0) ;
    assert(tree_get_permissions(base, "nobody") == 0) ;

    // a tree that does not read: no answer, errno of the read
    errno = 0 ;
    assert(tree_get_permissions(base, "gone") == -1 && errno == ENOENT) ;
}

int main(void)
{
    printf("Starting tree contents test...\n") ;

    assert(mkdtemp(base)) ;
    strcat(base, "/") ;

    assert(service_graph_write_begin(getuid(), base, base) == 1) ;

    {
        char dir[SS_MAX_PATH_LEN] ;
        auto_strings(dir, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, SS_PROVIDE) ;
        assert(dir_create_parent(dir, 0755)) ;
    }

    test_edit() ;
    test_current() ;
    test_permissions() ;

    assert(service_graph_write_end(0) == 1) ;
    assert(dir_destroy(base)) ;

    printf("All tests passed!\n") ;
    return 0 ;
}
