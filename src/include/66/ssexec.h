/* 
 * ssexec.h
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
 
#ifndef SSEXEC_H
#define SSEXEC_H

#include <skalibs/stralloc.h>
#include <skalibs/types.h>

typedef struct ssexec_s ssexec_t , *ssexec_t_ref ;
struct ssexec_s
{
	stralloc base ;
	stralloc live ;
	stralloc tree ;
	stralloc livetree ;
	stralloc scandir ;
	stralloc treename ;
	int treeallow ; //1 yes , 0 no
	uid_t owner ;
	unsigned int timeout ;
	char const *prog ;
	char const *help ;
	char const *usage ;
} ;

#define SSEXEC_ZERO { 	.base = STRALLOC_ZERO , \
						.live = STRALLOC_ZERO , \
						.tree = STRALLOC_ZERO , \
						.livetree = STRALLOC_ZERO , \
						.scandir = STRALLOC_ZERO , \
						.treename = STRALLOC_ZERO , \
						.treeallow = 0 , \
						.owner = 0 , \
						.timeout = 0 , \
						.prog = 0 , \
						.help = 0 , \
						.usage = 0 }

typedef int ssexec_func_t(int, char const *const *argv, char const *const *envp, ssexec_t *info) ;
typedef ssexec_func_t *ssexec_func_t_ref ;

extern void ssexec_free(ssexec_t *info) ;
extern ssexec_t const ssexec_zero ;
extern int set_ssinfo(ssexec_t *info) ;

extern ssexec_func_t ssexec_init ;
extern ssexec_func_t ssexec_enable ;
extern ssexec_func_t ssexec_disable ;
extern ssexec_func_t ssexec_start ;
extern ssexec_func_t ssexec_stop ;
extern ssexec_func_t ssexec_svctl ;
extern ssexec_func_t ssexec_dbctl ;

extern char const *usage_enable ;
extern char const *help_enable ;
extern char const *usage_disable ;
extern char const *help_disable ;
extern char const *usage_dbctl ;
extern char const *help_dbctl ;
extern char const *usage_svctl ;
extern char const *help_svctl ;
extern char const *usage_start ;
extern char const *help_start ;
extern char const *usage_stop ;
extern char const *help_stop ;
extern char const *usage_init ;
extern char const *help_init ;

extern int ssexec_main(int argc, char const *const *argv, char const *const *envp,ssexec_func_t *func,ssexec_t *info) ;

#endif
