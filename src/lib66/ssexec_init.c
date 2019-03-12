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
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>

#include <oblibs/obgetopt.h>
#include <oblibs/string.h>
#include <oblibs/error2.h>
#include <oblibs/directory.h>
#include <oblibs/types.h>//scan_mode
#include <oblibs/stralist.h>
#include <oblibs/files.h>

#include <skalibs/buffer.h>
#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/direntry.h>

#include <66/utils.h>
#include <66/enum.h>
#include <66/constants.h>
#include <66/tree.h>
#include <66/backup.h>
#include <66/db.h>
#include <66/svc.h>
#include <66/resolve.h>
#include <66/ssexec.h>
#include <66/rc.h>



int ssexec_init(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
	int r, db, classic, earlier ;
	
	genalloc gasvc = GENALLOC_ZERO ; //stralist type
	genalloc gares = GENALLOC_ZERO ; //ss_resolve_t type
	stralloc ressrc = STRALLOC_ZERO ;
	stralloc resdst = STRALLOC_ZERO ;
	stralloc sares = STRALLOC_ZERO ;
	ss_resolve_t res = RESOLVE_ZERO ;
	
	classic = db = earlier = 0 ;

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
	}
	
	size_t dirlen ;
	size_t svdirlen ;
	char svdir[info->tree.len + SS_SVDIRS_LEN + SS_DB_LEN + 1 + info->treename.len + 1] ;
	memcpy(svdir,info->tree.s,info->tree.len) ;
	memcpy(svdir + info->tree.len ,SS_SVDIRS ,SS_SVDIRS_LEN) ;
	svdirlen = info->tree.len + SS_SVDIRS_LEN ;
	memcpy(svdir + svdirlen, SS_SVC ,SS_SVC_LEN) ;
	svdir[svdirlen +  SS_SVC_LEN] = 0 ;
	
	/** resolve live dir*/
	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC)) strerr_diefu1sys(111,"set revolve pointer to source") ;
	
	if (!stralloc_copy(&ressrc,&sares)) retstralloc(111,"main") ;
	ressrc.len--;
	if (!stralloc_cats(&ressrc,SS_RESOLVE)) retstralloc(111,"main") ;
	if (!stralloc_0(&ressrc)) retstralloc(111,"main") ;
		
	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_LIVE)) strerr_diefu1sys(111,"set revolve pointer to source") ;
	r = scan_mode(sares.s,S_IFDIR) ;
	if (r < 0) strerr_dief2x(111,sares.s," conflicting format") ;
	if (!r)
	{
		VERBO2 strerr_warni3x("create directory: ",sares.s,SS_RESOLVE) ;
		r = dir_create_under(sares.s,SS_RESOLVE+1,0700) ;
		if (!r) strerr_diefu3sys(111,"create directory: ",sares.s,SS_RESOLVE) ;
	}
	if (!stralloc_copy(&resdst,&sares)) retstralloc(111,"main") ;
	resdst.len--;
	if (!stralloc_cats(&resdst,SS_RESOLVE)) retstralloc(111,"main") ;
	if (!stralloc_0(&resdst)) retstralloc(111,"main") ;
	
	if (!hiercopy(ressrc.s,resdst.s)) strerr_diefu4sys(111,"copy resolve file of: ",ressrc.s," to ",resdst.s) ;
		
	stralloc_free(&ressrc) ;
	stralloc_free(&resdst) ;
	/** svc already initiated */
	if (classic)
	{
		int i ;
			
		if (!dir_cmp(svdir,info->scandir.s,"",&gasvc)) strerr_diefu4x(111,"compare ",svdir," to ",info->scandir.s) ;
		if (!genalloc_len(stralist,&gasvc))
		{
			VERBO1 strerr_warni2x("Initialization aborted -- no classic services into tree: ",info->treename.s) ;
			genalloc_deepfree(ss_resolve_t,&gares,ss_resolve_free) ;
			genalloc_deepfree(stralist,&gasvc,stra_free) ;
			goto follow ;
		}
				
		for (i = 0 ; i < genalloc_len(stralist,&gasvc) ; i++)
		{
			char *name = gaistr(&gasvc,i) ;
			ss_resolve_t tmp = RESOLVE_ZERO ;
			if (!ss_resolve_check(info,name,SS_RESOLVE_LIVE)) strerr_dief2sys(110,"unknow service: ",name) ;
			if (!ss_resolve_read(&tmp,sares.s,name)) strerr_diefu2sys(111,"read resolve file of: ",name) ;
			if (!ss_resolve_add_deps(&gares,&tmp,info)) strerr_diefu2sys(111,"resolve dependencies of: ",name) ;	
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
			dirlen = strlen(svdir) ;
			for (i = 0 ; i < genalloc_len(ss_resolve_t,&gares) ; i++)
			{
				char *string = genalloc_s(ss_resolve_t,&gares)[i].sa.s ;
				char *name = string + genalloc_s(ss_resolve_t,&gares)[i].name ;
				size_t namelen = gaistrlen(&gasvc,i) ;
				char tocopy[dirlen + 1 + namelen + 1] ;
				memcpy(tocopy,svdir,dirlen) ;
				tocopy[dirlen] = '/' ;
				memcpy(tocopy + dirlen + 1, name, namelen) ;
				tocopy[dirlen + 1 + namelen] = 0 ;
				if (!hiercopy(tocopy,string + genalloc_s(ss_resolve_t,&gares)[i].runat)) strerr_diefu4sys(111,"to copy: ",tocopy," to: ",string + genalloc_s(ss_resolve_t,&gares)[i].runat) ;
				if (!file_create_empty(string + genalloc_s(ss_resolve_t,&gares)[i].runat,"earlier",0644)) strerr_diefu3sys(111,"mark ",string + genalloc_s(ss_resolve_t,&gares)[i].name," as earlier service") ;
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
				VERBO1 strerr_warni3x("db of tree: ",info->treename.s," already initiated") ;
				goto end ;
			}
		}else strerr_dief3x(110,"scandir: ",info->scandir.s," is not running") ;
	}else goto end ;
	
	
	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC))		
		VERBO1 strerr_diefu1x(111,"set revolve pointer to source") ;
	
	if (!ss_resolve_check(info,SS_MASTER +1,SS_RESOLVE_SRC))  strerr_diefu1x(111,"find inner bundle -- please make a bug report") ;
	if (!ss_resolve_read(&res,sares.s,SS_MASTER + 1)) strerr_diefu1sys(111,"read resolve file of inner bundle") ;
	if (!res.ndeps)
	{
		VERBO1 strerr_warni2x("Initialization aborted -- no atomic services into tree: ",info->treename.s) ;
		stralloc_free(&sares) ;
		ss_resolve_free(&res) ;
		goto end ;
	}
		
	if (!clean_val(&gasvc,res.sa.s + res.deps)) strerr_diefu1sys(111,"clean dependencies of inner bundle") ;
	for (unsigned int i = 0 ; i < genalloc_len(stralist,&gasvc) ; i++)
	{
		char *name = gaistr(&gasvc,i) ;
		ss_resolve_t tmp = RESOLVE_ZERO ;
		if (!ss_resolve_check(info,name,SS_RESOLVE_SRC)) strerr_dief2sys(110,"unknow service: ",name) ;
		if (!ss_resolve_read(&tmp,sares.s,name)) strerr_diefu2sys(111,"read resolve file of: ",name) ;
		if (!ss_resolve_add_deps(&gares,&tmp,info)) strerr_diefu2sys(111,"resolve dependencies of: ",name) ;	
		ss_resolve_free(&tmp) ;
	}
							
	if (!rc_init(info,&gares,envp)) strerr_diefu2sys(111,"initiate db of tree: ",info->treename.s) ;
	
	genalloc_deepfree(ss_resolve_t,&gares,ss_resolve_free) ;
	genalloc_deepfree(stralist,&gasvc,stra_free) ;
	ss_resolve_free(&res) ;
	stralloc_free(&sares) ;
	
	end:	
	return 0 ;
}
