/* 
 * set_livetree.c
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

#include <stddef.h>

#include <oblibs/log.h>

#include <skalibs/stralloc.h>
#include <skalibs/types.h>

#include <66/constants.h>

int set_livetree(stralloc *livetree,uid_t owner)
{
	int r ;
	char ownerpack[UID_FMT] ;

	r = set_livedir(livetree) ;
	if (r < 0) return -1 ;
	if (!r) return 0 ;
		
	size_t ownerlen = uid_fmt(ownerpack,owner) ;
	ownerpack[ownerlen] = 0 ;
	
	if (!stralloc_cats(livetree,SS_TREE "/")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	if (!stralloc_cats(livetree,ownerpack)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	if (!stralloc_0(livetree)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	livetree->len--;
	return 1 ;
}
