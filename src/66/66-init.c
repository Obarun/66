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

#include <skalibs/buffer.h>
#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <s6/config.h>
#include <s6-rc/config.h>
#include <s6/s6-supervise.h>

#include <66/utils.h>
#include <66/constants.h>
#include <66/tree.h>
#include <66/backup.h>
#include <66/db.h>

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
	int r, classic, db ;
	
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
	
	classic = db = 0 ;
	
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
	if (!r) strerr_dief3x(110,"scandir: ",scandir.s," doesn't exist") ;
	
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
	
	livetree.len--;
	if (!stralloc_cats(&livetree,"/")) retstralloc(111,"main") ;
	if (!stralloc_cats(&livetree,treename)) retstralloc(111,"main") ;
	if (!stralloc_0(&livetree)) retstralloc(111,"main") ;
	
	
	tree.len-- ;
	size_t svdirlen ;
	char svdir[tree.len + SS_SVDIRS_LEN + SS_DB_LEN + treenamelen + 1 + 1] ;
	memcpy(svdir,tree.s,tree.len) ;
	memcpy(svdir + tree.len ,SS_SVDIRS ,SS_SVDIRS_LEN) ;
	svdirlen = tree.len + SS_SVDIRS_LEN ;
	memcpy(svdir + svdirlen, SS_SVC ,SS_SVC_LEN) ;
	svdir[svdirlen +  SS_SVC_LEN] = 0 ;
	
	/** svc already initiated */
	r = scan_mode(scandir.s,S_IFDIR) ;
	if (r < 0) strerr_dief2x(111,scandir.s," conflicted format") ;
	if (r)
	{
		strerr_warni2x(treename," svc services already initiated") ;
		classic = 0 ;
	}
	
	/** db already initiated? */
	if (db_ok(livetree.s,treename))
	{
		strerr_warni2x(treename," db already initiated") ;
		db = 0 ;
	}
	
	/** svc service work */
	if (classic)
	{
		VERBO2 strerr_warni5x("copy svc service from ",svdir," to ",scandir.s," ...") ;
		if (!hiercopy(svdir,scandir.s)) strerr_diefu4sys(111,"copy: ",svdir," to: ",scandir.s) ;
	}
	
	/** rc services work */
	if (db)
	{
		
		/** we assume that 66-scandir was launched previously because a db
		 * need an operationnal scandir, so we control if /run/66/scandir/owner exist,
		 * if not, exist immediately  */	
		r = scandir_ok(scandir.s) ;
		if (r != 1) strerr_dief3x(111,"scandir: ",scandir.s," is not running") ;
			
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
	
	stralloc_free(&base) ;
	stralloc_free(&tree) ;
	stralloc_free(&live) ;
	stralloc_free(&scandir) ;
	stralloc_free(&livetree) ;

	return 0 ;
}
	

