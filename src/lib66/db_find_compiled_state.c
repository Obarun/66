/* 
 * db_find_compiled_state.c
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

#include <sys/stat.h>

#include <oblibs/error2.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/constants.h>

/** 1-> backup
 * 0-> ori 
 * -1->not initiated */
int db_find_compiled_state(char const *livetree, char const *treename)
{
	int r ;
	size_t treelen = strlen(livetree) ;
	size_t namelen = strlen(treename) ;
		
	struct stat st ;

	char current[treelen + 1 + namelen + SS_DB_LEN + 1] ;
	memcpy(current, livetree, treelen) ;
	current[treelen] = '/' ;
	memcpy(current + treelen + 1, treename,namelen) ;
	memcpy(current + treelen + 1 + namelen, SS_DB, SS_DB_LEN) ;
	current[treelen + 1 + namelen + SS_DB_LEN] = 0 ;
	
	if(lstat(current,&st) < 0) return -1 ;
	if(!(S_ISLNK(st.st_mode))) 
	{
		VERBO3 strerr_warnwu2x("find symlink: ",current) ;
		return -1 ;
	}
	
	stralloc symreal = STRALLOC_ZERO ;
	
	r = sarealpath(&symreal,current) ;
	if (r < 0 )
	{
		VERBO3 strerr_warnwu2x("find real path: ",current) ;
		return -1 ; 
	}
	char *b = NULL ;
	b = memmem(symreal.s,symreal.len,SS_BACKUP,SS_BACKUP_LEN) ;

	stralloc_free(&symreal) ;
	
	if (!b) return 0 ;
		
	return 1 ;
}
