/* 
 * state.c
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

#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>//realpath
#include <stdio.h>

#include <oblibs/types.h>

#include <skalibs/posixplz.h>
#include <skalibs/uint32.h>
#include <skalibs/uint64.h>
#include <skalibs/djbunix.h>

#include <66/state.h>

void ss_state_rmfile(char const *src,char const *name)
{
	size_t srclen = strlen(src) ;
	size_t namelen = strlen(name) ;

	char tmp[srclen + 1 + namelen + 1] ;
	memcpy(tmp,src,srclen) ;
	tmp[srclen] = '/' ;
	memcpy(tmp + srclen + 1, name, namelen) ;
	tmp[srclen + 1 + namelen] = 0 ;

	unlink_void(tmp) ;
}

void ss_state_pack(char *pack, ss_state_t *sta)
{
	uint32_pack_big(pack,sta->reload) ;
	uint32_pack_big(pack+4,sta->init) ;
	uint32_pack_big(pack+8,sta->unsupervise) ;
	uint32_pack_big(pack+12,sta->state) ;
	uint64_pack_big(pack+16,sta->pid) ;
}

void ss_state_unpack(char *pack,ss_state_t *sta)
{
	uint32_t reload ;
	uint32_t init ;
	uint32_t unsupervise ;
	uint32_t state ;
	uint64_t pid ;
	
	uint32_unpack_big(pack,&reload) ;
	sta->reload = reload ;
	uint32_unpack_big(pack+4,&init) ;
	sta->init = init ;
	uint32_unpack_big(pack+8,&unsupervise) ;
	sta->unsupervise = unsupervise ;
	uint32_unpack_big(pack+12,&state) ;
	sta->state = state ;
	uint64_unpack_big(pack+16,&pid) ;
	sta->pid = pid ;
}

int ss_state_write(ss_state_t *sta, char const *dst, char const *name)
{
	char pack[SS_STATE_SIZE] ;
	size_t dstlen = strlen(dst) ;
	size_t namelen = strlen(name) ;
	
	char tmp[dstlen + 1 + namelen + 1] ;
	memcpy(tmp,dst,dstlen) ;
	tmp[dstlen] = '/' ;
	memcpy(tmp + dstlen + 1, name, namelen) ;
	tmp[dstlen + 1 + namelen] = 0 ;
	
	ss_state_pack(pack,sta) ;
	if (!openwritenclose_unsafe(tmp,pack,SS_STATE_SIZE)) return 0 ;

	return 1 ;
}

int ss_state_read(ss_state_t *sta, char const *src, char const *name)
{
	char pack[SS_STATE_SIZE] ;
	size_t srclen = strlen(src) ;
	size_t namelen = strlen(name) ;
	
	char tmp[srclen + 1 + namelen + 1] ;
	memcpy(tmp,src,srclen) ;
	tmp[srclen] = '/' ;
	memcpy(tmp + srclen + 1, name, namelen) ;
	tmp[srclen + 1 + namelen] = 0 ;

	if (openreadnclose(tmp, pack, SS_STATE_SIZE) < SS_STATE_SIZE) return 0 ;
	ss_state_unpack(pack, sta) ;

	return 1 ;	
}

int ss_state_check(char const *src, char const *name)
{
	int r ;
	size_t srclen = strlen(src) ;
	size_t namelen = strlen(name) ;
	char tmp[srclen + 1 + namelen + 1] ;
	memcpy(tmp,src,srclen) ;
	tmp[srclen] = '/' ;
	memcpy(tmp + srclen + 1, name, namelen) ;
	tmp[srclen + 1 + namelen] = 0 ;
	r = scan_mode(tmp,S_IFREG) ;
	if (!r || r < 0) return 0 ;
	else return 1 ;
}

void ss_state_setflag(ss_state_t *sta,int flags,int flags_val)
{
	switch (flags)
	{
		case SS_FLAGS_RELOAD: sta->reload = flags_val ; break ;
		case SS_FLAGS_INIT: sta->init = flags_val ; break ;
		case SS_FLAGS_UNSUPERVISE: sta->unsupervise = flags_val ; break ;
		case SS_FLAGS_STATE: sta->state = flags_val ; break ;
		case SS_FLAGS_PID: sta->pid = (uint32_t)flags_val ; break ;
		default: return ;
	}
}
