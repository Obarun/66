/* 
 * db_update.c
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
 
#include <66/db.h>

#include <s6-rc/config.h>//S6RC_BINPREFIX

#include <sys/stat.h>

#include <oblibs/error2.h>

#include <skalibs/types.h>
#include <skalibs/djbunix.h>

#include <66/constants.h>
#include <66/utils.h>

int db_update(char const *newdb, char const *tree, char const *live,char const *const *envp)
{
	
	pid_t pid ;
	int wstat ;
	size_t treelen = strlen(tree) ;
	size_t newdblen = strlen(newdb) ;
	size_t livelen = strlen(live) ;
	
	char db[newdblen + 1 + treelen + 1] ; 
	memcpy(db, newdb, newdblen) ;
	memcpy(db + newdblen, "/", 1) ;
	memcpy(db + newdblen + 1, tree, treelen) ;
	db[newdblen + 1 + treelen] = 0 ;
		
	char newlive[livelen + 1 + treelen + 1] ;
	memcpy(newlive, live,livelen) ;
	memcpy(newlive + livelen , "/", 1) ;
	memcpy(newlive + livelen + 1, tree,treelen) ;
	newlive[livelen + 1 + treelen] = 0 ;
	
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
		
	VERBO3 strerr_warnt5x("update ",newlive," to ",db," ...") ;
	
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
