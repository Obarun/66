/* 
 * db_switch_to.c
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

#include <66/db.h>

#include <oblibs/error2.h>

#include <skalibs/stralloc.h>
#include <skalibs/types.h>

#include <66/constants.h>
#include <66/backup.h>
#include <66/utils.h>
#include <66/enum.h>

/** 1-> backup
 * 0-> ori */
int db_switch_to(char const *base, char const *livetree, char const *tree, char const *treename, char const *const *envp,unsigned int where)
{
	int r ;
	
	stralloc db = STRALLOC_ZERO ;
	char type[UINT_FMT] ;
	type[uint_fmt(type, BUNDLE)] = 0 ;
	size_t typelen = strlen(type) ;
	size_t cmdlen ;
	char cmd[typelen + 6 + 1] ;
	memcpy(cmd,"-t",2) ;
	memcpy(cmd + 2,type,typelen) ;
	cmdlen = 2 + typelen ;
	memcpy(cmd + cmdlen," -b",3) ;
	cmd[cmdlen + 3] = 0 ;
	
	r = backup_cmd_switcher(VERBOSITY,cmd,treename) ;
	if (r < 0)
	{
		VERBO3 strerr_warnwu2sys("find origin of db service for: ",treename) ;
		goto err ;
	}
	// point to origin
	if (!r && where)
	{
		VERBO3 strerr_warnt2x("make a backup of db service for: ",treename) ;
		if (!backup_make_new(base,tree,treename,LONGRUN))
		{
			VERBO3 strerr_warnwu2sys("make a backup of db service for: ",treename) ;
			goto err ;
		}
		VERBO3 strerr_warnt3x("switch db service for tree: ",treename," to backup") ;
		memcpy(cmd + cmdlen," -s1",4) ;
		cmd[cmdlen + 4] = 0 ;
		r = backup_cmd_switcher(VERBOSITY,cmd,treename) ;
		if (r < 0)
		{
			VERBO3 strerr_warnwu3sys("switch db service for: ",treename," to backup") ;
			goto err ;
		}
		if (db_ok(livetree,treename))
		{
			if (!backup_realpath_sym(&db,tree,LONGRUN))
			{
				VERBO3 strerr_warnwu2sys("find path of db: ",db.s) ;
			}
			VERBO3 strerr_warnt4x("update ",livetree," to ",db.s) ;
			if (!db_update(db.s, treename,livetree,envp))
			{	
				VERBO3 strerr_warnt2x("rollback db service: ", treename) ;
				memcpy(cmd + cmdlen," -s0",4) ;
				cmd[cmdlen + 4] = 0 ;
				r = backup_cmd_switcher(VERBOSITY,cmd,treename) ;
				if (r < 0)
				{
					VERBO3 strerr_warnwu3sys("switch db service for: ",treename," to source") ;
					goto err ;
				}
				db = stralloc_zero ;
				if (!backup_realpath_sym(&db,tree,LONGRUN))
				{
					VERBO3 strerr_warnwu2sys("find path of db service for: ",treename) ;
					goto err ;
				}
				if (!db_update(db.s, treename,livetree,envp))
				{
					VERBO3 strerr_warnwu3sys("switch: ",treename," to source") ;
					VERBO3 strerr_warnwu1sys("unable to rollback the db state, please make a bug report") ;
					goto err ;
				}
			}			
		}
	}
	else if (r > 0 && !where)
	{
		VERBO3 strerr_warnt3x("switch db service for tree: ",treename," to source") ;
		memcpy(cmd + cmdlen," -s0",4) ;
		cmd[cmdlen + 4] = 0 ;
		r = backup_cmd_switcher(VERBOSITY,cmd,treename) ;
		if (r < 0)
		{
			VERBO3 strerr_warnwu3sys("switch db service for: ",treename," to source") ;
			goto err ;
		}
		
		if (db_ok(livetree,treename))
		{
			if (!backup_realpath_sym(&db,tree,LONGRUN))
			{
				VERBO3 strerr_warnwu2sys("find path of db: ",db.s) ;
				goto err ;
			}
			VERBO3 strerr_warnt4x("update ",livetree," to ",db.s) ;
			if (!db_update(db.s, treename,livetree,envp))
			{	
				VERBO3 strerr_warnt2x("rollback db: ", treename) ;
				memcpy(cmd + cmdlen," -s1",4) ;
				cmd[cmdlen + 4] = 0 ;
				r = backup_cmd_switcher(VERBOSITY,cmd,treename) ;
				if (r < 0)
				{
					VERBO3 strerr_warnwu3sys("switch db service for: ",treename," to backup") ;
					goto err ;
				}
				db = stralloc_zero ;
				if (!backup_realpath_sym(&db,tree,LONGRUN))
				{
					VERBO3 strerr_warnwu2sys("find path of db: ",treename) ;
					goto err ;
				}
				if (!db_update(db.s, treename,livetree,envp))
				{
					VERBO3 strerr_warnwu3sys("switch: ",treename," to source") ;
					VERBO3 strerr_warnwu1sys("unable to rollback the db state, please make a bug report") ;
					goto err ;
				}
			}
		}
		VERBO3 strerr_warnt2x("make a backup of db service for: ",treename) ;
		if (!backup_make_new(base,tree,treename,LONGRUN))
		{
			VERBO3 strerr_warnwu2sys("make a backup of db service for: ",treename) ;
			goto err ;
		}
	}
	
	stralloc_free(&db) ;
	return 1 ;

	err:
		stralloc_free(&db) ;
		return 0 ;
}
