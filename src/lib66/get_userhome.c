/* 
 * get_userhome.c
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

char const *get_userhome(uid_t myuid)
{
	char const *user_home = NULL ;
	struct passwd *st = getpwuid(myuid) ;
	
	if (!st)
	{
		if (!errno) errno = ESRCH ;
		return 0 ;
	}
	user_home = st->pw_dir ;

	if (!user_home) return 0 ;

	return user_home ;
}
