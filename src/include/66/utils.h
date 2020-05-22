/* 
 * utils.h
 * 
 * Copyright (c) 2018-2020 Eric Vidal <eric@obarun.org>
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
#include <66/resolve.h>

#define MYUID getuid()
#define YOURUID(passto,owner) youruid(passto,owner)
#define MYGID getgid()
#define YOURGID(passto,owner) yourgid(passto,owner)

/** ss_utils.c file */
extern int scandir_ok (char const *dir) ;
extern int scandir_send_signal(char const *scandir,char const *signal) ;
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
extern int module_in_cmdline(genalloc *gares, ss_resolve_t *res, char const *dir) ;
extern int module_search_service(char const *src, genalloc *gares, char const *name,uint8_t *found, char module_name[256]) ;
/** ss_instance.c file */
extern int instance_check(char const *svname) ;
extern int instance_splitname(stralloc *sa,char const *name,int len,int what) ;
extern int instance_create(stralloc *sasv,char const *svname, char const *regex, int len) ;

#endif
