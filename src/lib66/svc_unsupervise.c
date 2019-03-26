/* 
 * svc_unsupervise.c
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

#include <66/svc.h>

#include <string.h>
#include <unistd.h>//access

#include <oblibs/error2.h>

#include <skalibs/genalloc.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/utils.h>
#include <66/resolve.h>
#include <66/ssexec.h>

int svc_unsupervise(ssexec_t *info,genalloc *ga,char const *sig,char const *const *envp)
{
	int writein ;
	
	ss_resolve_t_ref pres ;
	stralloc sares = STRALLOC_ZERO ;
	
	if (!access(info->tree.s,W_OK)) writein = SS_DOUBLE ;
	else writein = SS_SIMPLE ;
	
	if (!svc_send(info,ga,sig,envp))
	{
		VERBO1 strerr_warnwu1x("stop services") ;
		goto err ;
	}
	
	for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,ga) ; i++) 
	{
		char const *string = genalloc_s(ss_resolve_t,ga)[i].sa.s ;
		VERBO2 strerr_warni2x("delete directory service: ",string + genalloc_s(ss_resolve_t,ga)[i].runat) ;
		if (rm_rf(string + genalloc_s(ss_resolve_t,ga)[i].runat) < 0)
		{
			VERBO1 strerr_warnwu2sys("delete: ",string + genalloc_s(ss_resolve_t,ga)[i].runat) ;
			goto err ;
		}
	}
	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_LIVE))
	{
		strerr_warnwu1sys("set revolve pointer to live") ;
		goto err ;
	}
	for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,ga) ; i++)
	{
		pres = &genalloc_s(ss_resolve_t,ga)[i] ;
		char const *string = pres->sa.s ;
		char const *name = string + pres->name  ;
		// do not remove the resolve file if the daemon was not disabled
		if (pres->disen)
		{				
			ss_resolve_setflag(pres,SS_FLAGS_INIT,SS_FLAGS_TRUE) ;
			ss_resolve_setflag(pres,SS_FLAGS_RUN,SS_FLAGS_FALSE) ;
			ss_resolve_setflag(pres,SS_FLAGS_RELOAD,SS_FLAGS_FALSE) ;
			ss_resolve_setflag(pres,SS_FLAGS_UNSUPERVISE,SS_FLAGS_FALSE) ;
			VERBO2 strerr_warni2x("Write resolve file of: ",name) ;
			if (!ss_resolve_write(pres,sares.s,name,writein))
			{
				VERBO1 strerr_warnwu2sys("write resolve file of: ",name) ;
				goto err ;
			}
			VERBO1 strerr_warni2x("Unsupervised successfully: ",name) ;
			continue ;
		}
		VERBO2 strerr_warni2x("Delete resolve file of: ",name) ;
		if (!ss_resolve_rmfile(pres,sares.s,name,writein))
		{
			VERBO1 strerr_warnwu2sys("delete resolve file of: ",name) ;
			goto err ;
		}
		VERBO1 strerr_warni2x("Unsupervised successfully: ",name) ;
	}
	stralloc_free(&sares) ;
	return 1 ;
	err:
		stralloc_free(&sares) ;
		return 0 ;
}

