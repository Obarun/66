/* 
 * db_update.c
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
 
#include <66/db.h>

#include <s6-rc/config.h>//S6RC_BINPREFIX

#include <sys/stat.h>

#include <oblibs/error2.h>

#include <skalibs/types.h>
#include <skalibs/djbunix.h>

#include <66/constants.h>
#include <66/utils.h>
#include <66/ssexec.h>

int db_update(char const *newdb, ssexec_t *info,char const *const *envp)
{
	
	pid_t pid ;
	int wstat ;
	size_t newdblen = strlen(newdb) ;
	
	
	char db[newdblen + 1 + info->treename.len + 1] ; 
	memcpy(db, newdb, newdblen) ;
	memcpy(db + newdblen, "/", 1) ;
	memcpy(db + newdblen + 1, info->treename.s, info->treename.len) ;
	db[newdblen + 1 + info->treename.len] = 0 ;
		
	char newlive[info->livetree.len + 1 + info->treename.len + 1] ;
	memcpy(newlive, info->livetree.s,info->livetree.len) ;
	memcpy(newlive + info->livetree.len , "/", 1) ;
	memcpy(newlive + info->livetree.len + 1, info->treename.s,info->treename.len) ;
	newlive[info->livetree.len + 1 + info->treename.len] = 0 ;
	
	char const *newargv[10] ;
	unsigned int m = 0 ;
	char fmt[UINT_FMT] ;
	fmt[uint_fmt(fmt, VERBOSITY)] = 0 ;
	
	newargv[m++] = S6RC_BINPREFIX "s6-rc-update" ;
	newargv[m++] = "-v" ;
	newargv[m++] = fmt ;
	newargv[m++] = "-l" ;
	newargv[m++] =  newlive ;
	newargv[m++] = "--" ;
	newargv[m++] = db ;
	newargv[m++] = 0 ;
		
	pid = child_spawn0(newargv[0],newargv,envp) ;
	if (waitpid_nointr(pid,&wstat, 0) < 0)
	{
		strerr_warnwu2sys("wait for ",newargv[0]) ;
		return 0 ;
	}
	if (wstat)
	{
		VERBO3 strerr_warnwu4x("update: ",newlive," to ",db) ;
		return 0 ;
	}
	return 1 ;
}
