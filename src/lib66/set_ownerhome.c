/* 
 * set_ownerhome.c
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
 
#include <sys/types.h>
#include <pwd.h>
#include <errno.h>

#include <oblibs/error2.h>

#include <skalibs/stralloc.h>

#include <66/config.h>

int set_ownerhome(stralloc *base,uid_t owner)
{
	char const *user_home = NULL ;
	int e = errno ;
	struct passwd *st = getpwuid(owner) ;
	
	if (!st)
	{
		if (!errno) errno = ESRCH ;
		return 0 ;
	}
	user_home = st->pw_dir ;
	errno = e ;
	if (!user_home) return 0 ;
	
	if (!stralloc_cats(base,user_home))	retstralloc(0,"set_ownerhome") ;
	if (!stralloc_catb(base,"/",1))	retstralloc(0,"set_ownerhome") ;
	if (!stralloc_0(base)) retstralloc(0,"set_ownerhome") ;
		
	return 1 ;
}
