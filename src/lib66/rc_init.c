/* 
 * rc_send.c
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

#include <66/rc.h>

#include <string.h>
#include <stdlib.h>

#include <oblibs/error2.h>

#include <skalibs/genalloc.h>

#include <66/resolve.h>
#include <66/ssexec.h>
#include <66/utils.h>
#include <s6/s6-supervise.h>

int rc_init(ssexec_t *info, char const *const *envp)
{
	int r ;
	
	int nargc = 4 ;
	char const *newargv[nargc] ;
	unsigned int m = 0 ;
	
	newargv[m++] = "fake_name" ;
	newargv[m++] = "-d" ;
	newargv[m++] = info->treename.s ;
	newargv[m++] = 0 ;
				
	if (ssexec_init(nargc,newargv,envp,info))
		return 0 ;
	
	
	VERBO3 strerr_warnt2x("reload scandir: ",info->scandir.s) ;
	r = s6_svc_writectl(info->scandir.s, S6_SVSCAN_CTLDIR, "an", 2) ;
	if (r < 0) return 0 ;
	
	return 1 ;
}
