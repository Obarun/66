/* 
 * set_livedir.c
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
 
#include <66/utils.h>
 
#include <oblibs/error2.h>
#include <oblibs/directory.h>

#include <skalibs/stralloc.h>

#include <66/config.h>

int set_livedir(stralloc *live)
{
	int r ;
	
	if (live->len)
	{
		r = dir_scan_absopath(live->s) ;
		if(r < 0) return -1 ;
		if (live->s[live->len - 2] != '/')
		{
			live->len-- ;
			if (!stralloc_cats(live,"/")) retstralloc(0,"set_livedir") ;
			if (!stralloc_0(live)) retstralloc(0,"set_livedir") ;
		}
	}
	else
	{
		if (!stralloc_cats(live,SS_LIVE)) retstralloc(0,"set_livedir") ;
		if (!stralloc_0(live)) retstralloc(0,"set_livedir") ;
	}
	
	return 1 ;
}
