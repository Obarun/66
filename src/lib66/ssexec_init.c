/* 
 * ssexec_init.c
 * 
 * Copyright (c) 2018-2020 Eric Vidal <eric@obarun.org>
 * 
 * All rights reserved.
 * 
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */
 
#include <string.h>
#include <sys/types.h>
#include <unistd.h>//chown
#include <stdio.h>

#include <oblibs/log.h>
#include <oblibs/types.h>//scan_mode
#include <oblibs/directory.h>
#include <oblibs/sastr.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>

#include <66/utils.h>
#include <66/constants.h>
#include <66/tree.h>
#include <66/db.h>
#include <66/svc.h>
#include <66/resolve.h>
#include <66/ssexec.h>
#include <66/rc.h>
#include <66/state.h>

int ssexec_init(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
	int r, db, classic, earlier ;
	ssize_t i = 0, logname = 0 ;
	genalloc gares = GENALLOC_ZERO ; //ss_resolve_t type
	stralloc sares = STRALLOC_ZERO ;
	stralloc sasvc = STRALLOC_ZERO ;
	ss_resolve_t res = RESOLVE_ZERO ;
	ss_state_t sta = STATE_ZERO ;
	
	classic = db = earlier = 0 ;
	
	gid_t gidowner ;
	if (!yourgid(&gidowner,info->owner)) log_dieusys(LOG_EXIT_SYS,"set gid") ;
	
	if (argc != 2) log_usage(usage_init) ;
	if (*argv[1] == 'c') classic = 1 ;
	else if (*argv[1] == 'd') db = 1 ;
	else if (*argv[1] == 'b') classic = db = 1 ;
	else log_die(LOG_EXIT_USER,"uknow command: ",argv[1]) ;
	
	if (!tree_get_permissions(info->tree.s,info->owner))
		log_die(LOG_EXIT_USER,"You're not allowed to use the tree: ",info->tree.s) ;
		
	r = scan_mode(info->scandir.s,S_IFDIR) ;
	if (r < 0) log_die(LOG_EXIT_SYS,info->scandir.s," conflicted format") ;
	if (!r) log_die(LOG_EXIT_USER,"scandir: ",info->scandir.s," doesn't exist") ;
		
	r = scandir_ok(info->scandir.s) ;
	if (r != 1) earlier = 1 ; 

	r = scan_mode(info->livetree.s,S_IFDIR) ;
	if (r < 0) log_die(LOG_EXIT_SYS,info->livetree.s," conflicted format") ;
	if (!r)
	{
		log_trace("create directory: ",info->livetree.s) ;
		r = dir_create(info->livetree.s,0700) ;
		if (!r) log_dieusys(LOG_EXIT_SYS,"create directory: ",info->livetree.s) ;
		log_trace("chown directory: ",info->livetree.s) ;
		if (chown(info->livetree.s,info->owner,gidowner) < 0) log_dieusys(LOG_EXIT_SYS,"chown directory: ",info->livetree.s) ;
	}
	
	size_t dirlen ;
	char svdir[info->tree.len + SS_SVDIRS_LEN + SS_SVC_LEN + 1] ;
	memcpy(svdir,info->tree.s,info->tree.len) ;
	memcpy(svdir + info->tree.len ,SS_SVDIRS ,SS_SVDIRS_LEN) ;
	memcpy(svdir + info->tree.len + SS_SVDIRS_LEN, SS_SVC ,SS_SVC_LEN) ;
	dirlen = info->tree.len + SS_SVDIRS_LEN + SS_SVC_LEN ;
	svdir[dirlen] = 0 ;
	
	/** svc already initiated? */
	if (classic)
	{
		if (!sastr_dir_get(&sasvc,svdir,"",S_IFDIR)) log_dieusys(LOG_EXIT_SYS,"get classic services from: ",svdir) ;
		if (!sasvc.len)
		{
			log_info("Initialization aborted -- no classic services into tree: ",info->treename.s) ;
			goto follow ;
		}
		
		if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC)) 
			log_dieu(LOG_EXIT_SYS,"set revolve pointer to source") ;
		
		for (i = 0;i < sasvc.len; i += strlen(sasvc.s + i) + 1)
		{
			char *name = sasvc.s + i ;
			ss_resolve_t tmp = RESOLVE_ZERO ;
			if (!ss_resolve_check(sares.s,name)) log_diesys(LOG_EXIT_USER,"unknown service: ",name) ;
			if (!ss_resolve_read(&tmp,sares.s,name)) log_dieusys(LOG_EXIT_SYS,"read resolve file of: ",name) ;
			if (!ss_resolve_add_deps(&gares,&tmp,sares.s)) log_dieusys(LOG_EXIT_SYS,"resolve dependencies of: ",name) ;	
			ss_resolve_free(&tmp) ;
		}
		
		if (!earlier)
		{
			/** reverse to start first the logger */
			genalloc_reverse(ss_resolve_t,&gares) ;
			if (!svc_init(info,svdir,&gares)) log_dieu(LOG_EXIT_SYS,"initiate service of tree: ",info->treename.s) ;
		}
		else
		{
			if (!ss_resolve_create_live(info)) log_dieusys(LOG_EXIT_SYS,"create live state") ;
			for (i = 0 ; i < genalloc_len(ss_resolve_t,&gares) ; i++)
			{
				logname = 0 ;
				char *string = genalloc_s(ss_resolve_t,&gares)[i].sa.s ;
				char *name = string + genalloc_s(ss_resolve_t,&gares)[i].name ;
				size_t namelen = strlen(name) ;
				logname = get_rstrlen_until(name,SS_LOG_SUFFIX) ;
				if (logname > 0) name = string + genalloc_s(ss_resolve_t,&gares)[i].logassoc ;
				char tocopy[dirlen + 1 + namelen + 1] ;
				memcpy(tocopy,svdir,dirlen) ;
				tocopy[dirlen] = '/' ;
				memcpy(tocopy + dirlen + 1, name, namelen) ;
				tocopy[dirlen + 1 + namelen] = 0 ;
				if (!hiercopy(tocopy,string + genalloc_s(ss_resolve_t,&gares)[i].runat)) log_dieusys(LOG_EXIT_SYS,"copy earlier service: ",tocopy," to: ",string + genalloc_s(ss_resolve_t,&gares)[i].runat) ;
				ss_state_setflag(&sta,SS_FLAGS_RELOAD,SS_FLAGS_FALSE) ;
				ss_state_setflag(&sta,SS_FLAGS_INIT,SS_FLAGS_FALSE) ;
	//			ss_state_setflag(&sta,SS_FLAGS_UNSUPERVISE,SS_FLAGS_FALSE) ;
				ss_state_setflag(&sta,SS_FLAGS_STATE,SS_FLAGS_UNKNOWN) ;
				ss_state_setflag(&sta,SS_FLAGS_PID,SS_FLAGS_UNKNOWN) ;
				if (!ss_state_write(&sta,string + genalloc_s(ss_resolve_t,&gares)[i].state,name)) log_dieusys(LOG_EXIT_SYS,"write state file of: ",name) ;
				log_info("Initialized successfully: ", logname < 0 ? name : string + genalloc_s(ss_resolve_t,&gares)[i].logreal) ;
			}
		}
	}

	follow:

	stralloc_free(&sares) ;
	stralloc_free(&sasvc) ;
	ss_resolve_free(&res) ;
	genalloc_deepfree(ss_resolve_t,&gares,ss_resolve_free) ;
		
	/** db already initiated? */
	if (db)
	{
		if (!earlier)
		{
			if (db_ok(info->livetree.s,info->treename.s))
			{
				log_warn("db of tree: ",info->treename.s," already initialized") ;
				goto end ;
			}
		}else log_die(LOG_EXIT_USER,"scandir: ",info->scandir.s," is not running") ;
	}else goto end ;
	
	if (!rc_init(info,envp)) log_dieusys(LOG_EXIT_SYS,"initiate db of tree: ",info->treename.s) ;
	
	end:
	return 0 ;
}
