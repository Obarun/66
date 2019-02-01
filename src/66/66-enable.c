/* 
 * 66-enable.c
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
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/stralist.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/buffer.h>
#include <skalibs/direntry.h>
#include <skalibs/unix-transactional.h>
#include <skalibs/diuint32.h>

#include <66/constants.h>
#include <66/utils.h>
#include <66/enum.h>
#include <66/tree.h>
#include <66/db.h>
#include <66/parser.h>
#include <66/backup.h>
#include <66/svc.h>
#include <66/graph.h>

//#include <stdio.h>

static unsigned int FORCE = 0 ;
unsigned int VERBOSITY = 1 ;

stralloc saresolve = STRALLOC_ZERO ;

#define USAGE "66-enable [ -h help ] [ -v verbosity ] [ - l live ] [ -t tree ] [ -f ] [ -S ] service(s)"

static inline void info_help (void)
{
  static char const *help =
"66-enable <options> service(s)\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-v: increase/decrease verbosity\n"
"	-l: live directory\n"
"	-t: name of the tree to use\n"
"	-f: overwrite service(s)\n"
"	-S: enable and start the service\n"
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

static int start_parser(char const *src,char const *svname,char const *tree, unsigned int *nbsv)
{
	stralloc sasv = STRALLOC_ZERO ;
	
	if (!parse_service_before(src,svname,tree,nbsv,&sasv)) strerr_dief4x(111,"invalid service file: ",src,"/",svname) ;
	
	stralloc_free(&sasv) ;
	
	return 1 ;
}

int main(int argc, char const *const *argv,char const *const *envp)
{
	int r ;
	unsigned int nbsv, nlongrun, nclassic, start ;
	
	uid_t owner ;
	
	char const *src = SS_SERVICE_DIR ;
	char *treename = 0 ;
	
	stralloc base = STRALLOC_ZERO ;//SS_SYSTEM
	stralloc tree = STRALLOC_ZERO ;//-t options
	stralloc workdir = STRALLOC_ZERO ;//working dir directory
	stralloc live = STRALLOC_ZERO ;
	stralloc livetree = STRALLOC_ZERO ;
	stralloc sasrc = STRALLOC_ZERO ;
	genalloc gasrc = GENALLOC_ZERO ; //type diuint32
	genalloc ganlong = GENALLOC_ZERO ; // type stralist
	genalloc ganclassic = GENALLOC_ZERO ; // name of classic service, type stralist
	
	graph_t g = GRAPH_ZERO ;
	stralloc sagraph = STRALLOC_ZERO ;
	genalloc master = GENALLOC_ZERO ;
	genalloc tokeep = GENALLOC_ZERO ;
	
	r = nbsv = nclassic = nlongrun = start = 0 ;
		
	PROG = "66-enable" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">hv:l:t:fS", &l) ;
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
				case 'f' : 	FORCE = 1 ; break ;
				case 'S' :	start = 1 ;	break ;
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
	
	for(;*argv;argv++)
	{
		unsigned int found = 0 ;
		if (!resolve_src(&gasrc,&sasrc,*argv,src,&found)) strerr_diefu2x(111,"resolve source of service file: ",*argv) ;
	}
	
	for (unsigned int i = 0 ; i < genalloc_len(diuint32,&gasrc) ; i++)
		start_parser(sasrc.s + genalloc_s(diuint32,&gasrc)[i].right,sasrc.s + genalloc_s(diuint32,&gasrc)[i].left,tree.s,&nbsv) ;
	
	sv_alltype svblob[nbsv] ;
	
	for (int i = 0;i < nbsv;i++)
		svblob[i] =  genalloc_s(sv_alltype,&gasv)[i] ;
	
	sv_db before = {
		.services = svblob, 
		.nsv = nbsv ,
		.string = keep.s ,
		.stringlen = keep.len ,
		.ndeps = genalloc_len(unsigned int,&gadeps) 
	} ;
	
	{
		if (!tree_copy(&workdir,tree.s,treename)) strerr_diefu1sys(111,"create tmp working directory") ;
		for (unsigned int i = 0; i < before.nsv; i++)
		{
			r = write_services(&before.services[i], workdir.s,FORCE) ;
			if (!r)
			{
				cleanup(workdir.s) ;
				strerr_diefu2x(111,"write service: ",keep.s+before.services[i].cname.name) ;
			}
			if (r > 1) continue ; //service already added
			
			if (before.services[i].cname.itype > CLASSIC)
			{
				if (!stra_add(&ganlong,keep.s + before.services[i].cname.name)) retstralloc(111,"main") ;
				nlongrun++ ;
			}
			else if (before.services[i].cname.itype == CLASSIC) 
			{
				if (!stra_add(&ganclassic,keep.s + before.services[i].cname.name)) retstralloc(111,"main") ;
				nclassic++ ;
			}
		}
		
	}
	
	if (nclassic)
	{
		if (!svc_switch_to(base.s,tree.s,treename,SS_SWBACK))
		{
			cleanup(workdir.s) ;
			strerr_diefu3x(111,"switch ",treename," to backup") ;
		}	
	}
	
	if(nlongrun)
	{
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
			strerr_diefu2x(111,"update bundle: ", SS_MASTER + 1) ;
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
	
	if (start)
	{
		size_t arglen = 8 + genalloc_len(stralist,&ganclassic) + genalloc_len(stralist,&ganlong) ;
		char const *newup[arglen] ;
		unsigned int m = 0 ;
		char fmt[UINT_FMT] ;
		fmt[uint_fmt(fmt, VERBOSITY)] = 0 ;
		
		newup[m++] = SS_BINPREFIX "66-start" ;
		newup[m++] = "-v" ;
		newup[m++] = fmt ;
		newup[m++] = "-l" ;
		newup[m++] = live.s ;
		newup[m++] = "-t" ;
		newup[m++] = treename ;
		/** classic */
		for (unsigned int i = 0 ; i<genalloc_len(stralist,&ganclassic); i++)
			newup[m++] = gaistr(&ganclassic,i) ;
		/** rc */
		for (unsigned int i = 0 ; i<genalloc_len(stralist,&ganlong); i++)
			newup[m++] = gaistr(&ganlong,i) ;
		
		newup[m++] = 0 ;
		
		xpathexec_run (newup[0], newup, envp) ;
	}
	
	cleanup(workdir.s) ;
	
	/** general allocation*/
	freed_parser() ;
	/** inner allocation */
	stralloc_free(&base) ;
	stralloc_free(&tree) ;
	stralloc_free(&workdir) ;
	stralloc_free(&live) ;
	stralloc_free(&livetree) ;
	stralloc_free(&sasrc) ;
	genalloc_free(sv_src_t,&gasrc) ;
	genalloc_deepfree(stralist,&ganclassic,stra_free) ;
	genalloc_deepfree(stralist,&ganlong,stra_free) ;
	free(treename) ;
	
	/** graph stuff */
	genalloc_free(vertex_graph_t,&g.stack) ;
	genalloc_free(vertex_graph_t,&g.vertex) ;
	genalloc_deepfree(stralist,&master,stra_free) ;
	stralloc_free(&sagraph) ;
	genalloc_deepfree(stralist,&tokeep,stra_free) ;
	
	return 0 ;		
}
	

