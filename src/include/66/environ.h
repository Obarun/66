/* 
 * environ.h
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
 
#ifndef SS_ENVIRON_H
#define SS_ENVIRON_H

#include <sys/types.h>

#include <skalibs/stralloc.h>

#include <66/parser.h>

extern int env_resolve_conf(stralloc *env,char const *svname,uid_t owner) ;
extern int env_merge_conf(stralloc *result,stralloc *srclist,stralloc *modifs,uint8_t conf) ;
extern int env_clean_with_comment(stralloc *sa) ;
extern int env_compute(stralloc *result,sv_alltype *sv, uint8_t conf) ;
extern int env_find_current_version(stralloc *sa,char const *svconf) ;

#endif
