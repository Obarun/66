/* 
 * 66-disable.c
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
#include <stdlib.h>

#include <oblibs/obgetopt.h>
#include <oblibs/error2.h>
#include <oblibs/directory.h>
#include <oblibs/types.h>
#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/stralist.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/buffer.h>
#include <skalibs/unix-transactional.h>
#include <skalibs/direntry.h>
#include <skalibs/diuint32.h>
 
#include <66/constants.h>
#include <66/utils.h>
#include <66/enum.h>
#include <66/tree.h>
#include <66/db.h>
#include <66/backup.h>
#include <66/graph.h>

//#include <stdio.h>

static stralloc saresolve = STRALLOC_ZERO ;

unsigned int VERBOSITY = 1 ;

#define USAGE "66-disable [ -h help ] [ -v verbosity ] [ - l live ] [ -t tree ] [ -S ] service(s)"

static inline void info_help (void)
{
  static char const *help =
"66-disable <options> service(s)\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-v: increase/decrease verbosity\n"
"	-l: live directory\n"
"	-t: name of the tree to use\n"
"	-S: disable and stop the service\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
}

static void cleanup(char const *dst)
{
	int e = errno ;
	rm_rf(dst) ;
	errno = e ;
}

int find_logger(genalloc *ga, char const *name, char const *src)
{
	stralloc sa = STRALLOC_ZERO ;
	if (resolve_read(&sa,src,name,"logger"))
	{
		if (!stra_add(ga,sa.s))
		{
			stralloc_free(&sa) ;
			return 0 ;
		}
	}
	stralloc_free(&sa) ;
	return 1 ;
}

int remove_sv(genalloc *toremove, char const *name, char const *src, unsigned int type)
{
	int r ;

	size_t namelen = strlen(name) ;
	size_t srclen = strlen(src) ;
	size_t newlen ;
		
	/** classic service */
	if (type == CLASSIC)
	{
		char dst[srclen + SS_SVC_LEN + 1 + namelen + 1] ;
		memcpy(dst,src,srclen) ;
		memcpy(dst + srclen, SS_SVC, SS_SVC_LEN) ;
		dst[srclen + SS_SVC_LEN]  =  '/' ;
		memcpy(dst + srclen + SS_SVC_LEN + 1, name, namelen) ;
		dst[srclen + SS_SVC_LEN + 1 + namelen] = 0 ;
		
		VERBO3 strerr_warnt3x("Removing ",dst + srclen + 1," service ... ") ;
		if (rm_rf(dst) < 0)
		{
			VERBO3 strerr_warnwu2sys("remove: ", dst) ;
			return 0 ;
		}
				
		return 1 ;
	}
	
	stralloc sa = STRALLOC_ZERO ;
	genalloc gatmp = GENALLOC_ZERO ;// type stralist
	
	graph_t g = GRAPH_ZERO ;
	stralloc sagraph = STRALLOC_ZERO ;
	genalloc tokeep = GENALLOC_ZERO ;
	
	/** rc services */
	{
		/** build dependencies graph*/
		r = graph_type_src(&tokeep,src,1) ;
		if (r <= 0)
		{
			strerr_warnwu2x("resolve source of graph for tree: ",src) ;
			goto err ;
		}
		if (!graph_build(&g,&sagraph,&tokeep,src))
		{
			strerr_warnwu1x("make dependencies graph") ;
			goto err ;
		}
	
		if (!stra_add(toremove,name)) 
		{	 
			VERBO3 strerr_warnwu3x("add: ",name," as dependency to remove") ;
			goto err ;
		}
		
		r = graph_rdepends(toremove,&g,name,src) ;
		if (!r) 
		{
			VERBO3 strerr_warnwu2x("find services depending for: ",name) ;
			goto err ;
		}
		if(r == 2) VERBO3 strerr_warnt2x("any services don't depends of: ",name) ;
		
		if (!stralloc_catb(&sa,src,srclen)) retstralloc(0,"remove_sv") ;
		if (!stralloc_cats(&sa,SS_DB SS_SRC)) retstralloc(0,"remove_sv") ;
		if (!stralloc_cats(&sa, "/")) retstralloc(0,"remove_sv") ;
		newlen = sa.len ;
		if (genalloc_len(stralist,toremove))
			if (!find_logger(toremove,name,src)) goto err ;
			
		for (unsigned int i = 0; i < genalloc_len(stralist,toremove); i++)
		{
			sa.len = newlen ;
			if (!stralloc_cats(&sa,gaistr(toremove,i))) retstralloc(0,"remove_sv") ;
			if (!stralloc_0(&sa)) retstralloc(0,"remove_sv") ;
			VERBO3 strerr_warnt3x("Removing ",sa.s + srclen + 1," service ...") ;
			if (rm_rf(sa.s) < 0)
			{
				VERBO3 strerr_warnwu2sys("remove: ", sa.s) ;
				goto err ;
			}
		}

	}
	
	genalloc_free(vertex_graph_t,&g.stack) ;
	genalloc_free(vertex_graph_t,&g.vertex) ;
	stralloc_free(&sagraph) ;
	stralloc_free(&sa) ;
	genalloc_deepfree(stralist,&gatmp,stra_free) ;
	
	return 1 ;
	
	err:
		genalloc_free(vertex_graph_t,&g.stack) ;
		genalloc_free(vertex_graph_t,&g.vertex) ;
		stralloc_free(&sagraph) ;
		stralloc_free(&sa) ;
		genalloc_deepfree(stralist,&gatmp,stra_free) ;
		return 0 ;
}
		
int main(int argc, char const *const *argv,char const *const *envp)
{
	int r,rb ;
	unsigned int nlongrun, nclassic, stop ;
	
	uid_t owner ;
	
	char *treename = 0 ;
	
	stralloc base = STRALLOC_ZERO ;
	stralloc tree = STRALLOC_ZERO ;
	stralloc workdir = STRALLOC_ZERO ;
	stralloc live = STRALLOC_ZERO ;
	stralloc livetree = STRALLOC_ZERO ;
	
	genalloc ganlong = GENALLOC_ZERO ; //name of longrun service, type stralist
	genalloc ganclassic = GENALLOC_ZERO ; //name of classic service, stralist
	genalloc gadepstoremove = GENALLOC_ZERO ;
	
	graph_t g = GRAPH_ZERO ;
	stralloc sagraph = STRALLOC_ZERO ;
	genalloc tokeep = GENALLOC_ZERO ;
	genalloc master = GENALLOC_ZERO ;
	
	r = nclassic = nlongrun = stop = 0 ;
		
	PROG = "66-disable" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">hv:l:t:S", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'h' : 	info_help(); return 0 ;
				case 'v' :  if (!uint0_scan(l.arg, &VERBOSITY)) exitusage() ; break ;
				case 'l' :  if(!stralloc_cats(&live,l.arg)) retstralloc(111,"main") ;
							if(!stralloc_0(&live)) retstralloc(111,"main") ;
							break ;
				case 't' : 	if(!stralloc_cats(&tree,l.arg)) retstralloc(111,"main") ;
							if(!stralloc_0(&tree)) retstralloc(111,"main") ;
							break ;
				case 'S' :	stop = 1 ;	break ;
				default : exitusage() ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc < 1) exitusage() ;
	
	owner = MYUID ;
	if (!set_ownersysdir(&base,owner)) strerr_diefu1sys(111, "set owner directory") ;
	
	r = tree_sethome(&tree,base.s) ;
	if (r < 0) strerr_diefu1x(110,"find the current tree. You must use -t options") ;
	if (!r) strerr_diefu2sys(111,"find tree: ", tree.s) ;
	
	treename = tree_setname(tree.s) ;
	if (!treename) strerr_diefu1x(111,"set the tree name") ;
		
	if (!tree_get_permissions(tree.s))
		strerr_dief2x(110,"You're not allowed to use the tree: ",tree.s) ;
	
	r = set_livedir(&live) ;
	if (!r) retstralloc(111,"main") ;
	if(r < 0) strerr_dief3x(111,"live: ",live.s," must be an absolute path") ;
	
	if (!stralloc_copy(&livetree,&live)) retstralloc(111,"main") ;
		
	r = set_livetree(&livetree,owner) ;
	if (!r) retstralloc(111,"main") ;
	if(r < 0) strerr_dief3x(111,"livetree: ",livetree.s," must be an absolute path") ;
	
	
	if (!tree_copy(&workdir,tree.s,treename)) strerr_diefu1sys(111,"create tmp working directory") ;
	
	{
		stralloc type = STRALLOC_ZERO ;
		
		for(;*argv;argv++)
		{
			if (!resolve_pointo(&saresolve,base.s,live.s,tree.s,treename,0,SS_RESOLVE_SRC))
			{
				cleanup(workdir.s) ;
				strerr_diefu1x(111,"set revolve pointer to source") ;
			}
			rb = resolve_read(&type,saresolve.s,*argv,"remove") ;
			if (rb) 
			{
				strerr_warni2x(*argv,": is already disabled") ;
				continue ;
			}
			rb = resolve_read(&type,saresolve.s,*argv,"type") ;
			if (rb < -1)
			{
				cleanup(workdir.s) ;
				strerr_dief2x(111,"invalid .resolve directory: ",saresolve.s) ;
			}
			if (rb <= 0)
			{
				cleanup(workdir.s) ;
				strerr_dief2x(111,*argv,": is not enabled") ;
			}
			
			if (get_enumbyid(type.s,key_enum_el) == CLASSIC)
			{
				if (!stra_add(&ganclassic,*argv)) retstralloc(111,"main") ;
				nclassic++ ;
			}					
			if (get_enumbyid(type.s,key_enum_el) >= BUNDLE)
			{
				if (!stra_add(&ganlong,*argv)) retstralloc(111,"main") ;
				nlongrun++ ;
			}
		}
		stralloc_free(&type) ;
	}

	if (nclassic)
	{
		VERBO2 strerr_warni1x("remove svc services ... ") ;
		for (unsigned int i = 0 ; i < genalloc_len(stralist,&ganclassic) ; i++)
		{
			char *name = gaistr(&ganclassic,i) ;
			
			if (!remove_sv(&gadepstoremove,name,workdir.s,CLASSIC))
			{
				cleanup(workdir.s) ;
				strerr_diefu2x(111,"disable: ",name) ;
			}
			
			/** modify the resolve file for 66-stop*/
			if (!resolve_write(workdir.s,gaistr(&ganclassic,i),"remove","",1))
			{
				cleanup(workdir.s) ;
				strerr_diefu2sys(111,"write resolve file: remove for service: ",gaistr(&ganclassic,i)) ;
			}
		}
		char type[UINT_FMT] ;
		size_t typelen = uint_fmt(type, CLASSIC) ;
		type[typelen] = 0 ;
		size_t cmdlen ;
		char cmd[typelen + 6 + 1] ;
		memcpy(cmd,"-t",2) ;
		memcpy(cmd + 2,type,typelen) ;
		cmdlen = 2 + typelen ;
		memcpy(cmd + cmdlen," -b",3) ;
		cmd[cmdlen + 3] = 0 ;
		r = backup_cmd_switcher(VERBOSITY,cmd,treename) ;
		if (r < 0)
		{
			cleanup(workdir.s) ;
			strerr_diefu2sys(111,"find origin of svc: ",treename) ;
		}
		// point to origin
		if (!r)
		{
			stralloc sv = STRALLOC_ZERO ;
			VERBO2 strerr_warni2x("make backup svc of ",treename) ;
			if (!backup_make_new(base.s,tree.s,treename,CLASSIC))
			{
				cleanup(workdir.s) ;
				strerr_diefu2sys(111,"make a backup of db: ",treename) ;
			}
			memcpy(cmd + cmdlen," -s1",4) ;
			cmd[cmdlen + 4] = 0 ;
			r = backup_cmd_switcher(VERBOSITY,cmd,treename) ;
			if (r < 0)
			{
				cleanup(workdir.s) ;
				strerr_diefu3sys(111,"switch current db: ",treename," to source") ;
			}
			/** creer le fichier torelaod*/
			stralloc_free(&sv) ;
		}
		gadepstoremove = genalloc_zero ;
	}
	
	if (nlongrun)
	{	
		
		VERBO2 strerr_warni1x("remove rc services ... ") ;
		for (unsigned int i = 0 ; i < genalloc_len(stralist,&ganlong) ; i++)
		{
			char *name = gaistr(&ganlong,i) ;
			
			if (!remove_sv(&gadepstoremove,name,workdir.s,LONGRUN))
			{
				cleanup(workdir.s) ;
				strerr_diefu2x(111,"disable: ",name) ;
			}
		}

		for (unsigned int i = 0 ; i < genalloc_len(stralist,&gadepstoremove) ; i++ )
		{
			// modify the resolve file for 66-stop*/
			if (!resolve_write(workdir.s,gaistr(&gadepstoremove,i),"remove","",1))
			{
				cleanup(workdir.s) ;
				strerr_diefu2sys(111,"write resolve file: remove for service: ",gaistr(&gadepstoremove,i)) ;
			}
		}
		
		r = graph_type_src(&tokeep,workdir.s,1) ;
		if (!r)
		{
			cleanup(workdir.s) ;
			strerr_diefu2x(111,"resolve source of graph for tree: ",treename) ;
		}
		if (r < 0)
		{
			if (!stra_add(&master,"")) retstralloc(111,"main") ;
		}
		else 
		{
			if (!graph_build(&g,&sagraph,&tokeep,workdir.s))
			{
				cleanup(workdir.s) ;
				strerr_diefu1x(111,"make dependencies graph") ;
			}
			if (!graph_sort(&g))
			{
				cleanup(workdir.s) ;
				strerr_dief1x(111,"cyclic graph detected") ;
			}
			if (!graph_master(&master,&g))
			{
				cleanup(workdir.s) ;
				strerr_dief1x(111,"find master service") ;
			}
		}
		
		
		if (!db_write_contents(&master,SS_MASTER + 1,workdir.s))
		{
			cleanup(workdir.s) ;
			strerr_diefu2x(111,"update bundle: ", SS_MASTER) ;
		}
			
		if (!db_compile(workdir.s,tree.s,treename,envp))
		{
			cleanup(workdir.s) ;
			strerr_diefu4x(111,"compile ",workdir.s,"/",treename) ; 
		}
		
		/** this is an important part, we call s6-rc-update here */
		if (!db_switch_to(base.s,livetree.s,tree.s,treename,envp,SS_SWBACK))
		{
			cleanup(workdir.s) ;
			strerr_diefu3x(111,"switch ",treename," to backup") ;
		}
		
	}
	
	if (!tree_copy_tmp(workdir.s,base.s,live.s,tree.s,treename))
	{
		cleanup(workdir.s) ;
		strerr_diefu4x(111,"copy: ",workdir.s," to: ", tree.s) ;
	}
	
	if (stop)
	{
		size_t arglen = 9 + genalloc_len(stralist,&ganclassic) + genalloc_len(stralist,&ganlong) ;
		char const *newstop[arglen] ;
		unsigned int m = 0 ;
		char fmt[UINT_FMT] ;
		fmt[uint_fmt(fmt, VERBOSITY)] = 0 ;
		
		newstop[m++] = SS_BINPREFIX "66-stop" ;
		newstop[m++] = "-v" ;
		newstop[m++] = fmt ;
		newstop[m++] = "-l" ;
		newstop[m++] = live.s ;
		newstop[m++] = "-t" ;
		newstop[m++] = treename ;
		newstop[m++] = "-u" ;
		/** classic */
		for (unsigned int i = 0 ; i<genalloc_len(stralist,&ganclassic); i++)
			newstop[m++] = gaistr(&ganclassic,i) ;
		/** rc */
		for (unsigned int i = 0 ; i<genalloc_len(stralist,&ganlong); i++)
			newstop[m++] = gaistr(&ganlong,i) ;
		
		newstop[m++] = 0 ;
		
		xpathexec_run (newstop[0], newstop, envp) ;
	}
	
	cleanup(workdir.s) ;

	stralloc_free(&base) ;
	stralloc_free(&tree) ;
	stralloc_free(&workdir) ;
	stralloc_free(&live) ;
	stralloc_free(&livetree) ;
	
	genalloc_deepfree(stralist,&ganlong,stra_free) ;
	genalloc_deepfree(stralist,&ganclassic,stra_free) ;
	genalloc_deepfree(stralist,&gadepstoremove,stra_free) ;
	free(treename) ;
		
	/** graph stuff */
	genalloc_free(vertex_graph_t,&g.stack) ;
	genalloc_free(vertex_graph_t,&g.vertex) ;
	stralloc_free(&sagraph) ;	
	genalloc_deepfree(stralist,&master,stra_free) ;
	genalloc_deepfree(stralist,&tokeep,stra_free) ;
	
	return 0 ;		
}
	

