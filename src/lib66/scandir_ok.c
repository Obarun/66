/* 
 * scandir_ok.c
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
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#include <skalibs/djbunix.h>

#include <s6/s6-supervise.h>


/** this following function come from Laurent Bercot 
 * author of s6 library all rights reserved on this author
 * It was just modified a little bit to be able to scan
 * a scandir directory instead of a service directory */
int scandir_ok (char const *dir)
{
	size_t dirlen = strlen(dir) ;
	int fd ;
	char fn[dirlen + 1 + strlen(S6_SVSCAN_CTLDIR) + 9 + 1] ;
	memcpy(fn, dir, dirlen) ;
	fn[dirlen] = '/' ;
	memcpy(fn + dirlen + 1, S6_SVSCAN_CTLDIR, strlen(S6_SVSCAN_CTLDIR)) ;
	memcpy(fn + dirlen + 1 + strlen(S6_SVSCAN_CTLDIR), "/control", 9) ;
	fn[dirlen + 1 + strlen(S6_SVSCAN_CTLDIR) + 9] = 0 ;
	fd = open_write(fn) ;
	if (fd < 0)
	{
		if ((errno == ENXIO) || (errno == ENOENT)) return 0 ;
		else return -1 ;
	}
	fd_close(fd) ;
	return 1 ;
}
