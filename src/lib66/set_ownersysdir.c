/* 
 * set_ownersysdir.c
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
 
#include <sys/types.h>
#include <pwd.h>
#include <errno.h>

#include <oblibs/log.h>

#include <skalibs/stralloc.h>

#include <66/config.h>

int set_ownersysdir(stralloc *base, uid_t owner)
{
	char const *user_home = NULL ;
	int e = errno ;
	struct passwd *st = getpwuid(owner) ;
	errno = 0 ;
	if (!st)
	{
		if (!errno) errno = ESRCH ;
		return 0 ;
	}
	user_home = st->pw_dir ;
	errno = e ;
	if (user_home == NULL) return 0 ;
	
	if(owner > 0){
		if (!stralloc_cats(base,user_home))	log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		if (!stralloc_cats(base,"/")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		if (!stralloc_cats(base,SS_USER_DIR)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		if (!stralloc_0(base)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	}
	else
	{
		if (!stralloc_cats(base,SS_SYSTEM_DIR)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		if (!stralloc_0(base)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	}
	base->len--;
	return 1 ;
}
