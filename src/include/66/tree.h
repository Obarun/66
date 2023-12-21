/*
 * tree.h
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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

#include <skalibs/stralloc.h>
#include <skalibs/cdb.h>
#include <skalibs/cdbmake.h>

#include <66/ssexec.h>
#include <66/resolve.h>

#define TREE_GROUPS_BOOT "boot"
#define TREE_GROUPS_BOOT_LEN (sizeof TREE_GROUPS_BOOT - 1)
#define TREE_GROUPS_ADM "admin"
#define TREE_GROUPS_ADM_LEN (sizeof TREE_GROUPS_ADM - 1)
#define TREE_GROUPS_USER "user"
#define TREE_GROUPS_USER_LEN (sizeof TREE_GROUPS_USER - 1)

typedef struct resolve_tree_s resolve_tree_t, *resolve_tree_t_ref ;
struct resolve_tree_s
{
    uint32_t salen ;
    stralloc sa ;

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
#define RESOLVE_TREE_ZERO { 0,STRALLOC_ZERO,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }

typedef enum resolve_tree_enum_e resolve_tree_enum_t, *resolve_tree_enum_t_ref;
enum resolve_tree_enum_e
{
    E_RESOLVE_TREE_NAME = 0,
    E_RESOLVE_TREE_ENABLED,
    E_RESOLVE_TREE_DEPENDS,
    E_RESOLVE_TREE_REQUIREDBY,
    E_RESOLVE_TREE_ALLOW,
    E_RESOLVE_TREE_GROUPS,
    E_RESOLVE_TREE_CONTENTS,
    E_RESOLVE_TREE_NDEPENDS,
    E_RESOLVE_TREE_NREQUIREDBY,
    E_RESOLVE_TREE_NALLOW,
    E_RESOLVE_TREE_NGROUPS,
    E_RESOLVE_TREE_NCONTENTS,
    E_RESOLVE_TREE_INIT,
    E_RESOLVE_TREE_SUPERVISED,
    E_RESOLVE_TREE_ENDOFKEY
} ;

typedef struct resolve_tree_master_s resolve_tree_master_t, *resolve_tree_master_t_ref ;
struct resolve_tree_master_s
{
    uint32_t salen ;
    stralloc sa ;

    uint32_t name ;
    uint32_t allow ;
    uint32_t current ;
    uint32_t contents ;

    uint32_t nallow ;
    uint32_t ncontents ;

} ;
#define RESOLVE_TREE_MASTER_ZERO { 0,STRALLOC_ZERO,0,0,0,0,0,0 }

typedef enum resolve_tree_master_enum_e resolve_tree_master_enum_t, *resolve_tree_master_enum_t_ref;
enum resolve_tree_master_enum_e
{
    E_RESOLVE_TREE_MASTER_NAME = 0,
    E_RESOLVE_TREE_MASTER_ALLOW,
    E_RESOLVE_TREE_MASTER_CURRENT,
    E_RESOLVE_TREE_MASTER_CONTENTS,
    E_RESOLVE_TREE_MASTER_NALLOW,
    E_RESOLVE_TREE_MASTER_NCONTENTS,
    E_RESOLVE_TREE_MASTER_ENDOFKEY
} ;

extern resolve_field_table_t resolve_tree_field_table[] ;
extern resolve_field_table_t resolve_tree_master_field_table[] ;

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


/**
 *
 * Resolve API
 *
 * */

/** tree */
extern int tree_resolve_read_cdb(cdb *c, resolve_tree_t *tres) ;
extern int tree_resolve_write_cdb(cdbmaker *c, resolve_tree_t *tres) ;
extern int tree_resolve_copy(resolve_tree_t *dst, resolve_tree_t *tres) ;
extern int tree_resolve_modify_field(resolve_tree_t *tres, uint8_t field, char const *data) ;
extern int tree_resolve_get_field_tosa(stralloc *sa, resolve_tree_t *tres, resolve_tree_enum_t field) ;
extern int tree_resolve_array_search(resolve_tree_t *ares, unsigned int areslen, char const *name) ;
extern void tree_service_add(char const *treename, char const *service, ssexec_t *info) ;
extern void tree_service_remove(char const *base, char const *treename, char const *service) ;
/** Master */
extern int tree_resolve_master_read_cdb(cdb *c, resolve_tree_master_t *mres) ;
extern int tree_resolve_master_write_cdb(cdbmaker *c, resolve_tree_master_t *mres) ;
extern int tree_resolve_master_create(char const *base, uid_t owner) ;
extern int tree_resolve_master_copy(resolve_tree_master_t *dst, resolve_tree_master_t *mres) ;
extern int tree_resolve_master_modify_field(resolve_tree_master_t *mres, uint8_t field, char const *data) ;
extern int tree_resolve_master_get_field_tosa(stralloc *sa, resolve_tree_master_t *mres, resolve_tree_master_enum_t field) ;

/**
 *
 * Seed API
 *
 * */
extern int tree_seed_file_isvalid(char const *seedpath, char const *treename) ;
extern void tree_seed_free(tree_seed_t *seed) ;
extern int tree_seed_get_group_permissions(tree_seed_t *seed) ;
extern ssize_t tree_seed_get_key(char *table,char const *str) ;
extern int tree_seed_isvalid(char const *seed) ;
extern int tree_seed_parse_file(tree_seed_t *seed, char const *seedpath) ;
extern int tree_seed_resolve_path(stralloc *sa, char const *seed) ;
extern int tree_seed_setseed(tree_seed_t *seed, char const *treename) ;


#endif
