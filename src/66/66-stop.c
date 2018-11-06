/* 
 * 66-stop.c
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

#include <sys/stat.h>

#include <oblibs/obgetopt.h>
#include <oblibs/error2.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>
#include <oblibs/stralist.h>

#include <skalibs/buffer.h>
#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/unix-transactional.h>

#include <s6/s6-supervise.h>//s6_svc_ok
#include <s6/config.h>


#include <66/tree.h>
#include <66/db.h>
#include <66/config.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/enum.h>
#include <66/svc.h>
#include <66/backup.h>
#include <stdio.h>

unsigned int VERBOSITY = 1 ;
static tain_t DEADLINE ;
unsigned int UNSUP = 0 ;
stralloc saresolve = STRALLOC_ZERO ;
genalloc gatoremove = GENALLOC_ZERO ;//stralist
genalloc gaunsup = GENALLOC_ZERO ; //stralist
#define USAGE "66-stop [ -h help ] [ -v verbosity ] [ -T timeout ] [ -l live ] [ -t tree ] [ -u unsupervise ] service(s)"

static inline void info_help (void)
{
  static char const *help =
"66-stop <options> service(s)\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-v: increase/decrease verbosity\n"
"	-l: live directory\n"
"	-T: timeout\n"
"	-t: tree to use\n"
"	-u: unsupervise service\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
}

int svc_release(char const *base, char const *scandir, char const *live, char const *tree, char const *treename)
{
	size_t scanlen = strlen(scandir) ;
	
	if (genalloc_len(stralist,&gaunsup))
	{
		for (unsigned int i = 0; i < genalloc_len(stralist,&gaunsup); i++)
		{
			char const *svname = gaistr(&gaunsup,i) ;
			size_t svnamelen = gaistrlen(&gaunsup,i) ;
	
			char rm[scanlen + 1 + svnamelen + 1] ;
			memcpy(rm,scandir,scanlen) ;
			rm[scanlen] = '/' ;
			memcpy(rm + scanlen + 1, svname,svnamelen) ;
			rm[scanlen + 1 + svnamelen] = 0 ;
			VERBO3 strerr_warnt2x("unsupervise: ",rm) ;
			if (rm_rf(rm) < 0)
			{
				VERBO3 strerr_warnwu2sys("remove directory: ",rm) ;
				return 0 ;
			}
		}
	}
	if (!resolve_pointo(&saresolve,base,live,tree,treename,0,SS_RESOLVE_SRC))
	{
		VERBO3 strerr_warnwu1x("set revolve pointer to source") ;
		return 0 ;
	}
	if (genalloc_len(stralist,&gatoremove))
	{
		for (unsigned int i = 0; i < genalloc_len(stralist,&gatoremove); i++)
		{
			char const *svname = gaistr(&gatoremove,i) ;
			
			if (!resolve_remove_service(saresolve.s,svname))
			{
				VERBO3 strerr_warnwu4sys("remove resolve service file: ",saresolve.s,"/.resolve/",svname) ;
				return 0 ;
			}
		}
	}
	
	return 1 ;
}
int svc_down(char const *base,char const *scandir,char const *live,char const *tree, char const *treename,genalloc *ga,char const *const *envp)
{
	int wstat ;
	pid_t pid ; 
	
	genalloc tot = GENALLOC_ZERO ; //stralist
		
	for (unsigned int i = 0 ; i < genalloc_len(svstat_t,ga) ; i++) 
	{
		char const *svname = genalloc_s(svstat_t,ga)[i].name ;
		int torm = genalloc_s(svstat_t,ga)[i].remove ;
		int unsup = genalloc_s(svstat_t,ga)[i].unsupervise ;
		
		if (!stra_add(&tot,svname))
		{
			VERBO3 strerr_warnwu3x("add: ",svname," as service to shutdown") ;
			return 0 ;
		}
		
		/** need to remove?*/
		if (torm)
		{
			if (!stra_add(&gatoremove,svname))
			{
				VERBO3 strerr_warnwu3x("add: ",svname," as service to remove") ;
				return 0 ;
			}
		}
		/** unsupervise */
		if (unsup)
		{
			if (!stra_add(&gaunsup,svname))
			{
				VERBO3 strerr_warnwu3x("add: ",svname," as service to unsupervise") ;
				return 0 ;
			}
		}
		/** logger */
		if (!resolve_pointo(&saresolve,base,live,tree,treename,0,SS_RESOLVE_SRC))
		{
			VERBO3 strerr_warnwu1x("set revolve pointer to source") ;
			return 0 ;
		}
		if (resolve_read(&saresolve,saresolve.s,svname,"logger"))
		{
			if (!stra_add(&tot,saresolve.s))
			{
				VERBO3 strerr_warnwu3x("add: ",saresolve.s," as service to shutdown") ;
				return 0 ;
			}
			if (torm)
			{
				if (!stra_add(&gatoremove,saresolve.s))
				{
					VERBO3 strerr_warnwu3x("add logger of: ",saresolve.s," as service to remove") ;
					return 0 ;
				}
			}
			if (unsup)
			{
				if (!stra_add(&gaunsup,svname))
				{
					VERBO3 strerr_warnwu3x("add: ",saresolve.s," as service to unsupervise") ;
					return 0 ;
				}
			}
		}
	}
	char const *newargv[11 + genalloc_len(stralist,&tot)] ;
	unsigned int m = 0 ;
	char fmt[UINT_FMT] ;
	fmt[uint_fmt(fmt, VERBOSITY)] = 0 ;
	char tt[UINT32_FMT] ;
	tt[uint32_fmt(tt,tain_to_millisecs(&DEADLINE))] = 0 ;

	newargv[m++] = SS_BINPREFIX "66-svctl" ;
	newargv[m++] = "-v" ;
	newargv[m++] = fmt ;
	newargv[m++] = "-T" ;
	newargv[m++] = tt ;
	newargv[m++] = "-l" ;
	newargv[m++] = live ;
	newargv[m++] = "-t" ;
	newargv[m++] = treename ;
	newargv[m++] = "-D" ;
	for (unsigned int i = 0 ; i < genalloc_len(stralist,&tot) ; i++) 
		newargv[m++] = gaistr(&tot,i) ;
	
	newargv[m++] = 0 ;
	
	pid = child_spawn0(newargv[0],newargv,envp) ;
	if (waitpid_nointr(pid,&wstat, 0) < 0)
	{
		VERBO3 strerr_warnwu2sys("wait for ",newargv[0]) ;
		return 0 ;
	}
	if (wstat)
	{
		VERBO3 strerr_warnwu1x("shutdown list of services") ;
		return 0 ;
	}
	
	
	if (!resolve_pointo(&saresolve,base,live,tree,treename,0,SS_RESOLVE_SRC))
	{
		VERBO3 strerr_warnwu1x("set revolve pointer to source") ;
		return 0 ;
	}
	for (unsigned int i = 0; i < genalloc_len(svstat_t,ga) ; i++)
	{
		VERBO3 strerr_warnt2x("remove remove resolve file for: ",genalloc_s(svstat_t,ga)[i].name) ;
		if (!resolve_remove(saresolve.s,genalloc_s(svstat_t,ga)[i].name,"remove"))
		{
			VERBO3 strerr_warnwu3sys("remove resolve file",saresolve.s,"/remove") ;
			return 0 ;
		}
	}
	genalloc_deepfree(stralist,&tot,stra_free) ;
	
	return 1 ;
	
}
int rc_release(char const *base, char const *scandir, char const *live,char const *tree, char const *treename)
{
//	size_t scanlen = strlen(scandir) ;
/*	size_t treenamelen = strlen(treename) ;

	if (genalloc_len(stralist,&gaunsup))
	{
		for (unsigned int i = 0; i < genalloc_len(stralist,&gaunsup); i++)
		{
			char const *svname = gaistr(&gaunsup,i) ;
			size_t svnamelen = gaistrlen(&gaunsup,i) ;
	
			char rm[scanlen + 1 + treenamelen + 1 + svnamelen + 1] ;
			memcpy(rm,scandir,scanlen) ;
			rm[scanlen] = '/' ;
			memcpy(rm + scanlen + 1, treename,treenamelen) ;
			rm[scanlen + 1 + svnamelen] = '-' ;
			memcpy(rm + scanlen + 1 + treenamelen + 1,svname,svnamelen) ;
			rm[scanlen + 1 + treenamelen + 1 + svnamelen] = 0 ;
			VERBO3 strerr_warnt2x("unsupervise: ",rm) ;
			if (rm_rf(rm) < 0)
			{
				VERBO3 strerr_warnwu2sys("remove directory: ",rm) ;
				return 0 ;
			}
		}
	}*/
	if (!resolve_pointo(&saresolve,base,live,tree,treename,0,SS_RESOLVE_SRC))
	{
		VERBO3 strerr_warnwu1x("set revolve pointer to source") ;
		return 0 ;
	}
	if (genalloc_len(stralist,&gatoremove))
	{
		for (unsigned int i = 0; i < genalloc_len(stralist,&gatoremove); i++)
		{
			char const *svname = gaistr(&gatoremove,i) ;
			
			if (!resolve_remove_service(saresolve.s,svname))
			{
				VERBO3 strerr_warnwu4sys("remove resolve service file: ",saresolve.s,"/.resolve/",svname) ;
				return 0 ;
			}
		}
	}
	
	
		
	return 1 ;
}

int rc_down(char const *base, char const *scandir, char const *live, char const *livetree,char const *tree, char const *treename,genalloc *ga,char const *const *envp,unsigned int trc)
{
	int wstat ;
	pid_t pid ;
	
	size_t livetreelen = strlen(livetree) ;
	size_t treenamelen = strlen(treename) ;

	genalloc tot = GENALLOC_ZERO ; //stralist

	for (unsigned int i = 0 ; i < genalloc_len(svstat_t,ga) ; i++) 
	{
		
		char const *svname = genalloc_s(svstat_t,ga)[i].name ;
		int torm = genalloc_s(svstat_t,ga)[i].remove ;
	//	int unsup = genalloc_s(svstat_t,ga)[i].unsupervise ;
		
		if (!stra_add(&tot,svname))
		{
			VERBO3 strerr_warnwu3x("add: ",svname," as service to shutdown") ;
			return 0 ;
		}
		
		/** need to remove?*/
		if (torm)
		{
			if (!stra_add(&gatoremove,svname))
			{
				VERBO3 strerr_warnwu3x("add: ",svname," as service to remove") ;
				return 0 ;
			}
		}
		/** unsupervise */
	/*	if (unsup)
		{
			if (!stra_add(&gaunsup,svname))
			{
				VERBO3 strerr_warnwu3x("add: ",svname," as service to unsupervise") ;
				return 0 ;
			}
		}*/
		/** logger */
		if (!resolve_pointo(&saresolve,base,live,tree,treename,0,SS_RESOLVE_SRC))
		{
			VERBO3 strerr_warnwu1x("set revolve pointer to source") ;
			return 0 ;
		}
		if (resolve_read(&saresolve,saresolve.s,svname,"logger"))
		{
			if (!stra_add(&tot,saresolve.s))
			{
				VERBO3 strerr_warnwu3x("add: ",saresolve.s," as service to shutdown") ;
				return 0 ;
			}
			if (torm)
			{
				if (!stra_add(&gatoremove,saresolve.s))
				{
					VERBO3 strerr_warnwu3x("add logger of: ",saresolve.s," as service to remove") ;
					return 0 ;
				}
			}
		/*	if (unsup)
			{
				if (!stra_add(&gaunsup,saresolve.s))
				{
					VERBO3 strerr_warnwu3x("add: ",saresolve.s," as service to unsupervise") ;
					return 0 ;
				}
			}*/
		}
	}
	
	
	
	char db[livetreelen + 1 + treenamelen + 1] ;
	memcpy(db,livetree,livetreelen) ;
	db[livetreelen] = '/' ;
	memcpy(db + livetreelen + 1, treename, treenamelen) ;
	db[livetreelen + 1 + treenamelen] = 0 ;
	
	if (!db_ok(livetree, treename))
		strerr_dief2x(111,db," is not initialized") ;
	
	//if (!db_switch_to(base,livetree,tree,treename,envp,SS_SWSRC))
	//	strerr_diefu3x(111,"switch",db," to source") ;
		
	char const *newargv[11 + genalloc_len(stralist,&tot)] ;
	unsigned int m = 0 ;
	char fmt[UINT_FMT] ;
	fmt[uint_fmt(fmt, VERBOSITY)] = 0 ;	
	
	int globalt ;
	tain_t globaltto ;
	tain_sub(&globaltto,&DEADLINE, &STAMP) ;
	globalt = tain_to_millisecs(&globaltto) ;
	if (!globalt) globalt = 1 ;
	if (globalt > 0 && (!trc || (unsigned int) globalt < trc))
		trc = (uint32_t)globalt ;
	
	char tt[UINT32_FMT] ;
	tt[uint32_fmt(tt,trc)] = 0 ;
	
	newargv[m++] = SS_BINPREFIX "66-dbctl" ;
	newargv[m++] = "-v" ;
	newargv[m++] = fmt ;
	newargv[m++] = "-T" ;
	newargv[m++] = tt ;
	newargv[m++] = "-l" ;
	newargv[m++] = live ;
	newargv[m++] = "-t" ;
	newargv[m++] = treename ;
	newargv[m++] = "-d" ;
	
	for (unsigned int i = 0; i < genalloc_len(stralist,&tot) ; i++)
			newargv[m++] = gaistr(&tot,i) ;
		
	newargv[m++] = 0 ;
	
	pid = child_spawn0(newargv[0],newargv,envp) ;
	if (waitpid_nointr(pid,&wstat, 0) < 0)
	{
		VERBO3 strerr_warnwu2sys("wait for ",newargv[0]) ;
		return 0 ;
	}
	if (wstat)
	{
		VERBO3 strerr_warnwu1x("bring down service list") ;
		return 0 ;
	}
	
	if (!resolve_pointo(&saresolve,base,live,tree,treename,0,SS_RESOLVE_SRC))
	{
		VERBO3 strerr_warnwu1x("set revolve pointer to source") ;
		return 0 ;
	}
	for (unsigned int i = 0; i < genalloc_len(svstat_t,ga) ; i++)
	{
		VERBO3 strerr_warnt2x("remove remove resolve file for: ",genalloc_s(svstat_t,ga)[i].name) ;
		if (!resolve_remove(saresolve.s,genalloc_s(svstat_t,ga)[i].name,"remove"))
		{
			VERBO3 strerr_warnwu3sys("remove resolve file",saresolve.s,"/remove") ;
			return 0 ;
		}
	}

	
	genalloc_deepfree(stralist,&tot,stra_free) ;
	
	return 1 ;
}

int main(int argc, char const *const *argv,char const *const *envp)
{
	int r, rb ;
	unsigned int trc ;
	
	uid_t owner ;
	
	stralloc base = STRALLOC_ZERO ;
	stralloc tree = STRALLOC_ZERO ;
	stralloc scandir = STRALLOC_ZERO ;
	stralloc live = STRALLOC_ZERO ;
	stralloc livetree = STRALLOC_ZERO ;
	stralloc svok = STRALLOC_ZERO ;
	
	genalloc nclassic = GENALLOC_ZERO ;
	genalloc nrc = GENALLOC_ZERO ;
	
	unsigned int tmain = trc = 0 ;
	
	PROG = "66-stop" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">hv:l:t:T:u", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;

			switch (opt)
			{
				case 'h' : 	info_help(); return 0 ;
				case 'v' :  if (!uint0_scan(l.arg, &VERBOSITY)) exitusage() ; break ;
				case 'l' : 	if(!stralloc_cats(&live,l.arg)) retstralloc(111,"main") ;
							if(!stralloc_0(&live)) retstralloc(111,"main") ;
							break ;
				case 'T' :	if (!uint0_scan(l.arg, &tmain)) exitusage() ; break ;
				case 't' : 	if(!stralloc_cats(&tree,l.arg)) retstralloc(111,"main") ;
							if(!stralloc_0(&tree)) retstralloc(111,"main") ;
							break ;
				case 'u' :	UNSUP = 1 ; break ;
				default : exitusage() ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc < 1) exitusage() ;
	
	owner = MYUID ;
	
	if (!set_ownersysdir(&base,owner)) strerr_diefu1sys(111, "set owner directory") ;
	
	if (tmain){
		tain_from_millisecs(&DEADLINE, tmain) ;
		trc = tmain ;
	}
	else DEADLINE = tain_infinite_relative ;
	
	r = tree_sethome(&tree,base.s) ;
	if (r < 0) strerr_diefu1x(110,"find the current tree. You must use -t options") ;
	if (!r) strerr_diefu2sys(111,"find tree: ", tree.s) ;
	
	size_t treelen = get_rlen_until(tree.s,'/',tree.len - 1) ;
	size_t treenamelen = (tree.len - 1) - treelen ;
	char treename[treenamelen + 1] ;
	memcpy(treename, tree.s + treelen + 1,treenamelen) ;
	treenamelen-- ;
	treename[treenamelen] = 0 ;
	
	if (!tree_get_permissions(tree.s))
		strerr_dief2x(110,"You're not allowed to use the tree: ",tree.s) ;
	
	r = set_livedir(&live) ;
	if (!r) retstralloc(111,"main") ;
	if(r < 0) strerr_dief3x(111,"live: ",live.s," must be an absolute path") ;
	
	if (!stralloc_copy(&livetree,&live)) retstralloc(111,"main") ;
	if (!stralloc_copy(&scandir,&live)) retstralloc(111,"main") ;
		
	r = set_livetree(&livetree,owner) ;
	if (!r) retstralloc(111,"main") ;
	if(r < 0) strerr_dief3x(111,"live: ",livetree.s," must be an absolute path") ;

	r = set_livescan(&scandir,owner) ;
	if (!r) retstralloc(111,"main") ;
	if(r < 0) strerr_dief3x(111,"live: ",scandir.s," must be an absolute path") ;
	
	if ((scandir_ok(scandir.s)) !=1 ) strerr_dief3sys(111,"scandir: ", scandir.s," is not running") ;
	
	if (!stralloc_catb(&svok,scandir.s,scandir.len - 1)) retstralloc(111,"main") ; 
	if (!stralloc_cats(&svok,"/")) retstralloc(111,"main") ; 
	size_t svoklen = svok.len ;
	
	{
		stralloc type = STRALLOC_ZERO ;
		svstat_t svsrc = SVSTAT_ZERO ;
		
		for(;*argv;argv++)
		{
			svsrc.type = 0 ;
			svsrc.name = 0 ;
			svsrc.namelen = 0 ;
			svsrc.remove = 0 ;
			svsrc.unsupervise = 0 ;
			svok.len = svoklen ;
			
			if (!resolve_pointo(&saresolve,base.s,live.s,tree.s,treename,0,SS_RESOLVE_SRC))
				strerr_diefu1x(111,"set revolve pointer to source") ;
			rb = resolve_read(&type,saresolve.s,*argv,"type") ;
			if (rb < -1) strerr_dief2x(111,"invalid .resolve directory: ",saresolve.s) ;
			if (rb < 0)
			{
				if (!resolve_pointo(&saresolve,base.s,live.s,tree.s,treename,0,SS_RESOLVE_BACK))
					strerr_diefu1x(111,"set revolve pointer to backup") ;
				r = resolve_read(&type,saresolve.s,*argv,"type") ;
				if (r < -1) strerr_diefu2sys(111,"invalid .resolve directory: ",saresolve.s) ; 
				if (r > 0) svsrc.remove = 1 ;
				if (r <= 0) strerr_dief2x(111,*argv,": is not enabled") ;
			}
			if (rb > 0)
			{
				svsrc.type = get_enumbyid(type.s,key_enum_el) ;
				svsrc.name = *argv ;
				svsrc.namelen = strlen(*argv) ;
				r = resolve_read(&type,saresolve.s,*argv,"remove") ;
				if (r < -1) strerr_diefu2sys(111,"invalid .resolve directory: ",saresolve.s) ;
				if (r > 0) svsrc.remove = 1 ;
				if (!resolve_pointo(&saresolve,base.s,live.s,tree.s,treename,0,SS_RESOLVE_BACK))
					strerr_diefu1x(111,"set revolve pointer to backup") ;
				r = resolve_read(&type,saresolve.s,*argv,"type") ;
				if (r <= 0 && !UNSUP) strerr_dief3x(111,"service: ",*argv," is not running, you can only start it") ;
			}
			if (svsrc.type == CLASSIC)
			{
				if (!stralloc_cats(&svok,*argv)) retstralloc(111,"main") ; 
				if (!stralloc_0(&svok)) retstralloc(111,"main") ; 
				r = s6_svc_ok(svok.s) ;
				if (r < 0) strerr_diefu2sys(111,"check ", svok.s) ;
				if (!r)	svsrc.remove = 1 ;
			}
			
			if (UNSUP) svsrc.unsupervise = 1 ;
			if (svsrc.remove) svsrc.unsupervise = 1 ;
			
			if (svsrc.type == CLASSIC)
			{
				if (!genalloc_append(svstat_t,&nclassic,&svsrc)) strerr_diefu3x(111,"add: ",*argv," on genalloc") ;				
			}
			else
			{
				if (!genalloc_append(svstat_t,&nrc,&svsrc)) strerr_diefu3x(111,"add: ",*argv," on genalloc") ;
			}
		}
		stralloc_free(&type) ;
		stralloc_free(&svok) ;
	}

	if (genalloc_len(svstat_t,&nclassic))
	{
		VERBO2 strerr_warni1x("stop svc services ...") ;
		if (!svc_down(base.s,scandir.s,live.s,tree.s,treename,&nclassic,envp))
			strerr_diefu1x(111,"update svc services") ;
		VERBO2 strerr_warni1x("release svc services ...") ;
		if (!svc_release(base.s,scandir.s,live.s,tree.s,treename))
			strerr_diefu1x(111,"release svc services ...") ;
		VERBO2 strerr_warni3x("switch svc services of: ",treename," to source") ;
		if (!svc_switch_to(base.s,tree.s,treename,SS_SWSRC))
			strerr_diefu3x(111,"switch svc service of: ",treename," to source") ;
		
	}
	if (genalloc_len(svstat_t,&nrc))
	{
		VERBO2 strerr_warni1x("stop rc services ...") ;
		if (!rc_down(base.s, scandir.s, live.s,livetree.s,tree.s,treename,&nrc,envp,trc))
			strerr_diefu1x(111,"update rc services") ;
		VERBO2 strerr_warni1x("release rc services ...") ;
		if (!rc_release(base.s,scandir.s, live.s,tree.s,treename))
			strerr_diefu1x(111,"release rc services") ;
		VERBO2 strerr_warni3x("switch rc services of: ",treename," to source") ;
		if (!db_switch_to(base.s,livetree.s,tree.s,treename,envp,SS_SWSRC))
			strerr_diefu5x(111,"switch",livetree.s,"/",treename," to source") ;
	}
		
	if (UNSUP || genalloc_len(stralist,&gatoremove))
	{
		VERBO2 strerr_warnt2x("send signal -an to scandir: ",scandir.s) ;
		if (scandir_send_signal(scandir.s,"an") <= 0)
			strerr_diefu2sys(111,"send signal to scandir: ", scandir.s) ;
	}
	
	stralloc_free(&base) ;
	stralloc_free(&tree) ;
	stralloc_free(&scandir) ;
	stralloc_free(&live) ;
	stralloc_free(&livetree) ;
	genalloc_free(svstat_t,&nrc) ;
	genalloc_free(svstat_t,&nclassic) ;
	
	return 0 ;		
}
	

