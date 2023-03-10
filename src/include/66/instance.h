/*
 * instance.h
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

#ifndef SS_INSTANCE_H
#define SS_INSTANCE_H

#include <sys/types.h>
#include <unistd.h> //getuid

#include <oblibs/log.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>

#include <66/ssexec.h>
#include <66/service.h>

/** ss_instance.c file */
extern int instance_check(char const *svname) ;
extern int instance_splitname(stralloc *sa,char const *name,int len,int what) ;
extern int instance_create(stralloc *sasv,char const *svname, char const *regex, int len) ;
extern void instance_splitname_to_char(char *store, char const *name, int len, int what) ;

#endif
