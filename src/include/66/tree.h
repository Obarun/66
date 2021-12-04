/*
 * tree.h
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#ifndef SS_TREE_H
#define SS_TREE_H

#include <sys/types.h>

#include <skalibs/stralloc.h>
#include <skalibs/cdb.h>
#include <skalibs/cdbmake.h>

#include <66/ssexec.h>

#define TREE_STRUCT 1

typedef struct resolve_tree_s resolve_tree_t, *resolve_tree_t_ref ;
struct resolve_tree_s
{
    uint32_t salen ;
    stralloc sa ;

    uint32_t name ;
    uint32_t depends ;
    uint32_t requiredby ;
    uint32_t allow ;
    uint32_t contents ;

    uint32_t ndepends ;
    uint32_t nrequiredby ;
    uint32_t ncontents ;
    uint32_t current ;//no->0, yes->1
    uint32_t init ;//not initialized->0, initialized->1
    uint32_t disen ;//disable->0, enable->1
} ;
#define RESOLVE_TREE_ZERO { 0,STRALLOC_ZERO,0,0,0,0,0,0,0,0,0,0,0 }

extern stralloc saseed ;

typedef struct tree_seed_s tree_seed_t, tree_seed_t_ref ;
struct tree_seed_s
{
    int name ;
    int depends ;
    int requiredby ;
    uint8_t enabled ;
    int allow ;
    int deny ;
    uint8_t current ;
    int group ;
    int services ;
    uint8_t nopts ;

} ;
#define TREE_SEED_ZERO { -1, -1, -1, 0, -1, -1, 0, -1, -1, 0 }

extern int tree_cmd_state(unsigned int verbosity,char const *cmd,char const *tree) ;
extern int tree_state(int argc, char const *const *argv) ;

extern int tree_copy(stralloc *dir, char const *tree,char const *treename) ;

extern int tree_copy_tmp(char const *workdir, ssexec_t *info) ;

/** Set the tree to use as current for 66 tools
 * This is avoid to use the -t options for all 66 tools
 * Search on @base the directory current and append @tree
 * with the path.
 * @Return 1 on success
 * @Return 0 on fail */
extern int tree_find_current(stralloc *tree, char const *base,uid_t owner) ;

extern int tree_get_permissions(char const *tree, uid_t owner) ;

extern int tree_sethome(ssexec_t *info) ;

extern char tree_setname(stralloc *sa, char const *tree) ;

extern int tree_switch_current(char const *base, char const *tree) ;

extern int tree_isvalid(ssexec_t *info) ;

/**
 *
 * Resolve API
 *
 * */

extern int tree_read_cdb(cdb *c, resolve_tree_t *tres) ;
extern int tree_write_cdb(cdbmaker *c, resolve_tree_t *tres) ;
extern int tree_resolve_copy(resolve_tree_t *dst, resolve_tree_t *tres) ;

/**
 *
 * Seed API
 *
 * */

extern void tree_seed_free(void) ;
extern int tree_seed_setseed(tree_seed_t *seed, char const *treename, uint8_t check_service) ;
extern int tree_seed_isvalid(char const *seed) ;

#endif
