/* 
 * state.h
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
 
#ifndef SS_STATE_H
#define SS_STATE_H

#include <stddef.h>

#include <skalibs/uint32.h>
#include <skalibs/uint64.h>

#define SS_FLAGS_FALSE 0
#define SS_FLAGS_TRUE 1
#define SS_FLAGS_UNKNOWN 2
#define SS_FLAGS_RELOAD 0
#define SS_FLAGS_INIT 1
#define SS_FLAGS_UNSUPERVISE 2
#define SS_FLAGS_STATE 3
#define SS_FLAGS_PID 4

typedef struct ss_state_s ss_state_t, *ss_state_t_ref ;
struct ss_state_s
{
	uint32_t reload ;
	uint32_t init ;
	uint32_t unsupervise ;
	uint32_t state ;//0 down,1 up, 2 earlier
	uint64_t pid ;
} ;
#define STATE_ZERO { 0,0,0,0,0 }
#define SS_STATE_SIZE 24
extern ss_state_t const ss_state_zero ;

extern void ss_state_rmfile(char const *src,char const *name) ;
extern void ss_state_pack(char *pack,ss_state_t *sta) ;
extern void ss_state_unpack(char *pack,ss_state_t *sta) ;
extern int ss_state_write(ss_state_t *sta,char const *dst,char const *name) ;
extern int ss_state_read(ss_state_t *sta,char const *src,char const *name) ;
extern int ss_state_check(char const *src, char const *name) ;
extern void ss_state_setflag(ss_state_t *sta,int flags,int flags_val) ;

#endif
