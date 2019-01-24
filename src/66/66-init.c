/* 
 * 66-init.c
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

#define USAGE "66-init [ -h help ] [ -v verbosity ] [ -l live ] [ -c classic ] [ -d database ] [ -B both service ] tree"

unsigned int VERBOSITY = 1 ;

static inline void info_help (void)
{
  static char const *help =
"66-init <options> tree\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-v: increase/decrease verbosity\n"
"	-l: live directory\n" 
"	-c: init classic service\n"
"	-d: init database service\n"
"	-B: init classic and database service\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
}

int main(int argc, char const *const *argv, char const *const *envp)
{
	int r, db, classic, earlier ;
	
	uid_t owner ;
	int wstat ;
	pid_t pid ;
	
	char const *treename = 0 ;
	size_t treenamelen ;
	
	stralloc base = STRALLOC_ZERO ;
	stralloc tree = STRALLOC_ZERO ;
	stralloc live = STRALLOC_ZERO ;
	stralloc scandir = STRALLOC_ZERO ;
	stralloc livetree = STRALLOC_ZERO ;
	stralloc saresolve = STRALLOC_ZERO ;
	stralloc type = STRALLOC_ZERO ;
	genalloc gasvc = GENALLOC_ZERO ; //stralist type
	
	svstat_t svstat = SVSTAT_ZERO ;
	genalloc gasvstat = GENALLOC_ZERO ; //svstat_t type
	classic = db = earlier = 0 ;
	
	PROG = "66-init" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, "hv:l:cdB", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'h' : info_help(); return 0 ;
				case 'v' : if (!uint0_scan(l.arg, &VERBOSITY)) exitusage() ; break ;
				case 'l' : if(!stralloc_cats(&live,l.arg)) retstralloc(111,"main") ;
						   if(!stralloc_0(&live)) retstralloc(111,"main") ;
						   break ;
				case 'c' : classic = 1 ; break ;
				case 'd' : db = 1 ; break ;
				case 'B' : classic = 1 ; db = 1 ; break ;
				default : exitusage() ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc != 1) exitusage() ;
	
	owner = MYUID ;
	treename = *argv ;
	treenamelen = strlen(treename) ;
	
	if (!set_ownersysdir(&base,owner)) strerr_diefu1sys(111, "set owner directory") ;
	
	r = set_livedir(&live) ;
	if (!r) retstralloc(111,"main") ;
	if(r < 0) strerr_dief3x(111,"live: ",live.s," must be an absolute path") ;
	
	if (!stralloc_cats(&tree,treename)) retstralloc(111,"main") ;
	if (!stralloc_0(&tree)) retstralloc(111,"main") ;
	
	r = tree_sethome(&tree,base.s) ;
	if (!r) strerr_diefu2sys(111,"find tree: ", tree.s) ;
	
	if (!tree_get_permissions(tree.s))
		strerr_dief2x(110,"You're not allowed to use the tree: ",tree.s) ;
		
	if (!stralloc_copy(&scandir,&live)) retstralloc(111,"main") ;
	
	r = set_livescan(&scandir,owner) ;
	if (!r) retstralloc(111,"main") ;
	if (r < 0) strerr_dief3x(111,"scandir: ",scandir.s," must be an absolute path") ;
	
	r = scan_mode(scandir.s,S_IFDIR) ;
	if (r < 0) strerr_dief2x(111,scandir.s," conflicted format") ;
	if (!r) strerr_dief3x(110,"scandir: ",scandir.s," doesn't exist") ;
	
	r = scandir_ok(scandir.s) ;
	if (r != 1) earlier = 1 ; 
	
	if (!stralloc_copy(&livetree,&live)) retstralloc(111,"main") ;
	r = set_livetree(&livetree,owner) ;
	if (!r) retstralloc(111,"main") ;
	if (r < 0) strerr_dief3x(111,"livetree: ",livetree.s," must be an absolute path") ;
	
	r = scan_mode(livetree.s,S_IFDIR) ;
	if (r < 0) strerr_dief2x(111,livetree.s," is not a directory") ;
	if (!r)
	{
		VERBO2 strerr_warni2x("create directory: ",livetree.s) ;
		r = dir_create(livetree.s,0700) ;
		if (!r) strerr_diefu2sys(111,"create directory: ",livetree.s) ;
	}
	
	tree.len-- ;
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
		if (!dir_cmp(svdir,scandir.s,"",&gasvc)) strerr_diefu4x(111,"compare ",svdir," to ",scandir.s) ;
		if (!earlier)
		{
			for (i = 0 ; i < genalloc_len(stralist,&gasvc) ; i++)
			{
				char *name = gaistr(&gasvc,i) ;
				size_t namelen = gaistrlen(&gasvc,i) ;
			
				if (!resolve_pointo(&saresolve,base.s,live.s,tree.s,treename,0,SS_RESOLVE_SRC))
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
				if (!svc_init(scandir.s,svdir,&gasvstat)) strerr_diefu2x(111,"initiate service of tree: ",treename) ;
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
				size_t scanlen = scandir.len - 1 ;
				char svscan[scanlen + 1 + namelen + 1] ;
				memcpy(svscan,scandir.s,scanlen) ;
				svscan[scanlen] = '/' ;
				memcpy(svscan + scanlen + 1, name,namelen) ;
				svscan[scanlen + 1 + namelen] = 0 ;
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
	
	/** db already initiated? */
	if (db)
	{
		if (!earlier)
		{
			if (db_ok(livetree.s,treename))
			{
				strerr_warni3x(" db of tree: ",treename," already initiated") ;
				goto end ;
			}
		}else strerr_dief3x(110,"scandir: ",scandir.s," is not running") ;
	}else goto end ;
	
	{
		livetree.len--;
		if (!stralloc_cats(&livetree,"/")) retstralloc(111,"main") ;
		if (!stralloc_cats(&livetree,treename)) retstralloc(111,"main") ;
		if (!stralloc_0(&livetree)) retstralloc(111,"main") ;
		
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
			newargv[m++] = livetree.s ;
			newargv[m++] = "-c" ;
			newargv[m++] = svdir ;
			newargv[m++] = "-p" ;
			newargv[m++] = prefix ;
			newargv[m++] = "--" ;
			newargv[m++] = scandir.s ;
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
		stralloc_free(&base) ;
		stralloc_free(&tree) ;
		stralloc_free(&live) ;
		stralloc_free(&scandir) ;
		stralloc_free(&livetree) ;

	return 0 ;
}
	

