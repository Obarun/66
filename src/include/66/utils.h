/* 
 * utils.h
 * 
 * Copyright (c) 2018 Eric Vidal <eric@obarun.org>
 * 
 * All rights reserved.
 * 
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */
 
#ifndef UTILS_H
#define UTILS_H

#include <sys/types.h>
#include <unistd.h> //getuid

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/tai.h>

#include <s6/ftrigr.h>

extern unsigned int VERBOSITY ;

#define VERBO1 if(VERBOSITY >= 1) 
#define VERBO2 if(VERBOSITY >= 2) 
#define VERBO3 if(VERBOSITY >= 3) 

#define MYUID getuid()
#define YOURUID(passto,owner) youruid(passto,owner)
#define MYGID getgid()
#define YOURGID(passto,owner) yourgid(passto,owner)

typedef struct svstat_s svstat_t, *svstat_t_ref ;
struct svstat_s
{
	int type ;
	char const *name ;
	size_t namelen ;
	int down ;
	int reload ;
	int init ;
	int unsupervise ;
	int remove ;
} ;
#define SVSTAT_ZERO { .type = 0, .name = 0, .namelen = 0, .down = 0, .init = 0, .reload = 0, .unsupervise = 0, .remove = 0 }
/** signal */
#define ALARM 0


extern int dir_cmpndel(char const *src, char const *dst,char const *exclude) ;

/** get_uidgid.c */
extern int youruid(uid_t *passto,char const *owner) ;
extern int yourgid(gid_t *passto,uid_t owner) ;

extern char const *get_userhome(uid_t myuid) ;

extern int set_ownerhome(stralloc *base,uid_t owner) ;

extern int set_ownersysdir(stralloc *base,uid_t owner) ;

extern int scandir_ok (char const *dir) ;

extern int scandir_send_signal(char const *scandir,char const *signal) ;

extern int set_livedir(stralloc *live) ;

extern int set_livescan(stralloc *live,uid_t owner) ;

extern int set_livetree(stralloc *live,uid_t owner) ;

extern int resolve_write(char const *dst,char const *svname, char const *file, char const *contents,unsigned int force) ;

extern int resolve_read(stralloc *sa, char const *src, char const *svname,char const *file) ;

extern int resolve_remove(char const *dst,char const *svname,char const *file) ;

extern int resolve_remove_service(char const *dst, char const *svname) ;

extern int resolve_symlive(char const *live, char const *tree, char const *treename) ;

extern int resolve_pointo(stralloc *sa,char const *base, char const *live,char const *tree,char const *treename,unsigned int type, unsigned int what) ;

extern int resolve_src(genalloc *ga, stralloc *sasrc, char const *name, char const *src,unsigned int *found) ;

extern int insta_check(char const *svname) ;

extern int insta_create(stralloc *sasv,stralloc *sv, char const *src, int len) ;

extern int insta_splitname(stralloc *sa,char const *name,int len,int what) ;

extern int insta_replace(stralloc *sa,char const *src,char const *cpy) ;

extern int find_logger(genalloc *ga, char const *name, char const *src) ;

#endif
