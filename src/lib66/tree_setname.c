/* 
 * tree_setname.c
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

#include <66/tree.h>

#include <string.h>
#include <sys/types.h>//ssize

#include <oblibs/string.h>

#include <skalibs/stralloc.h>

char tree_setname(stralloc *sa,char const *tree)
{
	size_t tlen = strlen(tree) ;
	ssize_t treelen = get_rlen_until(tree,'/',tlen) ;
	if (treelen <= 0) return 0 ;
	treelen++ ;
	size_t treenamelen = tlen - treelen ;

	if (!stralloc_catb(sa,tree + treelen,treenamelen)) return 0 ;
	if (!stralloc_0(sa)) return 0 ;
	sa->len--;
	return 1 ;
}
