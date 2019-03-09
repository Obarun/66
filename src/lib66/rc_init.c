/* 
 * rc_send.c
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

#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>

#include <66/resolve.h>
#include <66/ssexec.h>
#include <66/constants.h>
#include <66/utils.h>

#include <s6-rc/config.h>

int rc_init(ssexec_t *info,genalloc *ga, char const *const *envp)
{
	int writein,wstat ;
	pid_t pid ;
	
	stralloc src = STRALLOC_ZERO ;
	
	if (!access(info->tree.s,W_OK)) writein = SS_DOUBLE ;
	else writein = SS_SIMPLE ;
	
	char svdir[info->tree.len + SS_SVDIRS_LEN + SS_DB_LEN + 1 + info->treename.len + 1] ;
	memcpy(svdir,info->tree.s,info->tree.len) ;
	memcpy(svdir + info->tree.len ,SS_SVDIRS ,SS_SVDIRS_LEN) ;
	memcpy(svdir + info->tree.len + SS_SVDIRS_LEN,SS_DB,SS_DB_LEN) ;
	memcpy(svdir + info->tree.len + SS_SVDIRS_LEN + SS_DB_LEN, "/", 1) ;
	memcpy(svdir + info->tree.len + SS_SVDIRS_LEN + SS_DB_LEN + 1, info->treename.s,info->treename.len) ;
	svdir[info->tree.len + SS_SVDIRS_LEN + SS_DB_LEN + 1 + info->treename.len] = 0 ;	
	
	char ltree[info->livetree.len + 1 + info->treename.len + 1] ;
	memcpy(ltree,info->livetree.s,info->livetree.len) ;
	ltree[info->livetree.len] = '/' ;
	memcpy(ltree + info->livetree.len + 1, info->treename.s, info->treename.len) ;
	ltree[info->livetree.len + 1 + info->treename.len] = 0 ;
		
	char prefix[info->treename.len + 1 + 1] ;
	memcpy(prefix,info->treename.s,info->treename.len) ;
	memcpy(prefix + info->treename.len, "-",1) ;
	prefix[info->treename.len + 1] = 0 ;
		
	char tt[UINT32_FMT] ;
	tt[uint32_fmt(tt,info->timeout)] = 0 ;
			
	char const *newargv[12] ;
	unsigned int m = 0 ;
			
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
				
	if (wstat) { strerr_warnwu2x("init db of tree: ",info->treename.s) ; goto err ; }
	
	if (!ss_resolve_pointo(&src,info,SS_NOTYPE,SS_RESOLVE_LIVE))		
		{ strerr_warnwu1x("set revolve pointer to live") ; goto err ; }
		
	for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,ga) ; i++)
	{
		char const *string = genalloc_s(ss_resolve_t,ga)[i].sa.s ;
		char const *name = string + genalloc_s(ss_resolve_t,ga)[i].name  ;
		ss_resolve_setflag(&genalloc_s(ss_resolve_t,ga)[i],SS_FLAGS_INIT,SS_FLAGS_FALSE) ;
		VERBO2 strerr_warni2x("Write resolve file of: ",name) ;
		if (!ss_resolve_write(&genalloc_s(ss_resolve_t,ga)[i],src.s,name,writein))
			{ VERBO1 strerr_diefu2sys(111,"write resolve file of: ",name) ; goto err ; }
			
		VERBO1 strerr_warni2x("Initiated successfully: ",name) ;
	}
	
	VERBO2 strerr_warnt2x("reload scandir: ",info->scandir.s) ;
	if (scandir_send_signal(info->scandir.s,"an") <= 0) goto err ;
	
	stralloc_free(&src) ;
	return 1 ;
	
	err:
		stralloc_free(&src) ;
		return 0 ;
}
