/* 
 * ssexec_init.c
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
 
#include <string.h>
#include <sys/types.h>
#include <unistd.h>//chown
//#include <stdio.h>

#include <oblibs/error2.h>
#include <oblibs/types.h>//scan_mode
#include <oblibs/stralist.h>
#include <oblibs/directory.h>

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
	
	genalloc gasvc = GENALLOC_ZERO ; //stralist type
	genalloc gares = GENALLOC_ZERO ; //ss_resolve_t type
	
	stralloc sares = STRALLOC_ZERO ;
	ss_resolve_t res = RESOLVE_ZERO ;
	ss_state_t sta = STATE_ZERO ;
	
	classic = db = earlier = 0 ;
	
	gid_t gidowner ;
	if (!yourgid(&gidowner,info->owner)) strerr_diefu1sys(111,"set gid") ;
	
	if (argc != 2) exitusage(usage_init) ;
	if (*argv[1] == 'c') classic = 1 ;
	else if (*argv[1] == 'd') db = 1 ;
	else if (*argv[1] == 'b') classic = db = 1 ;
	else strerr_dief2x(110,"uknow command: ",argv[1]) ;
	
	if (!tree_get_permissions(info->tree.s,info->owner))
		strerr_dief2x(110,"You're not allowed to use the tree: ",info->tree.s) ;
		
	r = scan_mode(info->scandir.s,S_IFDIR) ;
	if (r < 0) strerr_dief2x(111,info->scandir.s," conflicted format") ;
	if (!r) strerr_dief3x(110,"scandir: ",info->scandir.s," doesn't exist") ;
		
	r = scandir_ok(info->scandir.s) ;
	if (r != 1) earlier = 1 ; 
	
	r = scan_mode(info->livetree.s,S_IFDIR) ;
	if (r < 0) strerr_dief2x(111,info->livetree.s," conflicted format") ;
	if (!r)
	{
		VERBO2 strerr_warni2x("create directory: ",info->livetree.s) ;
		r = dir_create(info->livetree.s,0700) ;
		if (!r) strerr_diefu2sys(111,"create directory: ",info->livetree.s) ;
		VERBO2 strerr_warni2x("chown directory: ",info->livetree.s) ;
		if (chown(info->livetree.s,info->owner,gidowner) < 0) strerr_diefu2sys(111,"chown directory: ",info->livetree.s) ;
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
		size_t i ;
		if (!dir_get(&gasvc,svdir,"",S_IFDIR)) strerr_diefu1x(111,"get classic services") ;
		if (!genalloc_len(stralist,&gasvc))
		{
			VERBO1 strerr_warni2x("Initialization aborted -- no classic services into tree: ",info->treename.s) ;
			genalloc_deepfree(ss_resolve_t,&gares,ss_resolve_free) ;
			genalloc_deepfree(stralist,&gasvc,stra_free) ;
			goto follow ;
		}
		
		if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC)) 
			strerr_diefu1x(111,"set revolve pointer to source") ;
		
		for (i = 0 ; i < genalloc_len(stralist,&gasvc) ; i++)
		{
			char *name = gaistr(&gasvc,i) ;
			ss_resolve_t tmp = RESOLVE_ZERO ;
			if (!ss_resolve_check(sares.s,name)) strerr_dief2sys(110,"unknown service: ",name) ;
			if (!ss_resolve_read(&tmp,sares.s,name)) strerr_diefu2sys(111,"read resolve file of: ",name) ;
			if (!ss_resolve_add_deps(&gares,&tmp,sares.s)) strerr_diefu2sys(111,"resolve dependencies of: ",name) ;	
			ss_resolve_free(&tmp) ;
		}
		/** reverse to start first the logger */
		genalloc_reverse(ss_resolve_t,&gares) ;
		if (!earlier)
		{
			if (!svc_init(info,svdir,&gares)) strerr_diefu2x(111,"initiate service of tree: ",info->treename.s) ;
		}
		else
		{
			if (!ss_resolve_create_live(info)) strerr_diefu1sys(111,"create live state") ;
			for (i = 0 ; i < genalloc_len(ss_resolve_t,&gares) ; i++)
			{
				char *string = genalloc_s(ss_resolve_t,&gares)[i].sa.s ;
				char *name = string + genalloc_s(ss_resolve_t,&gares)[i].name ;
				size_t namelen = strlen(name) ;
				char tocopy[dirlen + 1 + namelen + 1] ;
				memcpy(tocopy,svdir,dirlen) ;
				tocopy[dirlen] = '/' ;
				memcpy(tocopy + dirlen + 1, name, namelen) ;
				tocopy[dirlen + 1 + namelen] = 0 ;
				if (!hiercopy(tocopy,string + genalloc_s(ss_resolve_t,&gares)[i].runat)) strerr_diefu4sys(111,"copy: ",tocopy," to: ",string + genalloc_s(ss_resolve_t,&gares)[i].runat) ;
				ss_state_setflag(&sta,SS_FLAGS_RELOAD,SS_FLAGS_FALSE) ;
				ss_state_setflag(&sta,SS_FLAGS_INIT,SS_FLAGS_FALSE) ;
				ss_state_setflag(&sta,SS_FLAGS_UNSUPERVISE,SS_FLAGS_FALSE) ;
				ss_state_setflag(&sta,SS_FLAGS_STATE,SS_FLAGS_UNKNOWN) ;
				ss_state_setflag(&sta,SS_FLAGS_PID,SS_FLAGS_UNKNOWN) ;
				if (!ss_state_write(&sta,string + genalloc_s(ss_resolve_t,&gares)[i].state,name)) strerr_diefu2sys(111,"write state file of: ",name) ;
				VERBO1 strerr_warni2x("Initialized successfully: ",name) ;
			}
		}
	}

	follow:

	stralloc_free(&sares) ;
	ss_resolve_free(&res) ;
	genalloc_deepfree(ss_resolve_t,&gares,ss_resolve_free) ;
	genalloc_deepfree(stralist,&gasvc,stra_free) ;
	
	/** db already initiated? */
	if (db)
	{
		if (!earlier)
		{
			if (db_ok(info->livetree.s,info->treename.s))
			{
				VERBO1 strerr_warni3x("db of tree: ",info->treename.s," already initialized") ;
				goto end ;
			}
		}else strerr_dief3x(110,"scandir: ",info->scandir.s," is not running") ;
	}else goto end ;
	
	if (!rc_init(info,envp)) strerr_diefu2sys(111,"initiate db of tree: ",info->treename.s) ;
	
	end:
	return 0 ;
}
