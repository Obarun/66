/* 
 * tree_switch_current.c
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

#include <66/tree.h>
 
#include <sys/types.h>
#include <string.h>
#include <sys/stat.h>

//#include <oblibs/log.h>
#include <oblibs/directory.h>
#include <oblibs/types.h>

#include <skalibs/stralloc.h>
#include <skalibs/types.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/utils.h>


int tree_switch_current(char const *base, char const *treename)
{
	ssize_t r ;
	char pack[UID_FMT] ;
	size_t baselen = strlen(base) ;
	size_t treelen = strlen(treename) ;
	size_t newlen ;
	size_t packlen ;
	packlen = uint_fmt(pack,MYUID) ;
	pack[packlen] = 0 ;
	char dst[baselen + SS_TREE_CURRENT_LEN + 1 + packlen + treelen + 2 + 1] ;
	struct stat st ;
		
	memcpy(dst,base,baselen) ;
	memcpy(dst + baselen,SS_SYSTEM,SS_SYSTEM_LEN) ;
	dst[baselen + SS_SYSTEM_LEN] = '/' ;
	memcpy(dst + baselen + SS_SYSTEM_LEN + 1,treename,treelen) ;
	dst[baselen + SS_SYSTEM_LEN + 1 + treelen] = 0 ;

	r = scan_mode(dst,S_IFDIR) ;
	if (r <= 0) return 0 ;

	memcpy(dst + baselen,SS_TREE_CURRENT,SS_TREE_CURRENT_LEN) ;
	dst[baselen + SS_TREE_CURRENT_LEN] = '/' ;
	memcpy(dst + baselen + SS_TREE_CURRENT_LEN + 1, pack, packlen) ;
	newlen = baselen + SS_TREE_CURRENT_LEN + 1 + packlen ;
	dst[newlen] = 0 ;
	
	r = scan_mode(dst,S_IFDIR) ;
	if (!r){
		if (!dir_create_parent(dst,0755)) return 0 ;
	}
	if(r == -1) return 0 ;
	
	char current[newlen + 1 + SS_TREE_CURRENT_LEN + 1] ;
	memcpy(current,dst,newlen) ;
	current[newlen] = '/' ;
	memcpy(current + newlen + 1, SS_TREE_CURRENT, SS_TREE_CURRENT_LEN) ;
	current[newlen + 1 + SS_TREE_CURRENT_LEN] = 0 ;
	 
	memcpy(dst + baselen,SS_SYSTEM,SS_SYSTEM_LEN) ;
	memcpy(dst + baselen + SS_SYSTEM_LEN,"/",1) ;
	memcpy(dst + baselen + SS_SYSTEM_LEN + 1,treename,treelen) ;
	dst[baselen + SS_SYSTEM_LEN + 1 + treelen] = 0 ;

	if(lstat(current,&st)<0) r = 0 ;
	if(!(S_ISLNK(st.st_mode)))
	{
		r = -1 ;
	}else r = 1 ;
	if(r>0){
		r = unlink(current) ;
		if (r<0) return 0 ;
	}
	if (symlink(dst, current) < 0) return 0 ;
	
	return 1 ;
}
