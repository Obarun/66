/* 
 * svc_switch_to.c
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


#include <oblibs/error2.h>

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
	type[uint_fmt(type, CLASSIC)] = 0 ;
	size_t typelen = strlen(type) ;
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
		VERBO3 strerr_warnwu2sys("find origin of svc service for: ",info->treename) ;
		return 0 ;
	}
	// point to origin
	if (!r && where)
	{
		VERBO3 strerr_warnt2x("make a backup of svc service for: ",info->treename) ;
		if (!backup_make_new(info,CLASSIC))
		{
			VERBO3 strerr_warnwu2sys("make a backup of svc service for: ",info->treename) ;
			return 0 ;
		}
		VERBO3 strerr_warnt3x("switch svc service for tree: ",info->treename," to backup") ;
		memcpy(cmd + cmdlen," -s1",4) ;
		cmd[cmdlen + 4] = 0 ;
		r = backup_cmd_switcher(VERBOSITY,cmd,info) ;
		if (r < 0)
		{
			VERBO3 strerr_warnwu3sys("switch svc service for: ",info->treename," to backup") ;
		}
	}
	else if (r > 0 && !where)
	{
		VERBO3 strerr_warnt3x("switch svc service for tree: ",info->treename," to source") ;
		memcpy(cmd + cmdlen," -s0",4) ;
		cmd[cmdlen + 4] = 0 ;
		r = backup_cmd_switcher(VERBOSITY,cmd,info) ;
		if (r < 0)
		{
			VERBO3 strerr_warnwu3sys("switch svc service for: ",info->treename," to source") ;
		}
		VERBO3 strerr_warnt2x("make a backup of svc service for: ",info->treename) ;
		if (!backup_make_new(info,CLASSIC))
		{
			VERBO3 strerr_warnwu2sys("make a backup of svc service for: ",info->treename) ;
			return 0 ;
		}
	}
	return 1 ;
}
