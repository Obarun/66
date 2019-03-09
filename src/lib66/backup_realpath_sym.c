/* 
 * backup_realpath_sym.c
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
 
#include <66/utils.h>

#include <sys/stat.h>

#include <oblibs/error2.h>
#include <oblibs/types.h>

#include <skalibs/uint32.h>
#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/constants.h>
#include <66/enum.h>

int backup_realpath_sym(stralloc *sa, ssexec_t *info,unsigned int type)
{
	ssize_t r ;
	size_t typelen ;
	char *ptype = NULL ;
	
	if (type == CLASSIC)
	{
		ptype = SS_SYM_SVC ;
		typelen = SS_SYM_SVC_LEN;
	}
	else
	{
		ptype = SS_SYM_DB ;
		typelen = SS_SYM_DB_LEN;
	}
	
	char sym[info->tree.len + SS_SVDIRS_LEN + 1 + typelen + 1] ;
	memcpy(sym,info->tree.s,info->tree.len) ;
	memcpy(sym + info->tree.len, SS_SVDIRS, SS_SVDIRS_LEN) ;
	sym[info->tree.len + SS_SVDIRS_LEN] = '/' ;
	memcpy(sym + info->tree.len + SS_SVDIRS_LEN + 1, ptype,typelen) ;
	sym[info->tree.len + SS_SVDIRS_LEN + 1 + typelen] = '/' ;
	sym[info->tree.len + SS_SVDIRS_LEN + 1 + typelen + 1] = 0 ;
	
	r = scan_mode(sym,S_IFDIR) ;
	if(r <= 0) return 0 ; 
	sa->len = 0 ;
	r = sarealpath(sa,sym) ;
	if (r < 0 ) return 0 ; 
	if (!stralloc_0(sa)) retstralloc(0,"find_current") ;
	
	return 1 ;
}
