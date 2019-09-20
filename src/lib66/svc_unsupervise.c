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

#include <oblibs/error2.h>

#include <skalibs/genalloc.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/utils.h>
#include <66/resolve.h>
#include <66/ssexec.h>
#include <66/state.h>

int svc_unsupervise(ssexec_t *info,genalloc *ga,char const *sig,char const *const *envp)
{
	size_t i = 0 ;
	ss_state_t sta = STATE_ZERO ;
	ss_resolve_t_ref pres ;
	stralloc sares = STRALLOC_ZERO ;
	
	if (!svc_send(info,ga,sig,envp))
	{
		VERBO1 strerr_warnwu1x("stop services") ;
		goto err ;
	}
	
	for (; i < genalloc_len(ss_resolve_t,ga) ; i++) 
	{
		char const *string = genalloc_s(ss_resolve_t,ga)[i].sa.s ;
		VERBO2 strerr_warni2x("delete directory service: ",string + genalloc_s(ss_resolve_t,ga)[i].runat) ;
		if (rm_rf(string + genalloc_s(ss_resolve_t,ga)[i].runat) < 0)
		{
			VERBO1 strerr_warnwu2sys("delete: ",string + genalloc_s(ss_resolve_t,ga)[i].runat) ;
			goto err ;
		}
	}
	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC))
	{
		strerr_warnwu1sys("set revolve pointer to source") ;
		goto err ;
	}
	for (i = 0 ; i < genalloc_len(ss_resolve_t,ga) ; i++)
	{
		pres = &genalloc_s(ss_resolve_t,ga)[i] ;
		char const *string = pres->sa.s ;
		char const *name = string + pres->name  ;
		char const *state = string + pres->state ;
		// remove the resolve/state file if the service is disabled
		if (!pres->disen)
		{				
			VERBO2 strerr_warni2x("Delete resolve file of: ",name) ;
			ss_resolve_rmfile(sares.s,name) ;
			VERBO2 strerr_warni2x("Delete state file of: ",name) ;
			ss_state_rmfile(state,name) ;
		}
		else
		{
			ss_state_setflag(&sta,SS_FLAGS_RELOAD,SS_FLAGS_FALSE) ;
			ss_state_setflag(&sta,SS_FLAGS_INIT,SS_FLAGS_TRUE) ;
	//		ss_state_setflag(&sta,SS_FLAGS_UNSUPERVISE,SS_FLAGS_FALSE) ;
			ss_state_setflag(&sta,SS_FLAGS_STATE,SS_FLAGS_FALSE) ;
			ss_state_setflag(&sta,SS_FLAGS_PID,SS_FLAGS_FALSE) ;
			VERBO2 strerr_warni2x("Write state file of: ",name) ;
			if (!ss_state_write(&sta,state,name))
			{
				VERBO1 strerr_warnwu2sys("write state file of: ",name) ;
				goto err ;
			}
		}
		VERBO1 strerr_warni2x("Unsupervised successfully: ",name) ;
	}
	stralloc_free(&sares) ;
	return 1 ;
	err:
		stralloc_free(&sares) ;
		return 0 ;
}

