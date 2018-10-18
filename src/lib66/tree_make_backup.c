/* 
 * tree_make_backup.c
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

#include <oblibs/error2.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/constants.h>
#include <66/utils.h>

int tree_make_backup(char const *base, char const *tree, char const *treename)
{
	int r ;
	
	size_t baselen = strlen(base) - 1 ;//remove the last slash
	size_t treenamelen = strlen(treename) ;
	
	char treetmp[baselen + 1 + SS_SYSTEM_LEN + SS_BACKUP_LEN + treenamelen + 1 + 1] ;
	memcpy(treetmp, base, baselen) ;
	memcpy(treetmp + baselen, "/", 1) ;
	memcpy(treetmp + baselen + 1, SS_SYSTEM, SS_SYSTEM_LEN) ;
	memcpy(treetmp + baselen + 1 + SS_SYSTEM_LEN, SS_BACKUP, SS_BACKUP_LEN) ;
	memcpy(treetmp + baselen + 1 + SS_SYSTEM_LEN + SS_BACKUP_LEN, "/", 1) ;
	memcpy(treetmp + baselen + 1 + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1, treename, treenamelen) ;
	treetmp[baselen + 1 + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1 + treenamelen ] = 0 ;
	
	r = scan_mode(treetmp,S_IFDIR) ;
	if (r || (r < 0))
	{
		if (rm_rf(treetmp) < 0)
		{
			VERBO3 strerr_warnwu2sys("remove: ",treetmp) ;
			return 0 ;
		}
	}
	if (!r)
	{
		if (!dir_create(treetmp,0755))
		{
			VERBO3 strerr_warnwu2sys("create directory: ",treetmp) ;
			return 0 ;
		}
	}
	if (!hiercopy(tree, treetmp))
	{
		VERBO3 strerr_warnwu4sys("copy: ",tree," to ",treetmp) ;
		return 0 ;
	}
	
	return 1 ;
}
