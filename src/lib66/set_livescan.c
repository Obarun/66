/* 
 * set_livescan.c
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
#include <skalibs/types.h>

#include <66/config.h>

int set_livescan(stralloc *live,uid_t owner)
{
	int r ;
	char ownerpack[256] ;
		
	r = set_livedir(live) ;
	if (r < 0) return -1 ;
	if (!r) return 0 ;
		
	size_t ownerlen = uid_fmt(ownerpack,owner) ;
	ownerpack[ownerlen] = 0 ;
	
	live->len--;
	if (!stralloc_cats(live,"scandir/")) retstralloc(0,"set_livedir") ;
	if (!stralloc_cats(live,ownerpack)) retstralloc(0,"set_livedir") ;
	if (!stralloc_0(live)) retstralloc(0,"set_livedir") ;
	
	return 1 ;
}
