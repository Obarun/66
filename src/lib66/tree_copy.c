/* 
 * tree_copy.c
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

#include <string.h>

#include <oblibs/string.h>
#include <oblibs/directory.h>
#include <oblibs/error2.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/constants.h>
#include <66/utils.h>
#include <stdio.h>
int tree_copy(stralloc *dir, char const *tree,char const *treename)
{
	char *fdir = 0 ;
	size_t treelen = strlen(tree) ;
	char tmp[treelen + SS_SVDIRS_LEN + 1] ;
	memcpy(tmp,tree,treelen) ;
	memcpy(tmp + treelen,SS_SVDIRS,SS_SVDIRS_LEN) ;
	tmp[treelen + SS_SVDIRS_LEN] = 0 ;
	
	fdir = dir_create_tmp(dir,"/tmp",treename) ;
	if (!fdir) return 0 ;

	if (!hiercopy(tmp,fdir)) return 0 ;
	
	return 1 ;
}
