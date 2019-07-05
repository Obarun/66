/* 
 * set_livescan.c
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
 
#include <oblibs/error2.h>
#include <oblibs/directory.h>

#include <skalibs/stralloc.h>
#include <skalibs/types.h>

#include <66/config.h>
#include <66/constants.h>

int set_livescan(stralloc *scandir,uid_t owner)
{
	int r ;
	char ownerpack[UID_FMT] ;
		
	r = set_livedir(scandir) ;
	if (r < 0) return -1 ;
	if (!r) return 0 ;
		
	size_t ownerlen = uid_fmt(ownerpack,owner) ;
	ownerpack[ownerlen] = 0 ;
	
	if (!stralloc_cats(scandir,SS_SCANDIR "/")) retstralloc(0,"set_livescan") ;
	if (!stralloc_cats(scandir,ownerpack)) retstralloc(0,"set_livescan") ;
	if (!stralloc_0(scandir)) retstralloc(0,"set_livescan") ;
	scandir->len--;
	return 1 ;
}
