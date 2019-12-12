/* 
 * svc_switch_to.c
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


#include <oblibs/log.h>

#include <string.h>

#include <skalibs/types.h>

#include <66/backup.h>
#include <66/utils.h>
#include <66/enum.h>
#include <66/ssexec.h>

/** 1-> backup
 * 0-> ori */
int svc_switch_to(ssexec_t *info,unsigned int where)
{
	int r ;
	
	char type[UINT_FMT] ;
	size_t typelen = uint_fmt(type, CLASSIC) ;
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
		log_warnusys_return(LOG_EXIT_ZERO,"find origin of svc service for: ",info->treename.s) ;
	
	// point to origin
	if (!r && where)
	{
		log_trace("make a backup of svc service for: ",info->treename.s) ;
		if (!backup_make_new(info,CLASSIC))
			log_warnusys_return(LOG_EXIT_ZERO,"make a backup of svc service for: ",info->treename.s) ;
		
		log_trace("switch svc symlink of tree: ",info->treename.s," to backup") ;
		memcpy(cmd + cmdlen," -s1",4) ;
		cmd[cmdlen + 4] = 0 ;
		r = backup_cmd_switcher(VERBOSITY,cmd,info) ;
		if (r < 0)
		{
			log_warnusys("switch svc symlink of tree: ",info->treename.s," to backup") ;
		}
	}
	else if (r > 0 && !where)
	{
		log_trace("switch svc symlink of tree: ",info->treename.s," to source") ;
		memcpy(cmd + cmdlen," -s0",4) ;
		cmd[cmdlen + 4] = 0 ;
		r = backup_cmd_switcher(VERBOSITY,cmd,info) ;
		if (r < 0)
		{
			log_warnusys("switch svc symlink of tree: ",info->treename.s," to source") ;
		}
		
		log_trace("make a backup of svc service for: ",info->treename.s) ;
		if (!backup_make_new(info,CLASSIC))
			log_warnusys_return(LOG_EXIT_ZERO,"make a backup of svc service for: ",info->treename.s) ;
	}
	return 1 ;
}
