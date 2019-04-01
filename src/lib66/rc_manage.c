/* 
 * rc_manage.c
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

#include <66/rc.h>

#include <string.h>

#include <oblibs/error2.h>

#include <skalibs/tai.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>

#include <66/ssexec.h>
#include <66/utils.h>
#include <66/resolve.h>
#include <66/state.h>
#include <66/constants.h>
#include <66/db.h>

#include <s6-rc/s6rc-servicedir.h>

/**@Return 1 on success
 * @Return 2 on empty database
 * @Return 0 on fail */
int rc_manage(ssexec_t *info,genalloc *ga)
{
	int r ;
	ss_state_t sta = STATE_ZERO ;
	stralloc sares = STRALLOC_ZERO ;
	size_t newlen ;
	
	tain_t deadline ;
	if (info->timeout) tain_from_millisecs(&deadline, info->timeout) ;
	else deadline = tain_infinite_relative ;
	tain_now_g() ;
    tain_add_g(&deadline, &deadline) ;
    
    char prefix[info->treename.len + 2] ;
	memcpy(prefix,info->treename.s,info->treename.len) ;
	memcpy(prefix + info->treename.len,"-",1) ;
	prefix[info->treename.len + 1] = 0 ;
	
	char live[info->livetree.len + 1 + info->treename.len + 1 ];
	memcpy(live,info->livetree.s,info->livetree.len) ;
	live[info->livetree.len] = '/' ;
	memcpy(live + info->livetree.len + 1,info->treename.s,info->treename.len) ;
	live[info->livetree.len + 1 + info->treename.len] = 0 ;
	
	if (!ss_resolve_pointo(&sares,info,LONGRUN,SS_RESOLVE_SRC))
	{
		strerr_warnwu1sys("set revolve pointer to source") ;
		goto err ;
	}
	sares.len--;
	if (!stralloc_cats(&sares,"/")) goto err ;
	if (!stralloc_cats(&sares,info->treename.s)) goto err ;
	if (!stralloc_cats(&sares,"/")) goto err ;
	if (!stralloc_cats(&sares,SS_SVDIRS)) goto err ;
	if (!stralloc_cats(&sares,"/")) goto err ;
	newlen = sares.len ;
	for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,ga) ; i++)
	{
		char const *string = genalloc_s(ss_resolve_t,ga)[i].sa.s ;
		char const *name = string + genalloc_s(ss_resolve_t,ga)[i].name  ;
		char const *runat = string + genalloc_s(ss_resolve_t,ga)[i].runat ;
		int type = genalloc_s(ss_resolve_t,ga)[i].type ;
		//do not try to copy a bundle or oneshot, this is already done.
		if (type != LONGRUN) continue ;
		sares.len = newlen ;
		if (!stralloc_cats(&sares,name)) goto err ;
		if (!stralloc_0(&sares)) goto err ;
		if (!hiercopy(sares.s,runat)) goto err ;
	}
	/** do not really init the service if you come from backup,
	 * s6-rc-update will do the manage proccess for us. If we pass through
	 * here a double copy of the same service is made and s6-rc-update  will fail. */
	r = db_find_compiled_state(info->livetree.s,info->treename.s) ;
	if (!r)
	{
		r = s6rc_servicedir_manage_g(live, prefix, &deadline) ;
		if (!r)
		{
			VERBO1 strerr_warnwu3sys("supervise service directories in ", live, "/servicedirs") ;
			goto err ;
		}
	}
	for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,ga) ; i++)
	{
		char const *string = genalloc_s(ss_resolve_t,ga)[i].sa.s ;
		char const *name = string + genalloc_s(ss_resolve_t,ga)[i].name  ;
		char const *state = string + genalloc_s(ss_resolve_t,ga)[i].state ;
		VERBO2 strerr_warni2x("Write state file of: ",name) ;
		if (!ss_state_write(&sta,state,name))
		{
			VERBO1 strerr_warnwu2sys("write state file of: ",name) ;
			goto err ;
		}
		VERBO1 strerr_warni2x("Initialized successfully: ",name) ;
	}
	
	stralloc_free(&sares) ;
	return 1 ;
	err:
		stralloc_free(&sares) ;
		return 0 ;
}

