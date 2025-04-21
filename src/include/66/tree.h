/*
 * tree.h
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
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
#include <stdint.h>

#include <oblibs/hash.h>

#include <skalibs/stralloc.h>
#include <skalibs/cdb.h>
#include <skalibs/cdbmake.h>
#include <skalibs/tai.h>

#include <66/ssexec.h>
#include <66/resolve.h>
#include <66/graph.h>
#include <66/enum_tree.h>

#define TREE_GROUPS_BOOT "boot"
#define TREE_GROUPS_BOOT_LEN (sizeof TREE_GROUPS_BOOT - 1)
#define TREE_GROUPS_ADM "admin"
#define TREE_GROUPS_ADM_LEN (sizeof TREE_GROUPS_ADM - 1)
#define TREE_GROUPS_USER "user"
#define TREE_GROUPS_USER_LEN (sizeof TREE_GROUPS_USER - 1)

typedef struct resolve_tree_s resolve_tree_t, *resolve_tree_t_ref ;
struct resolve_tree_s
{
    stralloc sa ;
    uint32_t rversion ; //string, version of 66 of the resolve file at write time

    uint32_t name ;
    uint32_t enabled ;
    uint32_t depends ;
    uint32_t requiredby ;
    uint32_t allow ;
    uint32_t groups ;
    uint32_t contents ;

    uint32_t ndepends ;
    uint32_t nrequiredby ;
    uint32_t nallow ;
    uint32_t ngroups ; //not really useful for now, we accept only one group
    uint32_t ncontents ;

    uint32_t init ;//not initialized->0, initialized->1
    uint32_t supervised ;//not superviseded->0, supervised->1
} ;

#define RESOLVE_TREE_ZERO { STRALLOC_ZERO,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }

extern const resolve_tree_t tree_resolve_zero ;

typedef struct resolve_tree_master_s resolve_tree_master_t, *resolve_tree_master_t_ref ;
struct resolve_tree_master_s
{
    stralloc sa ;
    uint32_t rversion ; //string, version of 66 of the resolve file at write time

    uint32_t name ;
    uint32_t allow ;
    uint32_t current ;
    uint32_t contents ;

    uint32_t nallow ;
    uint32_t ncontents ;

} ;

#define RESOLVE_TREE_MASTER_ZERO { STRALLOC_ZERO,0,0,0,0,0,0,0 }

extern const resolve_tree_master_t tree_resolve_master_zero ;

typedef struct tree_seed_s tree_seed_t, tree_seed_t_ref ;
struct tree_seed_s
{
    stralloc sa ;

    int name ;
    int depends ;
    int requiredby ;
    int allow ;
    int deny ;
    int groups ;
    int contents ;

    uint8_t current ;
    uint8_t disen ;

    uint8_t nopts ;
} ;

#define TREE_SEED_ZERO { STRALLOC_ZERO, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0 }

struct resolve_hash_tree_s {
	char name[SS_MAX_SERVICE_NAME + 1] ; // name as key
	uint8_t visit ;
	resolve_tree_t tres ;
	UT_hash_handle hh ;

} ;

#define RESOLVE_HASH_TREE_ZERO { 0, 0, RESOLVE_TREE_ZERO, NULL }

#define TREE_FLAGS_STARTING 1 // 1 starting not really up
#define TREE_FLAGS_STOPPING (1 << 1) // 2 stopping not really down
#define TREE_FLAGS_UP (1 << 2) // 4 really up
#define TREE_FLAGS_DOWN (1 << 3) // 8 really down
#define TREE_FLAGS_BLOCK (1 << 4) // 16 all deps are not up/down
#define TREE_FLAGS_UNBLOCK (1 << 5) // 32 all deps up/down
#define TREE_FLAGS_FATAL (1 << 6) // 64 process crashed

typedef struct pidtree_s pidtree_t, *pidtree_t_ref ;
struct pidtree_s
{
    int pipe[2] ;
    pid_t pid ;
    resolve_tree_t *tres ;
    uint32_t index ; // index number of the vertex
    uint8_t state ; // current state of the vertex
    uint32_t nedge ; // number
    vertex_t *notif[SS_MAX_SERVICE] ; // array of vertex_t to notif when a edge is done
    uint32_t nnotif ;// number
} ;

#define PIDTREE_ZERO { \
    .pipe[0] = -1, \
    .pipe[1] = -1, \
    .tres = NULL, \
    .index = 0, \
    .state = 0, \
    .nedge = 0, \
    .notif = {NULL}, \
    .nnotif = 0 \
}

/** @Return 1 on success
 * @Return 0 if not valid
 * @Return -1 on system error */
extern int tree_isvalid(char const *base, char const *treename) ;

/** Append @tree with the name of the current tree
 * @Return 1 on success
 * @Return 0 on fail */
extern int tree_find_current(char *tree, char const *base) ;

/** @Return 1 on success
 * @Return 0 if not valid
 * @Return -1 on system error */
extern int tree_iscurrent(char const *base, char const *treename) ;

/** @Return 1 on success
 * @Return 0 on fail
 * @Return -1 on system error */
extern int tree_isinitialized(char const *base, char const *treename) ;

/** @Return 1 on success
 * @Return 0 on fail
 * @Return -1 on system error */
extern int tree_issupervised(char const *base, char const *treename) ;

/** @Return 1 on success
 * @Return 0 if not valid
 * @Return -1 on system error */
extern int tree_isenabled(char const *base, char const *treename) ;

/** @Return 1 on success
 * @Return 0 if not valid
 * @Return -1 on system error */
extern int tree_ongroups(char const *base, char const *treename, char const *group) ;

extern int tree_copy(stralloc *dir, char const *tree,char const *treename) ;

extern int tree_get_permissions(char const *base, char const *treename) ;

extern int tree_sethome(ssexec_t *info) ;

extern int tree_switch_current(char const *base, char const *tree) ;

/** Resolve API */
/** tree */
extern int tree_resolve_read_cdb(cdb *c, resolve_tree_t *tres) ;
extern int tree_resolve_write_cdb(cdbmaker *c, resolve_tree_t *tres) ;
extern void tree_resolve_sanitize(resolve_tree_t *tres) ;
extern void tree_resolve_modify_field(resolve_tree_t *tres, uint32_t field, char const *data) ;
extern int tree_resolve_get_field_tosa(stralloc *sa, resolve_tree_t *tres, resolve_tree_enum_table_t table) ;
extern void tree_service_add(char const *treename, char const *service, ssexec_t *info) ;
extern void tree_service_remove(char const *base, char const *treename, char const *service) ;
/** Master */
extern int tree_resolve_master_read_cdb(cdb *c, resolve_tree_master_t *mres) ;
extern int tree_resolve_master_write_cdb(cdbmaker *c, resolve_tree_master_t *mres) ;
extern int tree_resolve_master_create(char const *base, uid_t owner) ;
extern void tree_resolve_master_sanitize(resolve_tree_master_t *mres) ;
extern void tree_resolve_master_modify_field(resolve_tree_master_t *mres, uint32_t field, char const *data) ;
extern int tree_resolve_master_get_field_tosa(stralloc *sa, resolve_tree_master_t *mres, resolve_tree_enum_table_t table) ;

/** Seed API */
extern int tree_seed_file_isvalid(char const *seedpath, char const *treename) ;
extern void tree_seed_free(tree_seed_t *seed) ;
extern int tree_seed_get_group_permissions(tree_seed_t *seed) ;
extern ssize_t tree_seed_get_key(char *table,char const *str) ;
extern int tree_seed_isvalid(char const *seed) ;
extern int tree_seed_parse_file(tree_seed_t *seed, char const *seedpath) ;
extern int tree_seed_resolve_path(stralloc *sa, char const *seed) ;
extern int tree_seed_setseed(tree_seed_t *seed, char const *treename) ;

/** HASH API*/
extern int hash_add_tree(struct resolve_hash_tree_s **hash, char const *name, resolve_tree_t res) ;
extern struct resolve_hash_tree_s *hash_search_tree(struct resolve_hash_tree_s **hash, char const *name) ;
extern int hash_count_tree(struct resolve_hash_tree_s **hash) ;
extern void hash_free_tree(struct resolve_hash_tree_s **hash) ;

/** signal */
extern void tree_init_array(pidtree_t *apidt, tree_graph_t *g, uint8_t requiredby, uint8_t flag) ;
extern int tree_launch(pidtree_t *apidt, uint32_t ntree, unsigned int what, tain *deadline, ssexec_t *info) ;

#endif
