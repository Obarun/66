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

#include <skalibs/stralloc.h>

#include <66/backup.h>
#include <66/utils.h>
#include <66/enum.h>

/** 1-> backup
 * 0-> ori */
int svc_switch_to(char const *base, char const *tree,char const *treename,unsigned int where)
{
	int r ;
	
	stralloc sv = STRALLOC_ZERO ;
	
	r = backup_cmd_switcher(VERBOSITY,"-t30 -b",treename) ;
	if (r < 0)
	{
		VERBO3 strerr_warnwu2sys("find origin of svc service for: ",treename) ;
		goto err ;
	}
	// point to origin
	if (!r && where)
	{
		VERBO3 strerr_warnt2x("make a backup of svc service for: ",treename) ;
		if (!backup_make_new(base,tree,treename,CLASSIC))
		{
			VERBO3 strerr_warnwu2sys("make a backup of svc service for: ",treename) ;
			goto err ;
		}
		VERBO3 strerr_warnt3x("switch svc service for tree: ",treename," to backup") ;
		r = backup_cmd_switcher(VERBOSITY,"-t30 -s1",treename) ;
		if (r < 0)
		{
			VERBO3 strerr_warnwu3sys("switch svc service for: ",treename," to backup") ;
		}
	}
	else if (r > 0 && !where)
	{
		VERBO3 strerr_warnt3x("switch svc service for tree: ",treename," to source") ;
		r = backup_cmd_switcher(VERBOSITY,"-t30 -s0",treename) ;
		if (r < 0)
		{
			VERBO3 strerr_warnwu3sys("switch svc service for: ",treename," to source") ;
		}
		VERBO3 strerr_warnt2x("make a backup of svc service for: ",treename) ;
		if (!backup_make_new(base,tree,treename,CLASSIC))
		{
			VERBO3 strerr_warnwu2sys("make a backup of svc service for: ",treename) ;
			goto err ;
		}
	}
	return 1 ;
	
	err:
		stralloc_free(&sv) ;
		return 0 ;
}
