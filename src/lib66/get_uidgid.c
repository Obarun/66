/* 
 * get_uidgid.c
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

#include <errno.h>
#include <pwd.h>
#include <sys/types.h>

int youruid(uid_t *passto,char const *owner)
{
	int e ;
	e = errno ;
	errno = 0 ;
	struct passwd *st ;
	if (!(st = getpwnam(owner)) || errno)
	{
		if (!errno) errno = ESRCH ;
		return 0 ;
	}
	*passto = st->pw_uid ;
	errno = e ;
	return 1 ;
}

int yourgid(gid_t *passto,uid_t owner)
{
	int e ;
	e = errno ;
	errno = 0 ;
	struct passwd *st ;
	if (!(st = getpwuid(owner)) || errno)
	{
		if (!errno) errno = ESRCH ;
		return 0 ;
	}
	*passto = st->pw_gid ;
	errno = e ;
	return 1 ;
}
