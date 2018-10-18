/* 
 * db_ok.c
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
 
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <66/constants.h>

int db_ok(char const *livetree, char const *treename)
{
	size_t treelen = strlen(livetree) ;
	size_t namelen = strlen(treename) ;
		
	struct stat st ;

	char sym[treelen + 1 + namelen + 1] ;
	memcpy(sym, livetree, treelen) ;
	sym[treelen] = '/' ;
	memcpy(sym + treelen + 1, treename,namelen) ;
	sym[treelen + 1 + namelen] = 0 ;

	if(lstat(sym,&st) < 0) return 0 ;
	if(!(S_ISLNK(st.st_mode))) 
		return 0 ;
		
	return 1 ;
}
