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

extern int version_compare(char const  *a, char const *b, uint8_t ndot) ;
extern int version_store(stack *stk, char const *str, uint8_t ndot) ;

#endif
