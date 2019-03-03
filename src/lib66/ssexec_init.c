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

#include <oblibs/obgetopt.h>
#include <oblibs/error2.h>
#include <oblibs/directory.h>
#include <oblibs/types.h>//scan_mode
#include <oblibs/stralist.h>

#include <skalibs/buffer.h>
#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/direntry.h>

#include <s6/config.h>
#include <s6-rc/config.h>
#include <s6/s6-supervise.h>

#include <66/utils.h>
#include <66/enum.h>
#include <66/constants.h>
#include <66/tree.h>
#include <66/backup.h>
#include <66/db.h>
#include <66/svc.h>
#include <66/resolve.h>
#include <66/ssexec.h>

#include <stdio.h>

int ssexec_init(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
	int r, db, classic, earlier ;
	
	int wstat ;
	pid_t pid ;
	
	genalloc gasvc = GENALLOC_ZERO ; //stralist type
	
	genalloc gares = GENALLOC_ZERO ; //ss_resolve_t type
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
	if (r < 0) strerr_dief2x(111,info->livetree.s," conflicting format") ;
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

	stralloc src = STRALLOC_ZERO ;
	if (!ss_resolve_pointo(&src,info,SS_NOTYPE,SS_RESOLVE_SRC)) strerr_diefu1sys(111,"set revolve pointer to source") ;
	/** svc already initiated */
	if (classic)
	{
		int i ;
			
		if (!dir_cmp(svdir,info->scandir.s,"",&gasvc)) strerr_diefu4x(111,"compare ",svdir," to ",info->scandir.s) ;
		if (!genalloc_len(stralist,&gasvc))
		{
			VERBO1 strerr_warni3x("svc service of tree: ",info->treename.s," already initiated") ;
			goto follow ;
		}
		for (i = 0 ; i < genalloc_len(stralist,&gasvc) ; i++)
		{
			char *name = gaistr(&gasvc,i) ;
			ss_resolve_t tmp = RESOLVE_ZERO ;
			if (!ss_resolve_check(info,name,SS_RESOLVE_SRC)) strerr_dief2sys(110,"unknow service: ",name) ;
			if (!ss_resolve_read(&tmp,src.s,name)) strerr_diefu2sys(111,"read resolve file of: ",name) ;
			
			if (!genalloc_append(ss_resolve_t,&gares,&tmp)) strerr_diefu3x(111,"add: ",name," on genalloc") ;
			
		}
		if (!ss_resolve_addlogger(info,&gares))	strerr_diefu1sys(111,"add logger to list") ;
		
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
			}
		}
	}

	follow:

	stralloc_free(&src) ;
	ss_resolve_free(&res) ;
	genalloc_deepfree(ss_resolve_t,&gares,ss_resolve_free) ;
	
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
	
	{
		char ltree[info->livetree.len + 1 + info->treename.len + 1] ;
		memcpy(ltree,info->livetree.s,info->livetree.len) ;
		ltree[info->livetree.len] = '/' ;
		memcpy(ltree + info->livetree.len + 1, info->treename.s, info->treename.len) ;
		ltree[info->livetree.len + 1 + info->treename.len] = 0 ;
		
		memcpy(svdir + svdirlen,SS_DB,SS_DB_LEN) ;
		memcpy(svdir + svdirlen + SS_DB_LEN, "/", 1) ;
		memcpy(svdir + svdirlen + SS_DB_LEN + 1, info->treename.s,info->treename.len) ;
		svdir[svdirlen + SS_DB_LEN + 1 + info->treename.len] = 0 ;
		
		char prefix[info->treename.len + 1 + 1] ;
		memcpy(prefix,info->treename.s,info->treename.len) ;
		memcpy(prefix + info->treename.len, "-",1) ;
		prefix[info->treename.len + 1] = 0 ;
		
		{
			char const *newargv[10] ;
			unsigned int m = 0 ;
			
			newargv[m++] = S6RC_BINPREFIX "s6-rc-init" ;
			newargv[m++] = "-l" ;
			newargv[m++] = ltree ;
			newargv[m++] = "-c" ;
			newargv[m++] = svdir ;
			newargv[m++] = "-p" ;
			newargv[m++] = prefix ;
			newargv[m++] = "--" ;
			newargv[m++] = info->scandir.s ;
			newargv[m++] = 0 ;
			
			VERBO2 strerr_warni3x("initiate db ",svdir," ...") ;
			
			pid = child_spawn0(newargv[0],newargv,envp) ;
			if (waitpid_nointr(pid,&wstat, 0) < 0)
				strerr_diefu2sys(111,"wait for ",newargv[0]) ;
				
			if (wstat)
				strerr_diefu2x(111,"init db: ",svdir) ;
		}
	}
	end:	
	
	genalloc_deepfree(stralist,&gasvc,stra_free) ;
	return 0 ;
}
