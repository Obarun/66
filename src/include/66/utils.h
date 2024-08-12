/*
 * utils.h
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#ifndef SS_UTILS_H
#define SS_UTILS_H

#include <sys/types.h>
#include <unistd.h> //getuid

#include <oblibs/log.h>
#include <oblibs/stack.h>

#include <skalibs/stralloc.h>

#include <66/ssexec.h>

#define MYUID getuid()
#define YOURUID(passto,owner) youruid(passto,owner)
#define MYGID getgid()
#define YOURGID(passto,owner) yourgid(passto,owner)

/** ss_utils.c file */
extern char const *get_userhome(uid_t myuid) ;
extern int youruid(uid_t *passto,char const *owner) ;
extern int yourgid(gid_t *passto,uid_t owner) ;
extern int set_livedir(stralloc *live) ;
extern int set_livescan(stralloc *live,uid_t owner) ;
extern int set_livetree(stralloc *live,uid_t owner) ;
extern int set_livestate(stralloc *live,uid_t owner) ;
extern int set_ownerhome(stralloc *base,uid_t owner) ;
extern int set_ownersysdir(stralloc *base,uid_t owner) ;
extern int set_environment(stralloc *env,uid_t owner) ;
extern int read_svfile(stralloc *sasv,char const *name,char const *src) ;

extern int sa_pointo(stralloc *sa, ssexec_t *info, int type, unsigned int where) ;
extern int create_live_state(ssexec_t *info, char const *treename) ;
extern int create_live_tree(ssexec_t *info) ;
extern void name_isvalid(char const *name) ;

extern int set_ownerhome_stack(char *store) ;
extern int set_ownersysdir_stack(char *base, uid_t owner) ;
extern int set_ownerhome_stack_byuid(char *store, uid_t owner) ;
extern void set_treeinfo(ssexec_t *info) ;
/**
 * if a < b return -1
 * if a > b return 1
 * if a == b return 0
 * return -2 on system call failure
*/
extern int version_compare(char const  *a, char const *b, uint8_t ndot) ;
extern int version_store(stack *stk, char const *str, uint8_t ndot) ;

/**
 * Identifier
*/

typedef int identifier_func_t(char *store, const char *data) ;

identifier_func_t identifier_replace_instance ;
identifier_func_t identifier_replace_username ;
identifier_func_t identifier_replace_useruid ;
identifier_func_t identifier_replace_usergid ;
identifier_func_t identifier_replace_usergroup ;
identifier_func_t identifier_replace_home ;
identifier_func_t identifier_replace_shell ;
identifier_func_t identifier_replace_runtime ;

typedef struct identifier_table_s identifier_table_t, *identifier_table_t_ref ;
struct identifier_table_s
{
    const char *ident ;
    identifier_func_t *func;
} ;

extern identifier_table_t identifier_table[] ;
extern int identifier_replace(stralloc *sasv, char const *svname) ;

#endif
