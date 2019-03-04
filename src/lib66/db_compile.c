/* 
 * db_compile.c
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

#include <string.h>
#include <sys/stat.h>

#include <oblibs/string.h>
#include <oblibs/error2.h>
#include <oblibs/directory.h>

#include <skalibs/stralloc.h>
#include <skalibs/types.h>
#include <skalibs/djbunix.h>

#include <66/utils.h>
#include <66/constants.h>
#include <66/ssexec.h>

#include <stdio.h>
int db_compile(char const *workdir, char const *tree, char const *treename, char const *const *envp)
{
	int wstat, r ;
	pid_t pid ;
	
	stralloc source = STRALLOC_ZERO ;
	stralloc destination = STRALLOC_ZERO ;
		
	if (!stralloc_cats(&destination,workdir)) retstralloc(0,"compile_db") ;
	if (!stralloc_cats(&destination,SS_DB)) retstralloc(0,"compile_db") ;
	if (!stralloc_0(&destination)) retstralloc(0,"compile_db") ;
	r = dir_search(destination.s,treename,S_IFDIR) ;//+1 to remove the '/'
	destination.len--;
	if (!stralloc_cats(&destination,"/")) retstralloc(0,"compile_db") ;
	if (!stralloc_cats(&destination,treename)) retstralloc(0,"compile_db") ;
	if (!stralloc_0(&destination)) retstralloc(0,"compile_db") ;
	if (r)
	{
		if (rm_rf(destination.s) < 0)
		{
			VERBO3 strerr_warnwu2sys("remove ", destination.s) ;
			return 0 ;
		}
	}
	if (!stralloc_cats(&source,workdir)) retstralloc(0,"compile_db") ;
	if (!stralloc_cats(&source,SS_DB SS_SRC)) retstralloc(0,"compile_db") ;
	if (!stralloc_0(&source)) retstralloc(0,"compile_db") ;
			
	char const *newargv[7] ;
	unsigned int m = 0 ;
	char fmt[UINT_FMT] ;
	fmt[uint_fmt(fmt, VERBOSITY)] = 0 ;
	
	newargv[m++] = S6RC_BINPREFIX "s6-rc-compile" ;
	newargv[m++] = "-v" ;
	newargv[m++] = fmt ;
	newargv[m++] = "--" ;
	newargv[m++] = destination.s ;
	newargv[m++] = source.s ;
	newargv[m++] = 0 ;
	
	pid = child_spawn0(newargv[0],newargv,envp) ;
	if (waitpid_nointr(pid,&wstat, 0) < 0)
	{
		VERBO3 strerr_warnwu2sys("wait for ",newargv[0]) ;
		return 0 ;
	}
	if (wstat)
	{
		VERBO3 strerr_warnwu2x("compile: ",destination.s) ;
		return 0 ;
	}
	
	stralloc_free(&source) ;
	stralloc_free(&destination) ;
	
	return 1 ;
}
