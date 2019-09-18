/* 
 * tree_find_current.c
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

#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>

#include <oblibs/error2.h>
#include <oblibs/types.h>

#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/constants.h>

int tree_find_current(stralloc *tree, char const *base,uid_t owner)
{
	ssize_t r ;
	size_t baselen = strlen(base) ;
	char pack[UID_FMT] ;
	
	uint32_pack(pack,owner) ;
	size_t packlen = uint_fmt(pack,owner) ;
	pack[packlen] = 0 ;
	
	char sa[baselen + SS_TREE_CURRENT_LEN + 1 + packlen + 1 + SS_TREE_CURRENT_LEN + 1] ;
	memcpy(sa,base,baselen) ;
	memcpy(sa + baselen, SS_TREE_CURRENT, SS_TREE_CURRENT_LEN) ;
	sa[baselen + SS_TREE_CURRENT_LEN] = '/' ;
	memcpy(sa + baselen + SS_TREE_CURRENT_LEN + 1, pack,packlen) ;
	sa[baselen + SS_TREE_CURRENT_LEN + 1 + packlen] = '/' ;
	memcpy(sa + baselen + SS_TREE_CURRENT_LEN + 1 + packlen + 1,SS_TREE_CURRENT,SS_TREE_CURRENT_LEN) ;
	sa[baselen + SS_TREE_CURRENT_LEN + 1 + packlen + 1 + SS_TREE_CURRENT_LEN] = 0 ;
	
	r = scan_mode(sa,S_IFDIR) ;
	if(r <= 0) return 0 ; 
	r = sarealpath(tree,sa) ;
	if (r < 0 ) return 0 ; 
	if (!stralloc_0(tree)) retstralloc(0,"find_current") ;
	tree->len--;
	return 1 ;
}
