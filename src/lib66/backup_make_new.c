/* 
 * backup_make_new.c
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
#include <66/db.h>

#include <sys/stat.h>
#include <stdlib.h>
#include <oblibs/error2.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/constants.h>
#include <66/utils.h>
#include <66/enum.h>

// force: 0->check, 1->remove and create
int backup_make_new(char const *base, char const *tree, char const *treename, unsigned int type)
{
	int r ;
	
	size_t baselen = strlen(base) ;
	size_t treenamelen = strlen(treename) ;
	size_t newsrc ;
	size_t newback ;
	size_t typelen ;
	char *ptype = NULL ;
	
	if (type == CLASSIC)
	{
		ptype = SS_SVC ;
		typelen = SS_SVC_LEN ;
	}
	else
	{
		ptype = SS_DB ;
		typelen = SS_DB_LEN ;
	}
	
	char src[baselen + SS_SYSTEM_LEN + 1 + treenamelen + SS_SVDIRS_LEN + SS_RESOLVE_LEN + 1] ;
	memcpy(src,base,baselen) ;
	memcpy(src + baselen, SS_SYSTEM, SS_SYSTEM_LEN) ;
	src[baselen + SS_SYSTEM_LEN] = '/' ;
	memcpy(src + baselen + SS_SYSTEM_LEN + 1,treename,treenamelen) ;
	memcpy(src+ baselen + SS_SYSTEM_LEN + 1 + treenamelen,SS_SVDIRS, SS_SVDIRS_LEN) ;
	newsrc = baselen + SS_SYSTEM_LEN + 1 + treenamelen + SS_SVDIRS_LEN ;
	memcpy(src+ baselen + SS_SYSTEM_LEN + 1 + treenamelen + SS_SVDIRS_LEN, ptype,typelen) ;
	src[baselen + SS_SYSTEM_LEN + 1 + treenamelen + SS_SVDIRS_LEN +  typelen] = 0 ;
	
	char back[baselen + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1 + treenamelen + SS_RESOLVE_LEN + 1] ;
	memcpy(back, base, baselen) ;
	memcpy(back + baselen, SS_SYSTEM, SS_SYSTEM_LEN) ;
	memcpy(back + baselen + SS_SYSTEM_LEN, SS_BACKUP, SS_BACKUP_LEN) ;
	back[baselen + SS_SYSTEM_LEN + SS_BACKUP_LEN] = '/' ;
	memcpy(back + baselen + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1, treename, treenamelen) ;
	newback = baselen + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1 + treenamelen ;
	memcpy(back + baselen + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1 + treenamelen, ptype,typelen) ;
	back[baselen + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1 + treenamelen + typelen] = 0 ;
	
	r = scan_mode(back,S_IFDIR) ;
	if (r || (r < 0))
	{
		VERBO3 strerr_warnt2x("rm directory: ", back) ;
		if (rm_rf(back) < 0)
		{
			VERBO3 strerr_warnwu2sys("remove: ",back) ;
			return 0 ;
		}
		r = 0 ;
	}
	if (!r)
	{
		VERBO3 strerr_warnt2x("create directory: ", back) ;
		if (!dir_create(back,0755))
		{
			VERBO3 strerr_warnwu2sys("create directory: ",back) ;
			return 0 ;
		}
	}
	VERBO3 strerr_warnt4x("copy: ",src," to: ", back) ;
	if (!hiercopy(src, back))
	{
		VERBO3 strerr_warnwu4sys("copy: ",src," to ",back) ;
		return 0 ;
	}
	
	memcpy(src + newsrc,SS_RESOLVE,SS_RESOLVE_LEN) ;
	src[newsrc + SS_RESOLVE_LEN] = 0 ;
	
	memcpy(back + newback,SS_RESOLVE,SS_RESOLVE_LEN) ;
	back[newback + SS_RESOLVE_LEN] = 0 ;
	
	r = scan_mode(back,S_IFDIR) ;
	if (r || (r < 0))
	{
		VERBO3 strerr_warnt2x("rm directory: ", back) ;
		if (rm_rf(back) < 0)
		{
			VERBO3 strerr_warnwu2sys("remove: ",back) ;
			return 0 ;
		}
		r = 0 ;
	}
	if (!r)
	{
		VERBO3 strerr_warnt2x("create directory: ", back) ;
		if (!dir_create(back,0755))
		{
			VERBO3 strerr_warnwu2sys("create directory: ",back) ;
			return 0 ;
		}
	}
	VERBO3 strerr_warnt4x("copy: ",src," to: ", back) ;
	if (!hiercopy(src, back))
	{
		VERBO3 strerr_warnwu4sys("copy: ",src," to ",back) ;
		return 0 ;
	}
	
	return 1 ;
}
