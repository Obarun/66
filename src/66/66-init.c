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

int copy_svc(char const *scandir, char const *pathsvc)
{
	int r ;
	
	r = scan_mode(scandir,S_IFDIR) ;
	if (r < 0) { errno = EEXIST ; return 0 ; }
	if (!r)
	{
		VERBO3 strerr_warnt2x("create directory: ",scandir) ;
		r = dir_create(scandir,0755) ;
		if (!r){
			VERBO3 strerr_warnwu2sys("create directory: ",scandir) ;
			return 0 ;
		}
	}
	VERBO3 strerr_warnt4x("copy service to: ",scandir," from: ", pathsvc) ;
	if (!hiercopy(pathsvc,scandir))
	{
		VERBO3 strerr_warnwu4sys("copy service to: ",scandir," from:", pathsvc) ;
		return 0 ;
	}
/*	VERBO3 strerr_warnt2x("reload scandir: ",scandir) ;
	r = s6_svc_writectl(scandir, S6_SVSCAN_CTLDIR, "an", 2) ;
	if (r < 0)
	{
		VERBO3 strerr_warnw3sys("something is wrong with the ",scandir, "/" S6_SVSCAN_CTLDIR " directory. errno reported") ;
		return -1 ;
	}
		*/
	return 1 ;
}
int main(int argc, char const *const *argv, char const *const *envp)
{
	int r, both, classic, db ;
	char ownerpack[256] ;
	uid_t owner ;
	int wstat ;
	pid_t pid ;
	
	char const *treename = NULL ;
	size_t treenamelen ;
	
	stralloc base = STRALLOC_ZERO ;
	stralloc tree = STRALLOC_ZERO ;
	stralloc live = STRALLOC_ZERO ;
	
	both = classic = db = 0 ;
	
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
				case 'B' : both = 1 ; break ;
				default : exitusage() ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc != 1) exitusage() ;
	
	owner = MYUID ;
	
	if (!set_ownersysdir(&base,owner)) strerr_diefu1sys(111, "set owner directory") ;
	
	size_t ownerlen = uid_fmt(ownerpack,owner) ;
	ownerpack[ownerlen] = 0 ;
	
	r = set_livedir(&live) ;
	if (!r) retstralloc(111,"main") ;
	if(r < 0) strerr_dief3x(111,"live: ",live.s," must be an absolute path") ;
	
	if (!stralloc_cats(&tree,*argv)) retstralloc(111,"main") ;
	if (!stralloc_0(&tree)) retstralloc(111,"main") ;
	
	//treename = *argv ;
	treename = *argv ;
	//treenamelen = strlen(treename) ;
	treenamelen = strlen(treename) ;
	
	/** /run/66/scandir */
	live.len-- ;
	size_t scandirlen ;
	char scandir[live.len + ownerlen + 8 + 1] ;
	memcpy(scandir,live.s,live.len) ;
	memcpy(scandir + live.len, "scandir/",8) ;
	scandirlen = live.len + 8 ;
	scandir[scandirlen] = 0 ;
	
	/** /run/66/scandir/owner */
	size_t pathscandirlen ;
	char pathscandir[scandirlen + ownerlen + 1] ;
	memcpy(pathscandir,scandir,scandirlen) ;
	memcpy(pathscandir + scandirlen,ownerpack,ownerlen) ;
	pathscandirlen = scandirlen + ownerlen ;
	pathscandir[pathscandirlen] = 0 ;
		
	r = dir_search(scandir,ownerpack,S_IFDIR) ;
	if (r != 1) strerr_dief4x(110,"scandir ",scandir,ownerpack," doesn't exist") ;
	
	r = tree_sethome(&tree,base.s) ;
	if (r < 0) strerr_diefu1x(110,"find the current tree. You must use -t options") ;
	if (!r) strerr_diefu2sys(111,"find tree: ", tree.s) ;
	
	r = scan_mode(tree.s,S_IFDIR) ;
	if (!r) strerr_diefu2sys(111,"find tree: ",tree.s) ; 
	
	if (!tree_get_permissions(tree.s))
		strerr_dief2x(110,"You're not allowed to use the tree: ",tree.s) ;
	
	/** /base/.66/system/tree/servicedirs */
	tree.len-- ;
	size_t pathsvdlen ;
	char pathsvd[tree.len + SS_SVDIRS_LEN + SS_DB_LEN + treenamelen + 1 + 1] ;
	memcpy(pathsvd,tree.s,tree.len) ;
	memcpy(pathsvd + tree.len ,SS_SVDIRS ,SS_SVDIRS_LEN) ;
	pathsvdlen = tree.len + SS_SVDIRS_LEN ;
	/** base/.66/system/tree/servicedirs/svc */
	memcpy(pathsvd + pathsvdlen, SS_SVC ,SS_SVC_LEN) ;
	pathsvd[pathsvdlen +  SS_SVC_LEN] = 0 ;
	
	/** run/66/tree/owner */
	size_t pathlivetreelen ;
	char pathlivetree[live.len + 5 + ownerlen + 1 + treenamelen + 1] ;
	memcpy(pathlivetree,live.s,live.len) ;
	memcpy(pathlivetree + live.len,"tree/",5) ;
	memcpy(pathlivetree + live.len + 5,ownerpack,ownerlen) ;
	pathlivetree[live.len + 5 + ownerlen] = 0 ;
	r = scan_mode(pathlivetree,S_IFDIR) ;
	if (r < 0) strerr_dief2x(111,pathlivetree," is not a directory") ;
	if (!r)
	{
		VERBO2 strerr_warni2x("create directory: ",pathlivetree) ;
		r = dir_create(pathlivetree,0700) ;
		if (!r) strerr_diefu2sys(111,"create directory: ",pathlivetree) ;
	}
	if (db_ok(pathlivetree,treename))
	{
		strerr_warni2x(treename," already initiated") ;
		return 0 ;
	}
	memcpy(pathlivetree + live.len + 5 + ownerlen, "/",1) ;
	memcpy(pathlivetree + live.len + 5 + ownerlen + 1,treename,treenamelen) ;
	pathlivetreelen = live.len + 5 + ownerlen + 1 + treenamelen ;
	pathlivetree[pathlivetreelen] = 0 ;
	
	
	//r = scan_mode(pathlivetree,S_IFDIR) ;
	///if (r) strerr_dief2x(110,pathlivetree," already exist") ;
	
	/** svc service work */
	if (classic || both)
	{
		VERBO2 strerr_warni5x("copy svc service from ",pathsvd," to ",scandir," ...") ;
		if (!copy_svc(pathscandir,pathsvd)) strerr_diefu2sys(111,"copy svc service to: ",pathscandir) ;
	//	VERBO2 strerr_warni3x("switch ",pathsvd," to source directory ...") ;
	//	r = backup_cmd_switcher(VERBOSITY,"-t30 -s0",treename) ;
	//	if (r != 1)
	//		strerr_diefu3x(111,"switch: ",pathsvd," to source directory") ;
	}
	if (db || both)
	{
		
		/** we assume that 66-scandir was launched previously because a db
		 * need an operationnal scandir, so we control if /run/66/scandir/owner exist,
		 * if not, exist immediately  */	
		r = scandir_ok(pathscandir) ;
		if (r != 1) strerr_dief3x(111,"scandir: ",pathscandir," is not running") ;
			
		/** rc services work */
		/** base/.66/system/tree/servicedirs/compiled */
		memcpy(pathsvd + pathsvdlen,SS_DB,SS_DB_LEN) ;
		memcpy(pathsvd + pathsvdlen + SS_DB_LEN, "/", 1) ;
		memcpy(pathsvd + pathsvdlen + SS_DB_LEN + 1, treename,treenamelen) ;
		pathsvd[pathsvdlen + SS_DB_LEN + 1 + treenamelen] = 0 ;
		
		char prefix[treenamelen + 1 + 1] ;
		memcpy(prefix,treename,treenamelen) ;
		memcpy(prefix + treenamelen, "-",1) ;
		prefix[treenamelen + 1] = 0 ;
		
		{
			char const *newargv[10] ;
			unsigned int m = 0 ;
			
			newargv[m++] = S6RC_BINPREFIX "s6-rc-init" ;
			newargv[m++] = "-l" ;
			newargv[m++] = pathlivetree ;
			newargv[m++] = "-c" ;
			newargv[m++] = pathsvd ;
			newargv[m++] = "-p" ;
			newargv[m++] = prefix ;
			newargv[m++] = "--" ;
			newargv[m++] = pathscandir ;
			newargv[m++] = 0 ;
			
			VERBO2 strerr_warni3x("initiate db ",pathsvd," ...") ;
			
			pid = child_spawn0(newargv[0],newargv,envp) ;
			if (waitpid_nointr(pid,&wstat, 0) < 0)
				strerr_diefu2sys(111,"wait for ",newargv[0]) ;
				
			if (wstat)
				strerr_diefu2x(111,"init: ",pathsvd) ;
		}
		
	//	VERBO2 strerr_warni3x("switch ",pathsvd," to source directory ...") ;
	//	r = backup_cmd_switcher(VERBOSITY,"-t31 -s0",treename) ;
	//	if (r != 1)
	//		strerr_diefu3x(111,"switch: ",pathsvd," to original directory") ;
	}
	
	stralloc_free(&base) ;
	stralloc_free(&tree) ;
	stralloc_free(&live) ;

	return 0 ;
}
	

