/* 
 * tree_switch_tobackup.c
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

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/constants.h>
#include <66/utils.h>

int tree_switch_tobackup(char const *base, char const *treename, char const *tree, char const *livetree,char const *const *envp)
{
	int r ;
	
	size_t baselen = strlen(base) - 1 ;//remove the last slash
	size_t treenamelen = strlen(treename) ;
	
	r = tree_cmd_switcher(VERBOSITY,"-b",treename) ;
	/** !r we are on original source, we need to switch
	* to backup */
	if (r < 0)
	{
		VERBO3 strerr_warnwu2x("read symlink in: ", treename) ;
		return 0 ;
	}
	if (!r) 
	{ 
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
		if (!hiercopy(tree, treetmp))
		{
			VERBO3 strerr_warnwu4sys("copy: ",tree," to ",treetmp) ;
			return 0 ;
		}
		r = tree_cmd_switcher(VERBOSITY,"-s1",treename) ;
		if (r != 1)
		{
			rm_rf(treetmp) ;
			VERBO3 strerr_warnwu3x("switch: ",treename," to backup directory") ;
			return 0 ;
		}
		
		stralloc tmp = STRALLOC_ZERO ;
		
		if (!tree_from_current(&tmp,tree))
		{
			rm_rf(treetmp) ;
			VERBO3 strerr_warnwu2x("find current state of tree: ", treename) ;
			return 0 ;
		}
		/** only pass through here if the db is currently in use*/
		if ((db_find_compiled_state(livetree,treename) >= 0))
		{
			if (!db_update(tmp.s, treename,livetree,envp))
			{
				rm_rf(treetmp) ;
				VERBO3 strerr_warnwu6x("update db: ",treename," to ",tmp.s,"/compiled/",treename) ;
				return 0 ;
			}
		}
		stralloc_free(&tmp) ;
	}
	
	return 1 ;
}
