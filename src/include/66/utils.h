/*
 * utils.h
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

#ifndef SS_UTILS_H
#define SS_UTILS_H

#include <sys/types.h>
#include <unistd.h> //getuid

#include <oblibs/log.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>

#include <66/ssexec.h>

#define MYUID getuid()
#define YOURUID(passto,owner) youruid(passto,owner)
#define MYGID getgid()
#define YOURGID(passto,owner) yourgid(passto,owner)

typedef enum visit_e visit_t ;
enum visit_e
{
    VISIT_WHITE = 0,
    VISIT_GRAY,
    VISIT_BLACK
} ;

extern void visit_init(visit_t *visit, size_t len) ;

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
extern int read_svfile(stralloc *sasv,char const *name,char const *src) ;

extern int sa_pointo(stralloc *sa, ssexec_t *info, int type, unsigned int where) ;
extern int create_live_state(ssexec_t *info, char const *treename) ;
extern int create_live_tree(ssexec_t *info) ;
extern void name_isvalid(char const *name) ;

extern int set_ownerhome_stack(char *store) ;
extern int set_ownersysdir_stack(char *base, uid_t owner) ;

#endif
