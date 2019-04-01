/* 
 * rc_init.c
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
#include <stdlib.h>

#include <oblibs/error2.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>

#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>

#include <66/resolve.h>
#include <66/ssexec.h>
#include <66/constants.h>
#include <66/utils.h>
#include <66/state.h>

#include <s6-rc/config.h>

/**@Return 1 on success
 * @Return 2 on empty database
 * @Return 0 on fail */
int rc_init(ssexec_t *info, char const *const *envp)
{
	int r, wstat, empty = 0 ;
	pid_t pid ;
	
	ss_state_t sta = STATE_ZERO ;
	stralloc sares = STRALLOC_ZERO ;
	ss_resolve_t res = RESOLVE_ZERO ;
	genalloc gasvc = GENALLOC_ZERO ; //stralist type
	genalloc gares = GENALLOC_ZERO ; //ss_resolve_t type
	
	char svdir[info->tree.len + SS_SVDIRS_LEN + SS_DB_LEN + 1 + info->treename.len + 1] ;
	char ltree[info->livetree.len + 1 + info->treename.len + 1] ;
	char prefix[info->treename.len + 1 + 1] ;
	char tt[UINT32_FMT] ;
	char const *newargv[12] ;
	unsigned int m = 0 ;
	
	gid_t gidowner ;
	if (!yourgid(&gidowner,info->owner)){ VERBO1 strerr_warnwu1sys("set gid") ; goto err ; }
	
	r = scan_mode(info->livetree.s,S_IFDIR) ;
	if (r < 0){ VERBO1 strerr_warnw2x(info->livetree.s," conflicted format") ; goto err ; }
	if (!r)
	{
		VERBO2 strerr_warni2x("create directory: ",info->livetree.s) ;
		r = dir_create(info->livetree.s,0700) ;
		if (!r){ VERBO1 strerr_warnwu2sys("create directory: ",info->livetree.s) ; goto err ; }
		VERBO2 strerr_warni2x("chown directory: ",info->livetree.s) ;
		if (chown(info->livetree.s,info->owner,gidowner) < 0){ VERBO1 strerr_warnwu2sys("chown directory: ",info->livetree.s) ; goto err ; }
	}
	
	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC))		
		{ VERBO1 strerr_warnwu1x("set revolve pointer to source") ; goto err ; }
	
	if (!ss_resolve_check(sares.s,SS_MASTER +1)) { VERBO1 strerr_warnwu1x("find inner bundle -- please make a bug report") ; goto err ; }
	if (!ss_resolve_read(&res,sares.s,SS_MASTER + 1)) { VERBO1 strerr_warnwu1sys("read resolve file of inner bundle") ; goto err ; }
	if (!res.ndeps)
	{
		VERBO1 strerr_warni2x("Initialization aborted -- no atomic services into tree: ",info->treename.s) ;
		empty = 1 ;
		goto end ;
	}
	if (!ss_resolve_create_live(info)) { VERBO1 strerr_warnwu1sys("create live state") ; goto err ; }
	
	memcpy(svdir,info->tree.s,info->tree.len) ;
	memcpy(svdir + info->tree.len ,SS_SVDIRS ,SS_SVDIRS_LEN) ;
	memcpy(svdir + info->tree.len + SS_SVDIRS_LEN,SS_DB,SS_DB_LEN) ;
	memcpy(svdir + info->tree.len + SS_SVDIRS_LEN + SS_DB_LEN, "/", 1) ;
	memcpy(svdir + info->tree.len + SS_SVDIRS_LEN + SS_DB_LEN + 1, info->treename.s,info->treename.len) ;
	svdir[info->tree.len + SS_SVDIRS_LEN + SS_DB_LEN + 1 + info->treename.len] = 0 ;	
		
	memcpy(ltree,info->livetree.s,info->livetree.len) ;
	ltree[info->livetree.len] = '/' ;
	memcpy(ltree + info->livetree.len + 1, info->treename.s, info->treename.len) ;
	ltree[info->livetree.len + 1 + info->treename.len] = 0 ;
		
	memcpy(prefix,info->treename.s,info->treename.len) ;
	memcpy(prefix + info->treename.len, "-",1) ;
	prefix[info->treename.len + 1] = 0 ;
		
	tt[uint32_fmt(tt,info->timeout)] = 0 ;
			
	newargv[m++] = S6RC_BINPREFIX "s6-rc-init" ;
	newargv[m++] = "-l" ;
	newargv[m++] = ltree ;
	newargv[m++] = "-c" ;
	newargv[m++] = svdir ;
	newargv[m++] = "-p" ;
	newargv[m++] = prefix ;
	newargv[m++] = "-t" ;
	newargv[m++] = tt ;
	newargv[m++] = "--" ;
	newargv[m++] = info->scandir.s ;
	newargv[m++] = 0 ;
			
	VERBO3 strerr_warni3x("initiate db of tree: ",info->treename.s," ...") ;
			
	pid = child_spawn0(newargv[0],newargv,envp) ;
	if (waitpid_nointr(pid,&wstat, 0) < 0) 
		{ strerr_warnwu2sys("wait for ",newargv[0]) ; goto err ; }
				
	if (wstat) { VERBO1 strerr_warnwu2x("init db of tree: ",info->treename.s) ; goto err ; }
	
	if (!clean_val(&gasvc,res.sa.s + res.deps)) { VERBO1 strerr_warnwu1sys("clean dependencies of inner bundle") ; goto err ; }
	
	for (unsigned int i = 0 ; i < genalloc_len(stralist,&gasvc) ; i++)
	{
		char *name = gaistr(&gasvc,i) ;
		ss_resolve_t tmp = RESOLVE_ZERO ;
		if (!ss_resolve_check(sares.s,name)){ VERBO1 strerr_warnw2sys("unknown service: ",name) ; goto err ; }
		if (!ss_resolve_read(&tmp,sares.s,name)) { VERBO1 strerr_warnwu2sys("read resolve file of: ",name) ; goto err ; }
		if (!ss_resolve_add_deps(&gares,&tmp,sares.s)) { VERBO1 strerr_warnwu2sys("resolve dependencies of: ",name) ; goto err ; }
		ss_resolve_free(&tmp) ;
	}
	
	for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,&gares) ; i++)
	{
		char const *string = genalloc_s(ss_resolve_t,&gares)[i].sa.s ;
		char const *name = string + genalloc_s(ss_resolve_t,&gares)[i].name  ;
		char const *state = string + genalloc_s(ss_resolve_t,&gares)[i].state  ;
		
		VERBO2 strerr_warni2x("Write state file of: ",name) ;
		if (!ss_state_write(&sta,state,name))
		{
			VERBO1 strerr_warnwu2sys("write state file of: ",name) ;
			goto err ;
		}
		VERBO1 strerr_warni2x("Initialized successfully: ",name) ;
	}
	
	/*
	VERBO2 strerr_warnt2x("reload scandir: ",info->scandir.s) ;
	if (scandir_send_signal(info->scandir.s,"an") <= 0) goto err ;
	*/
	end:
	genalloc_deepfree(ss_resolve_t,&gares,ss_resolve_free) ;
	genalloc_deepfree(stralist,&gasvc,stra_free) ;
	ss_resolve_free(&res) ;
	stralloc_free(&sares) ;
	return empty ? 2 : 1 ;
	
	err:
		genalloc_deepfree(ss_resolve_t,&gares,ss_resolve_free) ;
		genalloc_deepfree(stralist,&gasvc,stra_free) ;
		ss_resolve_free(&res) ;
		stralloc_free(&sares) ;
		return 0 ;
}
