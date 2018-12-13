/* 
 * tree_copy_tmp.c
 * 
 * Copyright (c) 2018 Eric Vidal <eric@obarun.org>
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

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/constants.h>
#include <66/enum.h>
#include <66/utils.h>


void err(unsigned int *e, unsigned int msg,char const *resolve,char const *swap, char const *svdir)
{
	switch (msg)
	{
		case 0: strerr_warnwu1x("set revolve pointer to source") ;
				break ;
		case 1: strerr_warnwu1x("set revolve pointer to backup") ;
				break ;
		case 2: strerr_warnwu4sys("to copy tree: ",resolve," to ", swap) ;
				break ;
		case 3: strerr_warnwu2sys("remove directory: ", svdir) ;
				break ;
		default: break ;
	}
	*e = 0 ;
}

int tree_copy_tmp(char const *workdir, char const *base, char const *live, char const *tree,char const *treename)
{
	stralloc saresolve = STRALLOC_ZERO ;
	stralloc swap = STRALLOC_ZERO ;
	unsigned int e = 1 ;
	size_t svdirlen ;
	size_t treelen = strlen(tree) ;	
	char svdir[treelen + SS_SVDIRS_LEN + SS_RESOLVE_LEN + 1] ;
	memcpy(svdir,tree,treelen) ;
	memcpy(svdir + treelen,SS_SVDIRS,SS_SVDIRS_LEN) ;
	svdirlen = treelen + SS_SVDIRS_LEN ;
	memcpy(svdir + svdirlen,SS_SVC, SS_SVC_LEN) ;
	svdir[svdirlen + SS_SVC_LEN] = 0 ;
	
	/** svc */
	if (rm_rf(svdir) < 0)
	{
		if (!resolve_pointo(&saresolve,base,live,tree,treename,CLASSIC,SS_RESOLVE_SRC))
		{
			err(&e,0,saresolve.s,swap.s,svdir) ;
			goto err ;
		}
		if (!resolve_pointo(&swap,base,live,tree,treename,CLASSIC,SS_RESOLVE_BACK))
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
		if (!resolve_pointo(&saresolve,base,live,tree,treename,LONGRUN,SS_RESOLVE_SRC))
		{
			err(&e,0,saresolve.s,swap.s,svdir) ;
			goto err ;
		}
		if (!resolve_pointo(&swap,base,live,tree,treename,LONGRUN,SS_RESOLVE_BACK))
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
		if (!resolve_pointo(&saresolve,base,live,tree,treename,0,SS_RESOLVE_SRC))
		{
			err(&e,0,saresolve.s,swap.s,svdir) ;
			goto err ;
		}
		saresolve.len--;
		if (!stralloc_cats(&saresolve,SS_RESOLVE)) retstralloc(0,"tree_copy_tmp") ;
		if (!stralloc_0(&saresolve)) retstralloc(0,"tree_copy_tmp") ;
		
		if (!resolve_pointo(&swap,base,live,tree,treename,0,SS_RESOLVE_BACK))
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
		if (!resolve_pointo(&saresolve,base,live,tree,treename,0,SS_RESOLVE_SRC))
		{	
			err(&e,0,saresolve.s,swap.s,svdir) ;
			goto err ;
		}
		if (!resolve_pointo(&swap,base,live,tree,treename,0,SS_RESOLVE_BACK))
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
	
	err:
		stralloc_free(&saresolve) ;
		stralloc_free(&swap) ;
		
	return e ; 
}
