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
 
#ifndef SS_ENVIRON_H
#define SS_ENVIRON_H

#include <sys/types.h>

#include <skalibs/stralloc.h>

extern int env_resolve_conf(stralloc *env,uid_t owner) ;
extern int env_merge_conf(char const *dst,char const *file,stralloc *srclist,stralloc *modifs,unsigned int force) ;

#endif
