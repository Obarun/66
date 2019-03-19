/* 
 * rc.h
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
 
#ifndef RC_H
#define RC_H

#include <66/ssexec.h>

#include <skalibs/genalloc.h>

extern int rc_init(ssexec_t *info, char const *const *envp) ;
extern int rc_send(ssexec_t *info,genalloc *ga,char const *sig,char const *const *envp) ;
extern int rc_unsupervise(ssexec_t *info, genalloc *ga,char const *sig,char const *const *envp) ;

#endif
