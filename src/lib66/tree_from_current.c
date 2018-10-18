/* 
 * tree_from_current.c
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

int tree_from_current(stralloc *sa, char const *tree)
{
	int r ;
	size_t treelen = strlen(tree) ;
		
	struct stat st ;

	char current[treelen + SS_SVDIRS_LEN + 1 + SS_TREE_CURRENT_LEN + 1] ;
	memcpy(current, tree, treelen) ;
	memcpy(current + treelen, SS_SVDIRS, SS_SVDIRS_LEN) ;
	memcpy(current + treelen + SS_SVDIRS_LEN, "/", 1) ;
	memcpy(current + treelen + SS_SVDIRS_LEN + 1, SS_TREE_CURRENT, SS_TREE_CURRENT_LEN) ;
	current[treelen + SS_SVDIRS_LEN + 1 + SS_TREE_CURRENT_LEN] = 0 ;
	
	if(lstat(current,&st) < 0) return 0 ;
	if(!(S_ISLNK(st.st_mode))) 
	{
		VERBO3 strerr_warnwu2x("find symlink: ",current) ;
		return 0 ;
	}
	r = sarealpath(sa,current) ;
	if (r < 0 )
	{
		VERBO3 strerr_warnwu2x("find real path: ",current) ;
		return 0 ; 
	}
	if (!stralloc_0(sa)) retstralloc(0,"find_current_state") ;
	
	return 1 ;
}
