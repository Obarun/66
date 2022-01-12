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
#include <66/resolve.h>


#define DATA_TREE 1
#define DATA_TREE_MASTER 2
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
    uint32_t disen ;//disable->0, enable->1
} ;
#define RESOLVE_TREE_ZERO { 0,STRALLOC_ZERO,0,0,0,0,0,0,0,0,0,0,0,0,0 }

typedef enum resolve_tree_enum_e resolve_tree_enum_t, *resolve_tree_enum_t_ref;
enum resolve_tree_enum_e
{
    TREE_ENUM_NAME = 0,
    TREE_ENUM_DEPENDS,
    TREE_ENUM_REQUIREDBY,
    TREE_ENUM_ALLOW,
    TREE_ENUM_GROUPS,
    TREE_ENUM_CONTENTS,
    TREE_ENUM_NDEPENDS,
    TREE_ENUM_NREQUIREDBY,
    TREE_ENUM_NALLOW,
    TREE_ENUM_NGROUPS,
    TREE_ENUM_NCONTENTS,
    TREE_ENUM_INIT,
    TREE_ENUM_DISEN,
    TREE_ENUM_ENDOFKEY
} ;

typedef struct resolve_tree_field_table_s resolve_tree_field_table_t, *resolve_tree_field_table_t_ref ;
struct resolve_tree_field_table_s
{
    char *field ;
} ;

extern resolve_tree_field_table_t resolve_tree_field_table[] ;

typedef struct resolve_tree_master_s resolve_tree_master_t, *resolve_tree_master_t_ref ;
struct resolve_tree_master_s
{
   uint32_t salen ;
   stralloc sa ;

   uint32_t name ;
   uint32_t allow ;
   uint32_t enabled ;
   uint32_t current ;

   uint32_t nenabled ;

} ;
#define RESOLVE_TREE_MASTER_ZERO { 0,STRALLOC_ZERO,0,0,0,0,0 }

typedef enum resolve_tree_master_enum_e resolve_tree_master_enum_t, *resolve_tree_master_enum_t_ref;
enum resolve_tree_master_enum_e
{
    TREE_ENUM_MASTER_NAME = 0,
    TREE_ENUM_MASTER_ALLOW,
    TREE_ENUM_MASTER_ENABLED,
    TREE_ENUM_MASTER_CURRENT,
    TREE_ENUM_MASTER_NENABLED,
    TREE_ENUM_MASTER_ENDOFKEY
} ;

extern resolve_tree_field_table_t resolve_tree_master_field_table[] ;

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
extern int tree_find_current(stralloc *tree, char const *base, uid_t owner) ;

/** @Return 1 on success
 * @Return 0 if not valid
 * @Return -1 on system error */
extern int tree_iscurrent(char const *base, char const *treename) ;

/** @Return 1 on success
 * @Return 0 on fail */
extern int tree_isinitialized(char const *live, char const *treename, uid_t owner) ;

/** @Return 1 on success
 * @Return 0 if not valid
 * @Return -1 on system error */
extern int tree_isenabled(char const *base, char const *treename) ;

/** @Return 1 on success
 * @Return 0 if not valid
 * @Return -1 on system error */
extern int tree_ongroups(char const *base, char const *treename, char const *group) ;

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
extern int tree_find_current(stralloc *tree, char const *base, uid_t owner) ;

extern int tree_get_permissions(char const *tree, uid_t owner) ;

extern int tree_sethome(ssexec_t *info) ;

extern char tree_setname(stralloc *sa, char const *tree) ;

extern int tree_switch_current(char const *base, char const *tree) ;



/**
 *
 * Resolve API
 *
 * */

/** tree */
extern int tree_read_cdb(cdb *c, resolve_tree_t *tres) ;
extern int tree_write_cdb(cdbmaker *c, resolve_tree_t *tres) ;
extern int tree_resolve_copy(resolve_tree_t *dst, resolve_tree_t *tres) ;
extern int tree_resolve_modify_field(resolve_tree_t *tres, uint8_t field, char const *data) ;
extern int tree_resolve_field_tosa(stralloc *sa, resolve_tree_t *tres, resolve_tree_enum_t field) ;

/** Master */
extern int tree_read_master_cdb(cdb *c, resolve_tree_master_t *mres) ;
extern int tree_write_master_cdb(cdbmaker *c, resolve_tree_master_t *mres) ;
extern int tree_resolve_master_create(char const *base, uid_t owner) ;
extern int tree_resolve_master_copy(resolve_tree_master_t *dst, resolve_tree_master_t *mres) ;
extern int tree_resolve_master_modify_field(resolve_tree_master_t *mres, uint8_t field, char const *data) ;
extern int tree_resolve_master_field_tosa(stralloc *sa, resolve_tree_master_t *mres, resolve_tree_master_enum_t field) ;

/**
 *
 * Seed API
 *
 * */

extern void tree_seed_free(tree_seed_t *seed) ;
extern int tree_seed_setseed(tree_seed_t *seed, char const *treename, uint8_t check_contents) ;
extern int tree_seed_isvalid(char const *seed) ;

#endif
