/* 
 * tree_copy_tmp.c
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
 
#include <66/tree.h>

#include <string.h>

#include <oblibs/string.h>
#include <oblibs/error2.h>
#include <oblibs/directory.h>
#include <oblibs/types.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/constants.h>
#include <66/enum.h>
#include <66/utils.h>
#include <66/ssexec.h>

void err(unsigned int *e, unsigned int msg,char const *resolve,char const *swap, char const *svdir)
{
	switch (msg)
	{
		case 0: strerr_warnwu1x("set revolve pointer to source") ;
				break ;
		case 1: strerr_warnwu1x("set revolve pointer to backup") ;
				break ;
		case 2: strerr_warnwu4sys("copy : ",svdir," to ", resolve) ;
				break ;
		case 3: strerr_warnwu2sys("remove directory: ", svdir) ;
				break ;
		case 4: strerr_warnwu1x("set revolve pointer to live") ;
				break ;
		default: break ;
	}
	*e = 0 ;
}

int tree_copy_tmp(char const *workdir, ssexec_t *info)
{
	stralloc saresolve = STRALLOC_ZERO ;
	stralloc swap = STRALLOC_ZERO ;
	unsigned int e = 1 ;
	int r ;
	size_t svdirlen ;
	size_t newlen ;
	
	char svdir[info->tree.len + SS_SVDIRS_LEN + SS_RESOLVE_LEN + 1] ;
	memcpy(svdir,info->tree.s,info->tree.len) ;
	memcpy(svdir + info->tree.len,SS_SVDIRS,SS_SVDIRS_LEN) ;
	svdirlen = info->tree.len + SS_SVDIRS_LEN ;
	memcpy(svdir + svdirlen,SS_SVC, SS_SVC_LEN) ;
	svdir[svdirlen + SS_SVC_LEN] = 0 ;
	
	size_t workdirlen = strlen(workdir) ; 
	char resolve[workdirlen + SS_RESOLVE_LEN + 1] ;
	memcpy(resolve,workdir,workdirlen) ;
	memcpy(resolve + workdirlen, SS_RESOLVE,SS_RESOLVE_LEN) ;
	resolve[workdirlen + SS_RESOLVE_LEN] = 0 ;
		
	/** svc */
	if (rm_rf(svdir) < 0)
	{
		if (!ss_resolve_pointo(&saresolve,info,CLASSIC,SS_RESOLVE_SRC))
		{
			err(&e,0,saresolve.s,swap.s,svdir) ;
			goto err ;
		}
		if (!ss_resolve_pointo(&swap,info,CLASSIC,SS_RESOLVE_BACK))
		{
			err(&e,1,saresolve.s,swap.s,svdir) ;
			goto err ;
		}
		if (!hiercopy(swap.s,saresolve.s))
		{
			err(&e,2,saresolve.s,swap.s,svdir) ;
			goto err ;
		}
		err(&e,3,saresolve.s,swap.s,svdir) ;
		goto err ;
	}
	
	/** db */
	memcpy(svdir + svdirlen,SS_DB, SS_DB_LEN) ;
	svdir[svdirlen + SS_DB_LEN] = 0 ;
	if (rm_rf(svdir) < 0)
	{
		if (!ss_resolve_pointo(&saresolve,info,LONGRUN,SS_RESOLVE_SRC))
		{
			err(&e,0,saresolve.s,swap.s,svdir) ;
			goto err ;
		}
		if (!ss_resolve_pointo(&swap,info,LONGRUN,SS_RESOLVE_BACK))
		{
			err(&e,1,saresolve.s,swap.s,svdir) ;
			goto err ;
		}
		if (!hiercopy(swap.s,saresolve.s))
		{
			err(&e,2,saresolve.s,swap.s,svdir) ;
			goto err ;
		}
		err(&e,3,saresolve.s,swap.s,svdir) ;
		goto err ;
	}	
	
	/** resolve */
	memcpy(svdir + svdirlen,SS_RESOLVE,SS_RESOLVE_LEN) ;
	svdir[svdirlen + SS_RESOLVE_LEN] = 0 ;
	
	if (rm_rf(svdir) < 0)
	{
		if (!ss_resolve_pointo(&saresolve,info,SS_NOTYPE,SS_RESOLVE_SRC))
		{
			err(&e,0,saresolve.s,swap.s,svdir) ;
			goto err ;
		}
		saresolve.len--;
		if (!stralloc_cats(&saresolve,SS_RESOLVE)) retstralloc(0,"tree_copy_tmp") ;
		if (!stralloc_0(&saresolve)) retstralloc(0,"tree_copy_tmp") ;
		
		if (!ss_resolve_pointo(&swap,info,SS_NOTYPE,SS_RESOLVE_BACK))
		{
			err(&e,1,saresolve.s,swap.s,svdir) ;
			goto err ;
		}
		swap.len--;
		if (!stralloc_cats(&swap,SS_RESOLVE)) retstralloc(0,"tree_copy_tmp") ;	
		if (!stralloc_0(&swap)) retstralloc(0,"tree_copy_tmp") ;
		if (!hiercopy(swap.s,saresolve.s))
		{
			err(&e,2,saresolve.s,swap.s,svdir) ;
			goto err ;
		}
		err(&e,3,saresolve.s,swap.s,svdir) ;
		goto err ;
	}	
	
	
	svdir[svdirlen] = 0 ;
		
	if (!hiercopy(workdir,svdir))
	{
		if (!ss_resolve_pointo(&saresolve,info,SS_NOTYPE,SS_RESOLVE_SRC))
		{	
			err(&e,0,saresolve.s,swap.s,svdir) ;
			goto err ;
		}
		if (!ss_resolve_pointo(&swap,info,SS_NOTYPE,SS_RESOLVE_BACK))
		{
			err(&e,1,saresolve.s,swap.s,svdir) ;
			goto err ;
		}
		if (!hiercopy(swap.s,saresolve.s))
		{
			err(&e,2,saresolve.s,swap.s,svdir) ;
			goto err ;
		}
		err(&e,2,saresolve.s,swap.s,svdir) ;
		goto err ;
	}
	
	/** resolve in live */
	if (!ss_resolve_pointo(&saresolve,info,SS_NOTYPE,SS_RESOLVE_LIVE))
	{	
		err(&e,0,saresolve.s,swap.s,resolve) ;
		goto err ;
	}
	newlen = saresolve.len ;
	saresolve.len--;
	if (!stralloc_cats(&saresolve,SS_RESOLVE)) goto err ;
	if (!stralloc_0(&saresolve)) goto err ;
	
	r = scan_mode(saresolve.s,S_IFDIR) ;
	if (r < 0) strerr_dief2x(111,resolve," conflicting format") ;
	if (!r)
	{
		saresolve.len = newlen ;
		if (!stralloc_0(&saresolve)) goto err ;
		VERBO2 strerr_warni3x("create directory: ",saresolve.s,SS_RESOLVE+1) ;
		r = dir_create_under(saresolve.s,SS_RESOLVE+1,0700) ;
		if (!r) strerr_diefu3sys(111,"create directory: ",saresolve.s,SS_RESOLVE+1) ;
	}
	saresolve.len = newlen ;
	if (!stralloc_cats(&saresolve,SS_RESOLVE)) goto err ;
	if (!stralloc_0(&saresolve)) goto err ;
	
	if (!ss_resolve_pointo(&swap,info,SS_NOTYPE,SS_RESOLVE_BACK))
	{
		err(&e,1,saresolve.s,swap.s,resolve) ;
		goto err ;
	}
	swap.len--;
	if (!stralloc_cats(&swap,SS_RESOLVE)) goto err ;
	if (!stralloc_0(&swap)) goto err ;
		
	if (rm_rf(saresolve.s) < 0)
	{
		if (!hiercopy(swap.s,saresolve.s))
		{
			err(&e,2,saresolve.s,swap.s,resolve) ;
			goto err ;
		}
		err(&e,2,saresolve.s,swap.s,resolve) ;
		goto err ;
	}
	
	if (!hiercopy(resolve,saresolve.s))
	{
		
		if (!hiercopy(swap.s,saresolve.s))
		{
			err(&e,2,saresolve.s,swap.s,resolve) ;
			goto err ;
		}
		err(&e,2,saresolve.s,swap.s,resolve) ;
		goto err ;
	}
	err:
		stralloc_free(&saresolve) ;
		stralloc_free(&swap) ;
		
	return e ; 
}
