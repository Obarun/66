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
#include <66/ssexec.h>

/** 1-> backup
 * 0-> ori */
int db_switch_to(ssexec_t *info, char const *const *envp,unsigned int where)
{
	int r ;
	
	stralloc db = STRALLOC_ZERO ;
	char type[UINT_FMT] ;
	size_t typelen = uint_fmt(type, BUNDLE) ;
	type[typelen] = 0 ;
	
	size_t cmdlen ;
	char cmd[typelen + 6 + 1] ;
	memcpy(cmd,"-t",2) ;
	memcpy(cmd + 2,type,typelen) ;
	cmdlen = 2 + typelen ;
	memcpy(cmd + cmdlen," -b",3) ;
	cmd[cmdlen + 3] = 0 ;
	
	r = backup_cmd_switcher(VERBOSITY,cmd,info) ;
	if (r < 0)
	{
		VERBO3 strerr_warnwu2sys("find origin of db service for: ",info->treename) ;
		goto err ;
	}
	// point to origin
	if (!r && where)
	{
		VERBO3 strerr_warnt2x("make a backup of db service for: ",info->treename) ;
		if (!backup_make_new(info,LONGRUN))
		{
			VERBO3 strerr_warnwu2sys("make a backup of db service for: ",info->treename) ;
			goto err ;
		}
		VERBO3 strerr_warnt3x("switch db service for tree: ",info->treename," to backup") ;
		memcpy(cmd + cmdlen," -s1",4) ;
		cmd[cmdlen + 4] = 0 ;
		r = backup_cmd_switcher(VERBOSITY,cmd,info) ;
		if (r < 0)
		{
			VERBO3 strerr_warnwu3sys("switch db service for: ",info->treename," to backup") ;
			goto err ;
		}
		if (db_ok(info->livetree.s, info->treename))
		{
			if (!backup_realpath_sym(&db,info,LONGRUN))
			{
				VERBO3 strerr_warnwu2sys("find path of db: ",db.s) ;
			}
			VERBO3 strerr_warnt4x("update ",info->livetree.s," to ",db.s) ;
			if (!db_update(db.s, info,envp))
			{	
				VERBO3 strerr_warnt2x("rollback db service: ", info->treename) ;
				memcpy(cmd + cmdlen," -s0",4) ;
				cmd[cmdlen + 4] = 0 ;
				r = backup_cmd_switcher(VERBOSITY,cmd,info) ;
				if (r < 0)
				{
					VERBO3 strerr_warnwu3sys("switch db service for: ",info->treename," to source") ;
					goto err ;
				}
				db = stralloc_zero ;
				if (!backup_realpath_sym(&db,info,LONGRUN))
				{
					VERBO3 strerr_warnwu2sys("find path of db service for: ",info->treename) ;
					goto err ;
				}
				if (!db_update(db.s,info,envp))
				{
					VERBO3 strerr_warnwu3sys("switch: ",info->treename," to source") ;
					VERBO3 strerr_warnwu1sys("unable to rollback the db state, please make a bug report") ;
					goto err ;
				}
			}			
		}
	}
	else if (r > 0 && !where)
	{
		VERBO3 strerr_warnt3x("switch db service for tree: ",info->treename," to source") ;
		memcpy(cmd + cmdlen," -s0",4) ;
		cmd[cmdlen + 4] = 0 ;
		r = backup_cmd_switcher(VERBOSITY,cmd,info) ;
		if (r < 0)
		{
			VERBO3 strerr_warnwu3sys("switch db service for: ",info->treename," to source") ;
			goto err ;
		}
		
		if (db_ok(info->livetree.s,info->treename))
		{
			if (!backup_realpath_sym(&db,info,LONGRUN))
			{
				VERBO3 strerr_warnwu2sys("find path of db: ",db.s) ;
				goto err ;
			}
			VERBO3 strerr_warnt4x("update ",info->livetree.s," to ",db.s) ;
			if (!db_update(db.s, info,envp))
			{	
				VERBO3 strerr_warnt2x("rollback db: ", info->treename) ;
				memcpy(cmd + cmdlen," -s1",4) ;
				cmd[cmdlen + 4] = 0 ;
				r = backup_cmd_switcher(VERBOSITY,cmd,info) ;
				if (r < 0)
				{
					VERBO3 strerr_warnwu3sys("switch db service for: ",info->treename," to backup") ;
					goto err ;
				}
				db = stralloc_zero ;
				if (!backup_realpath_sym(&db,info,LONGRUN))
				{
					VERBO3 strerr_warnwu2sys("find path of db: ",info->treename) ;
					goto err ;
				}
				if (!db_update(db.s, info,envp))
				{
					VERBO3 strerr_warnwu3sys("switch: ",info->treename," to source") ;
					VERBO3 strerr_warnwu1sys("unable to rollback the db state, please make a bug report") ;
					goto err ;
				}
			}
		}
		VERBO3 strerr_warnt2x("make a backup of db service for: ",info->treename) ;
		if (!backup_make_new(info,LONGRUN))
		{
			VERBO3 strerr_warnwu2sys("make a backup of db service for: ",info->treename) ;
			goto err ;
		}
	}
	
	stralloc_free(&db) ;
	return 1 ;

	err:
		stralloc_free(&db) ;
		return 0 ;
}
