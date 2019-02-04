/* 
 * ssexec_init.c
 * 
 * Copyright (c) 2018 Eric Vidal <eric@obarun.org>
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

//#include <stdio.h>

int ssexec_init(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
	int r, db, classic, earlier ;
	
	int wstat ;
	pid_t pid ;
	
	char const *treename = 0 ;
	size_t treenamelen ;
	
	stralloc tree = STRALLOC_ZERO ;
	stralloc saresolve = STRALLOC_ZERO ;
	stralloc type = STRALLOC_ZERO ;
	genalloc gasvc = GENALLOC_ZERO ; //stralist type
	
	svstat_t svstat = SVSTAT_ZERO ;
	genalloc gasvstat = GENALLOC_ZERO ; //svstat_t type
	classic = db = earlier = 0 ;
	
	//PROG = "66-init" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">cdB", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'c' : classic = 1 ; break ;
				case 'd' : db = 1 ; break ;
				case 'B' : classic = 1 ; db = 1 ; break ;
				default : exitusage(usage_init) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc != 1) exitusage(usage_init) ;
	
	treename = *argv ;
	treenamelen = strlen(treename) ;
		
	if (!stralloc_cats(&tree,treename)) retstralloc(111,"main") ;
		
	r = tree_sethome(&tree,info->base.s,info->owner) ;
	if (!r) strerr_diefu2sys(111,"find tree: ", tree.s) ;
	
	if (!tree_get_permissions(tree.s,info->owner))
		strerr_dief2x(110,"You're not allowed to use the tree: ",tree.s) ;
	
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
	
	
	size_t svdirlen ;
	char svdir[tree.len + SS_SVDIRS_LEN + SS_DB_LEN + treenamelen + 1 + 1] ;
	memcpy(svdir,tree.s,tree.len) ;
	memcpy(svdir + tree.len ,SS_SVDIRS ,SS_SVDIRS_LEN) ;
	svdirlen = tree.len + SS_SVDIRS_LEN ;
	memcpy(svdir + svdirlen, SS_SVC ,SS_SVC_LEN) ;
	svdir[svdirlen +  SS_SVC_LEN] = 0 ;
	
	/** svc already initiated */
	if (classic)
	{
		int i ;
		if (!dir_cmp(svdir,info->scandir.s,"",&gasvc)) strerr_diefu4x(111,"compare ",svdir," to ",info->scandir.s) ;
		if (!earlier)
		{
			for (i = 0 ; i < genalloc_len(stralist,&gasvc) ; i++)
			{
				char *name = gaistr(&gasvc,i) ;
				size_t namelen = gaistrlen(&gasvc,i) ;
			
				if (!resolve_pointo(&saresolve,info,0,SS_RESOLVE_SRC))
					strerr_diefu1x(111,"set revolve pointer to source") ;
				r = resolve_read(&type,saresolve.s,name,"type") ;
				if (r < -1) strerr_dief2x(111,"invalid .resolve directory: ",saresolve.s) ;
				if (r && type.len && get_enumbyid(type.s,key_enum_el) == CLASSIC)
				{
					svstat.down = 0 ;
					svstat.name = name ;
					svstat.namelen = namelen ;
					r = resolve_read(&type,saresolve.s,name,"down") ;
					if (r > 0) svstat.down = 1 ;
					if (!genalloc_append(svstat_t,&gasvstat,&svstat)) strerr_diefu3x(111,"add: ",name," on genalloc") ;
				}
			}
			if (genalloc_len(svstat_t,&gasvstat))
			{
				if (!svc_init(info->scandir.s,svdir,&gasvstat)) strerr_diefu2x(111,"initiate service of tree: ",treename) ;
			}
			else strerr_warni3x("svc service of tree: ",treename," already initiated") ;
		}
		else
		{
			size_t dirlen = strlen(svdir) ;
			for (i = 0 ; i < genalloc_len(stralist,&gasvc) ; i++)
			{
				char *name = gaistr(&gasvc,i) ;
				size_t namelen = gaistrlen(&gasvc,i) ;
				char svscan[info->scandir.len + 1 + namelen + 1] ;
				memcpy(svscan,info->scandir.s,info->scandir.len) ;
				svscan[info->scandir.len] = '/' ;
				memcpy(svscan + info->scandir.len + 1, name,namelen) ;
				svscan[info->scandir.len + 1 + namelen] = 0 ;
				char tocopy[dirlen + 1 + namelen + 1] ;
				memcpy(tocopy,svdir,dirlen) ;
				tocopy[dirlen] = '/' ;
				memcpy(tocopy + dirlen + 1, name, namelen) ;
				tocopy[dirlen + 1 + namelen] = 0 ;
				if (!hiercopy(tocopy,svscan)) strerr_diefu4sys(111,"to copy: ",tocopy," to: ",svscan) ;
			}
		}
	}
	genalloc_deepfree(stralist,&gasvc,stra_free) ;
	genalloc_free(svstat_t,&gasvstat) ;
	stralloc_free(&saresolve) ;
	stralloc_free(&type) ;
	stralloc_free(&tree) ;
	
	/** db already initiated? */
	if (db)
	{
		if (!earlier)
		{
			if (db_ok(info->livetree.s,treename))
			{
				strerr_warni3x(" db of tree: ",treename," already initiated") ;
				goto end ;
			}
		}else strerr_dief3x(110,"scandir: ",info->scandir.s," is not running") ;
	}else goto end ;
	
	{
		char ltree[info->livetree.len + 4 + treenamelen] ;
		memcpy(ltree,info->livetree.s,info->livetree.len) ;
		ltree[info->livetree.len] = '/' ;
		memcpy(ltree + info->livetree.len + 1, treename, treenamelen) ;
		ltree[info->livetree.len + 1 + treenamelen] = 0 ;
		
		memcpy(svdir + svdirlen,SS_DB,SS_DB_LEN) ;
		memcpy(svdir + svdirlen + SS_DB_LEN, "/", 1) ;
		memcpy(svdir + svdirlen + SS_DB_LEN + 1, treename,treenamelen) ;
		svdir[svdirlen + SS_DB_LEN + 1 + treenamelen] = 0 ;
		
		char prefix[treenamelen + 1 + 1] ;
		memcpy(prefix,treename,treenamelen) ;
		memcpy(prefix + treenamelen, "-",1) ;
		prefix[treenamelen + 1] = 0 ;
		
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
	return 0 ;
}
