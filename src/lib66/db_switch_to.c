/* 
 * db_switch_to.c
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

#include <66/db.h>

#include <string.h>

#include <oblibs/log.h>

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
	size_t typelen = uint_fmt(type, TYPE_BUNDLE) ;
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
		log_warnusys("find realpath of symlink for db of tree: ",info->treename.s) ;
		goto err ;
	}
	// point to origin
	if (!r && where)
	{
		log_trace("make a backup of db service for: ",info->treename.s) ;
		if (!backup_make_new(info,TYPE_LONGRUN))
		{
			log_warnusys("make a backup of db service for: ",info->treename.s) ;
			goto err ;
		}
		log_trace("switch db symlink of tree: ",info->treename.s," to backup") ;
		memcpy(cmd + cmdlen," -s1",4) ;
		cmd[cmdlen + 4] = 0 ;
		r = backup_cmd_switcher(VERBOSITY,cmd,info) ;
		if (r < 0)
		{
			log_warnusys("switch db symlink of tree: ",info->treename.s," to backup") ;
			goto err ;
		}
		if (db_ok(info->livetree.s, info->treename.s))
		{
			if (!backup_realpath_sym(&db,info,TYPE_LONGRUN))
			{
				log_warnusys("find path of db: ",db.s) ;
				goto err ;
			}
			log_trace("update ",info->livetree.s,"/",info->treename.s," to ",db.s) ;
			if (!db_update(db.s, info,envp))
			{	
				log_trace("rollback db service: ", info->treename.s) ;
				memcpy(cmd + cmdlen," -s0",4) ;
				cmd[cmdlen + 4] = 0 ;
				r = backup_cmd_switcher(VERBOSITY,cmd,info) ;
				if (r < 0)
				{
					log_warnusys("switch db symlink of tree: ",info->treename.s," to source") ;
					goto err ;
				}
			}			
		}
	}
	else if (r > 0 && !where)
	{
		log_trace("switch db symlink of tree: ",info->treename.s," to source") ;
		memcpy(cmd + cmdlen," -s0",4) ;
		cmd[cmdlen + 4] = 0 ;
		r = backup_cmd_switcher(VERBOSITY,cmd,info) ;
		if (r < 0)
		{
			log_warnusys("switch db symlink of tree: ",info->treename.s," to source") ;
			goto err ;
		}
		
		if (db_ok(info->livetree.s,info->treename.s))
		{
			if (!backup_realpath_sym(&db,info,TYPE_LONGRUN))
			{
				log_warnusys("find path of db: ",db.s) ;
				goto err ;
			}
			log_trace("update ",info->livetree.s,"/",info->treename.s," to ",db.s) ;
			if (!db_update(db.s, info,envp))
			{	
				log_trace("rollback db: ", info->treename.s) ;
				memcpy(cmd + cmdlen," -s1",4) ;
				cmd[cmdlen + 4] = 0 ;
				r = backup_cmd_switcher(VERBOSITY,cmd,info) ;
				if (r < 0)
				{
					log_warnusys("switch db service for: ",info->treename.s," to backup") ;
					goto err ;
				}
			}
		}
		log_trace("make a backup of db service for: ",info->treename.s) ;
		if (!backup_make_new(info,TYPE_LONGRUN))
		{
			log_warnusys("make a backup of db service for: ",info->treename.s) ;
			goto err ;
		}
	}
	
	stralloc_free(&db) ;
	return 1 ;

	err:
		stralloc_free(&db) ;
		return 0 ;
}
