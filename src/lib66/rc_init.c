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
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>
#include <oblibs/sastr.h>

#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/types.h>

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
	size_t pos = 0 ;
	
	ss_state_t sta = STATE_ZERO ;
	stralloc sares = STRALLOC_ZERO ;
	ss_resolve_t res = RESOLVE_ZERO ;
	stralloc sasvc = STRALLOC_ZERO ;
	genalloc gares = GENALLOC_ZERO ; //ss_resolve_t type
	
	char svdir[info->tree.len + SS_SVDIRS_LEN + SS_DB_LEN + 1 + info->treename.len + 1] ;
	char ltree[info->livetree.len + 1 + info->treename.len + 1] ;
	char prefix[info->treename.len + 1 + 1] ;
	char tt[UINT32_FMT] ;
	char const *newargv[12] ;
	unsigned int m = 0 ;
	
	gid_t gidowner ;
	if (!yourgid(&gidowner,info->owner)){ log_warnusys("set gid") ; goto err ; }
	
	r = scan_mode(info->livetree.s,S_IFDIR) ;
	if (r < 0){ log_warn(info->livetree.s," conflicted format") ; goto err ; }
	if (!r)
	{
		log_trace("create directory: ",info->livetree.s) ;
		r = dir_create(info->livetree.s,0700) ;
		if (!r){ log_warnusys("create directory: ",info->livetree.s) ; goto err ; }
		log_trace("chown directory: ",info->livetree.s) ;
		if (chown(info->livetree.s,info->owner,gidowner) < 0){ log_warnusys("chown directory: ",info->livetree.s) ; goto err ; }
	}
	
	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC))		
		{ log_warnu("set revolve pointer to source") ; goto err ; }
	
	if (!ss_resolve_check(sares.s,SS_MASTER +1)) { log_warnu("find inner bundle -- please make a bug report") ; goto err ; }
	if (!ss_resolve_read(&res,sares.s,SS_MASTER + 1)) { log_warnusys("read resolve file of inner bundle") ; goto err ; }
	if (!res.ndeps)
	{
		log_info("Initialization aborted -- no atomic services into tree: ",info->treename.s) ;
		empty = 1 ;
		goto end ;
	}
	if (!ss_resolve_create_live(info)) { log_warnusys("create live state") ; goto err ; }
	
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
			
	log_trace("initiate db of tree: ",info->treename.s," ...") ;
			
	pid = child_spawn0(newargv[0],newargv,envp) ;
	if (waitpid_nointr(pid,&wstat, 0) < 0) 
		{ log_warnusys("wait for ",newargv[0]) ; goto err ; }
				
	if (wstat) { log_warnu("init db of tree: ",info->treename.s) ; goto err ; }
	
	if (!sastr_clean_string(&sasvc,res.sa.s + res.deps)) { log_warnusys("clean dependencies of inner bundle") ; goto err ; }
	
	for (; pos < sasvc.len ; pos += strlen(sasvc.s + pos) +1)
	{
		char *name = sasvc.s + pos ;
		ss_resolve_t tmp = RESOLVE_ZERO ;
		if (!ss_resolve_check(sares.s,name)){ log_warnsys("unknown service: ",name) ; goto err ; }
		if (!ss_resolve_read(&tmp,sares.s,name)) { log_warnusys("read resolve file of: ",name) ; goto err ; }
		if (!ss_resolve_add_deps(&gares,&tmp,sares.s)) { log_warnusys("resolve dependencies of: ",name) ; goto err ; }
		ss_resolve_free(&tmp) ;
	}

	for (pos = 0 ; pos < genalloc_len(ss_resolve_t,&gares) ; pos++)
	{
		char const *string = genalloc_s(ss_resolve_t,&gares)[pos].sa.s ;
		char const *name = string + genalloc_s(ss_resolve_t,&gares)[pos].name  ;
		char const *state = string + genalloc_s(ss_resolve_t,&gares)[pos].state  ;
		
		log_trace("Write state file of: ",name) ;
		if (!ss_state_write(&sta,state,name))
		{
			log_warnusys("write state file of: ",name) ;
			goto err ;
		}
		log_info("Initialized successfully: ",name) ;
	}
	
	end:
	genalloc_deepfree(ss_resolve_t,&gares,ss_resolve_free) ;
	stralloc_free(&sasvc) ;
	ss_resolve_free(&res) ;
	stralloc_free(&sares) ;
	return empty ? 2 : 1 ;
	
	err:
		genalloc_deepfree(ss_resolve_t,&gares,ss_resolve_free) ;
		stralloc_free(&sasvc) ;
		ss_resolve_free(&res) ;
		stralloc_free(&sares) ;
		return 0 ;
}
