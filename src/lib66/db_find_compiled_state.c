/* 
 * db_find_compiled_state.c
 * 
 * Copyright (c) 2018-2020 Eric Vidal <eric@obarun.org>
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

#include <oblibs/log.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/constants.h>

/** return 1-> backup
 * return 0-> src 
 * -1->not initiated */
int db_find_compiled_state(char const *livetree, char const *treename)
{
	int r ;
	size_t treelen = strlen(livetree) ;
	size_t namelen = strlen(treename) ;
		
	struct stat st ;
	
	char current[treelen + 1 + namelen + 9 + 1] ;
	memcpy(current, livetree, treelen) ;
	current[treelen] = '/' ;
	memcpy(current + treelen + 1, treename,namelen) ;
	memcpy(current + treelen + 1 + namelen, "/compiled", 9) ;
	current[treelen + 1 + namelen + 9] = 0 ;
	
	if(lstat(current,&st) < 0) return -1 ;
	if(!(S_ISLNK(st.st_mode))) 
		log_warnu_return(LOG_EXIT_LESSONE,"find symlink: ",current) ;
		
	stralloc symreal = STRALLOC_ZERO ;
	
	r = sarealpath(&symreal,current) ;
	if (r < 0 ) 
	{
		stralloc_free(&symreal) ;
		log_warnu_return(LOG_EXIT_LESSONE,"find real path: ",current) ;
	}
	char *b = NULL ;
	b = memmem(symreal.s,symreal.len,SS_BACKUP,SS_BACKUP_LEN) ;

	stralloc_free(&symreal) ;
	
	if (!b) return 0 ;
		
	return 1 ;
}
