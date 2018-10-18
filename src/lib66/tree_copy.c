/* 
 * tree_copy.c
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

#include <oblibs/string.h>
#include <oblibs/directory.h>
#include <oblibs/error2.h>

#include <string.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/constants.h>
#include <66/utils.h>

int tree_copy(stralloc *dir, char const *tree,char const *treename)
{
	char *fdir = NULL ;

	
	stralloc tmp = STRALLOC_ZERO ;
	stralloc fdirtmp = STRALLOC_ZERO ;
	
	fdir = dir_create_tmp(&fdirtmp,"/tmp",treename) ;
	if (!fdir)
	{
		VERBO3 strerr_warnwu1x("create tempory directory") ;
		return 0 ;
	}
	if (!stralloc_cats(&tmp,tree)) retstralloc(0,"copy_tree") ;
	if (!stralloc_cats(&tmp,SS_SVDIRS)) retstralloc(0,"copy_tree") ;
	if (!stralloc_0(&tmp)) retstralloc(0,"copy_tree") ;
	
	if (!hiercopy(tmp.s, fdir))
	{
		VERBO3 strerr_warnwu2sys("to copy tree: ",tmp.s) ;
		return 0 ;
	}
	if (!stralloc_cats(dir,fdir)) retstralloc(0,"copy_tree") ;
	if (!stralloc_0(dir)) retstralloc(0,"copy_tree") ;

	stralloc_free(&tmp) ;
	stralloc_free(&fdirtmp) ;
	
	return 1 ;
}
