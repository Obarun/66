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

extern int env_clean(stralloc *src) ;
extern int env_split_one(char *line,genalloc *ga,stralloc *sa) ;
extern int env_split(genalloc *gaenv,stralloc *saenv,stralloc *src) ;
extern int env_parsenclean(stralloc *modifs,stralloc *src) ;

#endif
