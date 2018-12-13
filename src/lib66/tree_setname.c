/* 
 * tree_setname.c
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

#include <stddef.h>

#include <oblibs/string.h>

char *tree_setname(char const *tree)
{
	size_t tlen = strlen(tree) ;
	ssize_t treelen = get_rlen_until(tree,'/',tlen) ;
	if (treelen <= 0) return 0 ;
	treelen++ ;
	size_t treenamelen = tlen - treelen ;

	char treename[treenamelen + 1] ;
	memcpy(treename, tree + treelen,treenamelen) ;
	treename[treenamelen] = 0 ;
	
	return obstr_dup(treename) ;
}
