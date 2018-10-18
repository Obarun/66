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

#include <66/constants.h>
#include <66/utils.h>
#include <66/enum.h>
#include <66/tree.h>
#include <66/db.h>
#include <66/parser.h>
#include <66/backup.h>
#include <66/svc.h>

#include <stdio.h>
static unsigned int MULTI = 0 ;
static unsigned int FORCE = 0 ;
static char const *MSTART = NULL ;
unsigned int VERBOSITY = 1 ;

stralloc saresolve = STRALLOC_ZERO ;

#define USAGE "66-enable [ -h help ] [ -v verbosity ] [ - l live ] [ -t tree ] [ -f force ] [ -d directory ] service(s)"

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
"	-f: owerwrite service(s)\n"
"	-d: enable an entire directory\n"
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

int main(int argc, char const *const *argv,char const *const *envp)
{
	int r ;
	unsigned int nbsv, nlongrun, nclassic ;
	
	uid_t owner ;
	
	stralloc base = STRALLOC_ZERO ;//SS_SYSTEM
	stralloc tree = STRALLOC_ZERO ;//-t options
	stralloc dir = STRALLOC_ZERO ; //-d options
	stralloc sv_src = STRALLOC_ZERO ;//service src 
	stralloc workdir = STRALLOC_ZERO ;//working dir directory
	stralloc live = STRALLOC_ZERO ;
	stralloc livetree = STRALLOC_ZERO ;
	
	genalloc gargv = GENALLOC_ZERO ;//Multi service as arguments, type stralist
	genalloc ganlong = GENALLOC_ZERO ; // type stralist
	genalloc ganclassic = GENALLOC_ZERO ; // name of classic service, type stralist
	
	r = nbsv = nclassic = nlongrun =  0 ;
		
	PROG = "66-enable" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">hv:l:t:fd:", &l) ;
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
				case 'd' : 	if(!stralloc_cats(&dir,l.arg)) retstralloc(111,"main") ;
							if(!stralloc_0(&dir)) retstralloc(111,"main") ;
							MULTI = 1 ;
							break ;
				default : exitusage() ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc < 1) exitusage() ;
	/**only name of the service to enable is allowed with -d options*/
	if (argc > 1 && MULTI) exitusage() ;
	owner = MYUID ;
	if (!set_ownersysdir(&base,owner)) strerr_diefu1sys(111, "set owner directory") ;
	
	r = tree_sethome(&tree,base.s) ;
	if (r < 0) strerr_diefu1x(110,"find the current tree. You must use -t options") ;
	if (!r) strerr_diefu2sys(111,"find tree: ", tree.s) ;
	
	size_t treelen = get_rlen_until(tree.s,'/',tree.len) ;
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
		
	r = set_livetree(&livetree,owner) ;
	if (!r) retstralloc(111,"main") ;
	if(r < 0) strerr_dief3x(111,"live: ",livetree.s," must be an absolute path") ;
	
	/** relative path of absolute path
	 * if relative use SS_SYSTEM as path*/
	if (!stralloc_cats(&sv_src,SS_SERVICE_DIR)) retstralloc(111,"main") ;
	if (!stralloc_0(&sv_src)) retstralloc(111,"main") ;
	{
		if(MULTI){
			r = dir_scan_absopath(dir.s) ;
			if (r < 0){
				sv_src.len--;
				if (!stralloc_cats(&sv_src,"/")) retstralloc(111,"main") ;
				if (!stralloc_cats(&sv_src,dir.s)) retstralloc(111,"main") ;
				if (!stralloc_0(&sv_src)) retstralloc(111,"main") ;
			}else
				if (!stralloc_obreplace(&sv_src,dir.s)) retstralloc(111,"main") ;
		}
	}		
	stralloc_free(&dir) ;
	
	/** get all service on sv_src directory*/
	if (MULTI){
		if (!file_get_fromdir(&gargv,sv_src.s)) strerr_diefu2sys(111,"get services from directory: ",sv_src.s) ;
		MSTART = *argv ;
	}
	else{
		for(;*argv;argv++)
			if (!stra_add(&gargv,*argv)) retstralloc(111,"main") ;
	}
	
	/** parse all the files*/
	{
		stralloc sasv = STRALLOC_ZERO ;
		for(unsigned int i=0;i<genalloc_len(stralist,&gargv);i++)
		{
			if (!parse_service_before(sv_src.s, gaistr(&gargv,i),tree.s,&keep, &nbsv, &sasv)) strerr_dief4x(111,"invalid service file: ",sv_src.s,"/",gaistr(&gargv,i)) ;
		}
		stralloc_free(&sasv) ;
	}


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
			VERBO2 strerr_warni3x("add ",keep.s + before.services[i].cname.name," service ...") ;
			r = write_services(&before.services[i], workdir.s,tree.s,FORCE) ;
			if (!r)
			{
				cleanup(workdir.s) ;
				strerr_diefu2x(111,"write service: ",keep.s+before.services[i].cname.name) ;
			}
			if (r > 1) continue ;
			if (before.services[i].cname.itype > CLASSIC)
			{
				if (!stra_add(&ganlong,keep.s + before.services[i].cname.name)) retstralloc(111,"main") ;
				nlongrun++ ;
			}
			else if (before.services[i].cname.itype == CLASSIC) 
			{
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
		stralloc newupdate = STRALLOC_ZERO ;
		if (!stralloc_cats(&newupdate,"-a -D ")) retstralloc(111,"main") ;
		if (!stralloc_cats(&newupdate,workdir.s)) retstralloc(111,"main") ;
		
		if (MULTI)
		{
			if (!stralloc_cats(&newupdate," ")) retstralloc(111,"main") ;
			if (!stralloc_cats(&newupdate,MSTART)) retstralloc(111,"main") ;
			if (!stralloc_0(&newupdate)) retstralloc(111,"main") ;
			if (db_cmd_master(VERBOSITY,newupdate.s) != 1)
			{
					cleanup(workdir.s) ;
					strerr_diefu1x(111,"update bundle Start") ;
			}
		}
		else
		{
			for (unsigned int i = 0 ; i < genalloc_len(stralist,&ganlong) ; i++)
			{
				char *name = gaistr(&ganlong,i) ;
				if (!stralloc_cats(&newupdate," ")) retstralloc(111,"main") ;
				if (!stralloc_cats(&newupdate,name)) retstralloc(111,"main") ;
				
			}
			if (!stralloc_0(&newupdate)) retstralloc(111,"main") ;
		
			if (db_cmd_master(VERBOSITY,newupdate.s) != 1)
			{
					cleanup(workdir.s) ;
					strerr_diefu1x(111,"update bundle Start") ;
			}	
		}
		stralloc_free(&newupdate) ;
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
	
	stralloc swap = stralloc_zero ;
	size_t svdirlen ;	
	char svdir[tree.len + SS_SVDIRS_LEN + SS_RESOLVE_LEN + 1] ;
	memcpy(svdir,tree.s,tree.len--) ;//tree.len-1 to remove 0's stralloc
	memcpy(svdir + tree.len,SS_SVDIRS,SS_SVDIRS_LEN) ;
	svdirlen = tree.len + SS_SVDIRS_LEN ;
	memcpy(svdir + svdirlen,SS_SVC, SS_SVC_LEN) ;
	svdir[svdirlen + SS_SVC_LEN] = 0 ;
	/** svc */
	if (rm_rf(svdir) < 0)
	{
		if (!resolve_pointo(&saresolve,base.s,live.s,tree.s,treename,CLASSIC,SS_RESOLVE_SRC))
			strerr_diefu1x(111,"set revolve pointer to source") ;
		
		if (!resolve_pointo(&swap,base.s,live.s,tree.s,treename,CLASSIC,SS_RESOLVE_BACK))
			strerr_diefu1x(111,"set revolve pointer to source") ;
		if (!hiercopy(swap.s,saresolve.s))
		{
			cleanup(workdir.s) ;
			strerr_diefu4sys(111,"to copy tree: ",saresolve.s," to ", swap.s) ;
		}
		cleanup(workdir.s) ;
		strerr_diefu2sys(111,"remove directory: ", svdir) ;
	}
	/** db */
	memcpy(svdir + svdirlen,SS_DB, SS_DB_LEN) ;
	svdir[svdirlen + SS_DB_LEN] = 0 ;
	if (rm_rf(svdir) < 0)
	{
		if (!resolve_pointo(&saresolve,base.s,live.s,tree.s,treename,LONGRUN,SS_RESOLVE_SRC))
			strerr_diefu1x(111,"set revolve pointer to source") ;
		
		if (!resolve_pointo(&swap,base.s,live.s,tree.s,treename,LONGRUN,SS_RESOLVE_BACK))
			strerr_diefu1x(111,"set revolve pointer to source") ;
		if (!hiercopy(swap.s,saresolve.s))
		{
			cleanup(workdir.s) ;
			strerr_diefu4sys(111,"to copy tree: ",saresolve.s," to ", swap.s) ;
		}
		cleanup(workdir.s) ;
		strerr_diefu2sys(111,"remove directory: ", svdir) ;
	}	
	/** resolve */
	memcpy(svdir + svdirlen,SS_RESOLVE,SS_RESOLVE_LEN) ;
	svdir[svdirlen + SS_RESOLVE_LEN] = 0 ;
	size_t workdirlen = workdir.len - 1;
	char srcresolve[workdirlen + SS_RESOLVE_LEN + 1] ;
	memcpy(srcresolve,workdir.s,workdirlen) ;
	memcpy(srcresolve + workdirlen,SS_RESOLVE,SS_RESOLVE_LEN) ;
	srcresolve[workdirlen + SS_RESOLVE_LEN] = 0 ;
		
	if (!dir_cmpndel(srcresolve,svdir,""))
	{
		cleanup(workdir.s) ;
		strerr_diefu4sys(111,"compare resolve directory: ",srcresolve," to: ",svdir) ;
	}
	svdir[svdirlen] = 0 ;
		
	if (!hiercopy(workdir.s,svdir))
	{
		cleanup(workdir.s) ;
		strerr_diefu4sys(111,"to copy tree: ",workdir.s," to ", svdir) ;
	}
			
	cleanup(workdir.s) ;
		
	/** general allocation*/
	freed_parser() ;
	/** inner allocation */
	stralloc_free(&base) ;
	stralloc_free(&tree) ;
	stralloc_free(&dir) ;
	stralloc_free(&sv_src) ;
	stralloc_free(&workdir) ;
	stralloc_free(&live) ;
	stralloc_free(&livetree) ;
	stralloc_free(&swap) ;
	genalloc_deepfree(stralist,&ganclassic,stra_free) ;
	genalloc_deepfree(stralist,&ganlong,stra_free) ;
	
	return 0 ;		
}
	

