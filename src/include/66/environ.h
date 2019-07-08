/* 
 * environ.h
 * 
 * Copyright (c) 2018-2019 Eric Vidal <eric@obarun.org>
 * 
 * All rights reserved.
 * 
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */
 
#ifndef ENVIRON_H
#define ENVIRON_H

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>

#define MAXVAR  50 
#define MAXFILE 100 
#define MAXENV 4095 

typedef struct exlsn_s exlsn_t, *exlsn_t_ref ;
struct exlsn_s
{
  stralloc vars ;
  stralloc values ;
  genalloc data ; // array of elsubst
  stralloc modifs ;
} ;

#define EXLSN_ZERO { .vars = STRALLOC_ZERO, .values = STRALLOC_ZERO, .data = GENALLOC_ZERO, .modifs = STRALLOC_ZERO }


extern int env_clean(stralloc *src) ;
extern int env_split_one(char *line,genalloc *ga,stralloc *sa) ;
extern int env_split(genalloc *gaenv,stralloc *saenv,stralloc *src) ;
extern int env_parsenclean(stralloc *modifs,stralloc *src) ;
extern int make_env_from_line(char const **v,stralloc *sa) ;
extern int env_substitute(char const *key, char const *val,exlsn_t *info, char const *const *envp,int unexport) ;
extern int env_addkv (const char *key, const char *val, exlsn_t *info) ;
extern size_t build_env(char const *src,char const *const *envp,char const **newenv, char *tmpenv) ;
extern int env_get_from_src(stralloc *modifs,char const *src) ;
extern int env_resolve_conf(stralloc *env,uid_t owner) ;
extern int env_merge_conf(char const *dst,char const *file,stralloc *srclist,stralloc *modifs,unsigned int force) ;
#endif
