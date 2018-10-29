/* 
 * tree_sethome.c
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

#include <66/tree.h>

#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#include <oblibs/types.h>
#include <oblibs/error2.h>

#include <skalibs/stralloc.h>

#include <66/constants.h>

int tree_sethome(stralloc *tree, char const *base)
{
	int r ;
	
	if (!tree->len)
	{
		if (!tree_find_current(tree, base)) return -1 ;
	}
	else
	{
		char treename[tree->len + 1] ;
		memcpy(treename,tree->s,tree->len - 1) ;
		treename[tree->len - 1] = 0 ;
		tree->len = 0 ;
		if (!stralloc_cats(tree,base)) retstralloc(0,"main") ;
		if (!stralloc_cats(tree,SS_SYSTEM "/")) retstralloc(0,"main") ;
		if (!stralloc_cats(tree,treename)) retstralloc(0,"main") ;
		if (!stralloc_0(tree)) retstralloc(0,"main") ;
		r = scan_mode(tree->s,S_IFDIR) ;
		if (r < 0) errno = EEXIST ;
		if (r != 1) return 0 ;
	}
	
	return 1 ;
}
