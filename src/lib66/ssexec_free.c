/* 
 * ssexec_free.c
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
 
#include <stdlib.h>

#include <skalibs/stralloc.h>

#include <66/ssexec.h>

ssexec_t const ssexec_zero = SSEXEC_ZERO ; 

void ssexec_free(ssexec_t *info)
{
	stralloc_free(&info->base) ;
	stralloc_free(&info->live) ;
	stralloc_free(&info->tree) ;
	stralloc_free(&info->livetree) ;
	stralloc_free(&info->scandir) ;
	stralloc_free(&info->treename) ;
}
	

