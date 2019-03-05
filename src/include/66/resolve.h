/* 
 * resolve.h
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
 
#ifndef SSRESOLVE_H
#define SSRESOLVE_H

#include <stddef.h>

#include <skalibs/stralloc.h>
#include <skalibs/types.h>
#include <skalibs/uint32.h>
#include <66/ssexec.h>
#include <66/parser.h>

#define SS_RESOLVE "/.resolve"
#define SS_RESOLVE_LEN (sizeof SS_RESOLVE - 1)
#define SS_RESOLVE_LIVE 0
#define SS_RESOLVE_SRC 1
#define SS_RESOLVE_BACK 2
#define SS_NOTYPE 0

typedef struct ss_resolve_s ss_resolve_t, *ss_resolve_t_ref ;
struct ss_resolve_s
{
	uint32_t salen ;
	stralloc sa ;
	
	uint32_t name ;
	uint32_t description ;
	uint32_t logger ;
	uint32_t logreal ;
	uint32_t logassoc ;
	uint32_t dstlog ;
	uint32_t deps ;
	uint32_t src ;
	uint32_t live ;
	uint32_t runat ; //livetree->longrun,scandir->svc
	uint32_t tree ;
	uint32_t treename ;
	uint32_t exec_run ;
	uint32_t exec_finish ;
	
	uint32_t type ;
	uint32_t ndeps ;
	uint32_t reload ;
	uint32_t disen ;//disable->0,enable->1
	uint32_t init ;
	uint32_t unsupervise ;
	uint32_t down ;
	uint32_t run ;
	uint64_t pid ;
} ;
#define RESOLVE_ZERO { 0,STRALLOC_ZERO,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }

extern ss_resolve_t const ss_resolve_zero ;

extern int ss_resolve_pointo(stralloc *sa,ssexec_t *info,unsigned int type, unsigned int where) ;
extern int ss_resolve_src(genalloc *ga, stralloc *sasrc, char const *name, char const *src,unsigned int *found) ;
extern void ss_resolve_init(ss_resolve_t *res) ;
extern int ss_resolve_add_uint32(stralloc *sa, uint32_t data) ;
extern int ss_resolve_add_uint64(stralloc *sa, uint64_t data) ;
extern uint32_t ss_resolve_add_string(ss_resolve_t *res,char const *data) ;
extern int ss_resolve_pack(stralloc *sa,ss_resolve_t *res) ;
extern int ss_resolve_write(ss_resolve_t *res,char const *dst,char const *name) ;
extern int ss_resolve_read(ss_resolve_t *res,char const *src,char const *name) ;
extern void ss_resolve_free(ss_resolve_t *res) ;
extern int ss_resolve_check(ssexec_t *info, char const *name,unsigned int where) ;
extern int ss_resolve_setnwrite(ss_resolve_t *res, sv_alltype *services,ssexec_t *info,char const *dst) ;
extern int ss_resolve_setlognwrite(ss_resolve_t *sv, char const *dst) ;
extern int ss_resolve_rmfile(ss_resolve_t *res, char const *src,char const *name) ;
extern int ss_resolve_addlogger(ssexec_t *info,genalloc *ga) ;
#endif
