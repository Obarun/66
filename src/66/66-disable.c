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

#include <66/constants.h>
#include <66/utils.h>
#include <66/enum.h>
#include <66/tree.h>
#include <66/db.h>
#include <66/backup.h>

#include <stdio.h>
stralloc keep = STRALLOC_ZERO ;
stralloc deps = STRALLOC_ZERO ;
genalloc gadeps = GENALLOC_ZERO ;
genalloc graph = GENALLOC_ZERO ;
stralloc saresolve = STRALLOC_ZERO ;

unsigned int VERBOSITY = 1 ;

#define USAGE "66-enable [ -h help ] [ -v verbosity ] [ - l live ] [ -t tree ] service(s)"

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
;

 if (buffer_putsflush(buffer_1, help) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
}

typedef struct deps_graph_s deps_graph_t, *deps_graph_t_ref ;
struct deps_graph_s
{
	unsigned int name ; //pos in keep, name of the service
	unsigned int ndeps ; // number of deps 
	unsigned int ideps ; //pos in genalloc gadeps, id of 1 first deps
} ;

static void cleanup(char const *dst)
{
	int e = errno ;
	rm_rf(dst) ;
	errno = e ;
}


int make_depends_graph(char const *src)
{
	int fdsrc ;
	
	size_t srclen = strlen(src) ;
	char solve[srclen + SS_RESOLVE_LEN + 1] ;
	memcpy(solve,src,srclen) ;
	memcpy(solve + srclen, SS_RESOLVE,SS_RESOLVE_LEN) ;
	solve[srclen + SS_RESOLVE_LEN] = 0 ;
	
	stralloc ndeps = STRALLOC_ZERO ;
	genalloc gatmp = GENALLOC_ZERO ;//stralist
	genalloc tokeep = GENALLOC_ZERO ; //stralist
	deps_graph_t gdeps = { 0 , 0 , 0 } ; 
	
	DIR *dir = opendir(solve) ;
	if (!dir)
	{
		VERBO3 strerr_warnwu2sys("to open : ", solve) ;
		return 0 ;
	}
	fdsrc = dir_fd(dir) ;
		
	for (;;)
    {
		struct stat st ;
		direntry *d ;
		d = readdir(dir) ;
		if (!d) break ;
		if (d->d_name[0] == '.')
		if (((d->d_name[1] == '.') && !d->d_name[2]) || !d->d_name[1])
			continue ;
		if (stat_at(fdsrc, d->d_name, &st) < 0)
		{
			VERBO3 strerr_warnwu3sys("stat ", src, d->d_name) ;
			goto errdir ;
		}
		if (!S_ISDIR(st.st_mode)) continue ;
		
		if (!stra_add(&tokeep,d->d_name))
		{
			VERBO3 strerr_warnwu3sys("add: ",d->d_name," to the dependencies graph") ;
			goto errdir ;
		}
	}
	dir_close(dir) ;
	solve[srclen] = 0 ;
	for (unsigned int i = 0 ; i < genalloc_len(stralist,&tokeep) ; i++)
	{
		
		if (resolve_read(&ndeps,src,gaistr(&tokeep,i),"deps"))
		{
			if (!clean_val(&gatmp,ndeps.s))
			{
				VERBO3 strerr_warnwu2x("clean val: ",ndeps.s) ;
				goto err ;
			}
		}
		else continue ;
		gdeps.name = keep.len ;
		if (!stralloc_catb(&keep, gaistr(&tokeep,i), gaistrlen(&tokeep,i) + 1)) retstralloc(0,"make_depends_graph") ;
					
		/** remove previous ndeps */
		gdeps.ndeps = 0 ;
		gdeps.ideps = genalloc_len(unsigned int, &gadeps) ;
		for (unsigned int i = 0 ; i < genalloc_len(stralist,&gatmp);i++)
		{
			if (!genalloc_append(unsigned int,&gadeps,&deps.len)) retstralloc(0,"make_depends_graph") ;
			if (!stralloc_catb(&deps,gaistr(&gatmp,i),gaistrlen(&gatmp,i) + 1)) retstralloc(0,"make_depends_graph") ;
			gdeps.ndeps++ ;
		}
		if (!genalloc_append(deps_graph_t,&graph,&gdeps)) retstralloc(0,"make_depends_graph") ;
		gatmp = genalloc_zero ;
	}
	
	genalloc_deepfree(stralist,&gatmp,stra_free) ;
	genalloc_deepfree(stralist,&tokeep,stra_free) ;
	stralloc_free(&ndeps) ;
	
	return 1 ;
	
	errdir:
		dir_close(dir) ;
	err:
		genalloc_deepfree(stralist,&gatmp,stra_free) ;
		genalloc_deepfree(stralist,&tokeep,stra_free) ;
		stralloc_free(&ndeps) ;
		return 0 ;
}

int find_logger(genalloc *gakeep, char const *src, char const *service)
{
	if (resolve_read(&saresolve,src,service,"logger"))
	{
		if (!stra_add(gakeep,saresolve.s))
			return 0 ;
	}
	
	return 1 ;
}

int find_rdeps(genalloc *gakeep, char const *src, char const *name)
{
	for (unsigned int i = 0 ; i < genalloc_len(deps_graph_t,&graph) ; i++)
	{
		if (obstr_equal(name,keep.s + genalloc_s(deps_graph_t,&graph)[i].name)) continue ;
			
		for (unsigned int k = 0; k < genalloc_s(deps_graph_t,&graph)[i].ndeps; k++)
		{
			if (obstr_equal(name,deps.s + genalloc_s(unsigned int,&gadeps)[genalloc_s(deps_graph_t,&graph)[i].ideps+k]))
			{
				if (!stra_add(gakeep,keep.s + genalloc_s(deps_graph_t,&graph)[i].name)) 
				{ 
					VERBO3 strerr_warnwu3x("add: ",keep.s + genalloc_s(deps_graph_t,&graph)[i].name," as dependency to remove") ;
					return 0 ;
				}
				if (!find_logger(gakeep,src,keep.s + genalloc_s(deps_graph_t,&graph)[i].name)) 
				{
					VERBO3 strerr_warnwu3x("add: ",gaistr(gakeep,genalloc_len(stralist,gakeep)-1)," as dependency to remove") ;
					return 0 ;
				}
				if (find_rdeps(gakeep,src,keep.s + genalloc_s(deps_graph_t,&graph)[i].name))
					return 0 ;
			}	
		}
	}
		
	return 1 ;
}

int remove_sv(char const *src, char const *name,unsigned int type,genalloc *toremove)
{
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
	
	/** rc services */
	{
		
		if (!find_rdeps(toremove,src,name)) VERBO3 strerr_warnwu2x("resolve dependencies for: ",name) ;
		
		if (!stralloc_catb(&sa,src,srclen)) retstralloc(0,"remove_sv") ;
		if (!stralloc_cats(&sa,SS_DB SS_SRC)) retstralloc(0,"remove_sv") ;
		if (!stralloc_cats(&sa, "/")) retstralloc(0,"remove_sv") ;
		newlen = sa.len ;
		if (!find_logger(toremove,src,name)) return 0 ;
		for (unsigned int i = 0; i < genalloc_len(stralist,toremove); i++)
		{
			
			sa.len = newlen ;
			if (!stralloc_cats(&sa,gaistr(toremove,i))) retstralloc(0,"remove_sv") ;
			if (!stralloc_0(&sa)) retstralloc(0,"remove_sv") ;
			VERBO3 strerr_warnt3x("Removing ",sa.s + srclen + 1," service ...") ;
			if (rm_rf(sa.s) < 0)
			{
				VERBO3 strerr_warnwu2sys("remove: ", sa.s) ;
				return 0 ;
			}
		}
		/** remove the service itself */
		sa.len = newlen ;
		if (!stralloc_cats(&sa,name)) retstralloc(0,"remove_sv") ;
		if (!stralloc_0(&sa)) retstralloc(0,"remove_sv") ;
		VERBO3 strerr_warnt3x("Removing ",sa.s + srclen + 1," service... ") ;
		if (rm_rf(sa.s) < 0)
		{
			VERBO3 strerr_warnwu2sys("remove: ", sa.s) ;
			return 0 ;
		}
	}
	stralloc_free(&sa) ;
	genalloc_deepfree(stralist,&gatmp,stra_free) ;
	
	return 1 ;
}
		
int main(int argc, char const *const *argv,char const *const *envp)
{
	int r,rb ;
	unsigned int nlongrun, nclassic ;
	
	uid_t owner ;
	
	stralloc base = STRALLOC_ZERO ;
	stralloc tree = STRALLOC_ZERO ;
	stralloc workdir = STRALLOC_ZERO ;
	stralloc live = STRALLOC_ZERO ;
	stralloc livetree = STRALLOC_ZERO ;
	
	genalloc ganlong = GENALLOC_ZERO ; //name of longrun service, type stralist
	genalloc ganclassic = GENALLOC_ZERO ; //name of classic service, stralist
	genalloc gadepstoremove = GENALLOC_ZERO ;
	r = nclassic = nlongrun = 0 ;
		
	PROG = "66-disable" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">hv:l:t:", &l) ;
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
	
	size_t treelen = get_rlen_until(tree.s,'/',tree.len) ;
	size_t treenamelen = (tree.len - 1) - treelen ;
	char treename[treenamelen + 1] ;
	memcpy(treename, tree.s + treelen + 1,treenamelen) ;
	treenamelen-- ;
	treename[treenamelen] = 0 ;
		
	if (!tree_get_permissions(tree.s))
		strerr_dief2x(110,"You're not allowed to use the tree: ",tree.s) ;
	
	r = set_livedir(&live) ;
	if (!r) return 111 ;
	if(r < 0) strerr_dief3x(111,"live: ",live.s," must be an absolute path") ;
	
	if (!stralloc_copy(&livetree,&live)) retstralloc(111,"main") ;
		
	r = set_livetree(&livetree,owner) ;
	if (!r) retstralloc(111,"main") ;
	if(r < 0) strerr_dief3x(111,"live: ",livetree.s," must be an absolute path") ;
	
	
	if (!tree_copy(&workdir,tree.s,treename)) strerr_diefu1sys(111,"create tmp working directory") ;
	
	/** retrieve dependencies */
	{
		if (!resolve_pointo(&saresolve,base.s,live.s,tree.s,treename,0,SS_RESOLVE_SRC))
			strerr_diefu1x(111,"set revolve pointer to source") ;
		if (!make_depends_graph(saresolve.s)) strerr_diefu1x(111,"make dependencies graph") ;
	}
	
	{
		stralloc type = STRALLOC_ZERO ;
		
		for(;*argv;argv++)
		{
			if (!resolve_pointo(&saresolve,base.s,live.s,tree.s,treename,0,SS_RESOLVE_SRC))
				strerr_diefu1x(111,"set revolve pointer to source") ;
			
			/*rb = resolve_read(&type,saresolve.s,*argv,"remove") ;
			if (rb < -1) strerr_dief2x(111,"invalid .resolve directory: ",saresolve.s) ;
			if (rb >= 1)
			{
				VERBO2 strerr_warni2x(*argv,": already disabled") ;
				return 0 ;
			}*/
			rb = resolve_read(&type,saresolve.s,*argv,"type") ;
			if (rb < -1) strerr_dief2x(111,"invalid .resolve directory: ",saresolve.s) ;
			if (rb <= 0) strerr_dief2x(111,*argv,": is not enabled") ;
			if (get_enumbyid(type.s,key_enum_el) == CLASSIC)
			{
				if (!stra_add(&ganclassic,*argv)) retstralloc(111,"main") ;
				nclassic++ ;
			}					
			if (get_enumbyid(type.s,key_enum_el) >= LONGRUN)
			{
				if (!stra_add(&ganlong,*argv)) retstralloc(111,"main") ;
				nlongrun++ ;
			}
		}
		stralloc_free(&type) ;
	}

	size_t wlen = workdir.len - 1 ;
	size_t tlen = tree.len - 1 ;
	
	char src[wlen + SS_DB_LEN + SS_SRC_LEN + 1] ;
	memcpy(src,workdir.s,wlen) ;
	
/*	char dst[tlen + SS_SVDIRS_LEN + SS_DB_LEN + SS_SRC_LEN + SS_MASTER_LEN + 1] ;
	memcpy(dst,tree.s,tlen) ;
	memcpy(dst + tlen, SS_SVDIRS, SS_SVDIRS_LEN) ;*/

	if (nclassic)
	{
		for (unsigned int i = 0 ; i < genalloc_len(stralist,&ganclassic) ; i++)
		{
			char *name = gaistr(&ganclassic,i) ;
			VERBO2 strerr_warni1x("remove svc services ... ") ;
			if (!remove_sv(workdir.s,name,CLASSIC,&gadepstoremove))
				strerr_diefu2x(111,"disable: ",name) ;
			/** modify the resolve file for 66-stop*/
			if (!resolve_write(workdir.s,gaistr(&ganclassic,i),"remove","",1)) 
				strerr_diefu2sys(111,"write resolve file: remove for service: ",gaistr(&ganclassic,i)) ;
		}
		
		r = backup_cmd_switcher(VERBOSITY,"-t30 -b",treename) ;
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
			r = backup_cmd_switcher(VERBOSITY,"-t30 -s1",treename) ;
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
		stralloc newupdate = STRALLOC_ZERO ;
		
		if (!stralloc_cats(&newupdate,"-d -D ")) retstralloc(111,"main") ;
		if (!stralloc_cats(&newupdate,workdir.s)) retstralloc(111,"main") ;
		
		for (unsigned int i = 0 ; i < genalloc_len(stralist,&ganlong) ; i++)
		{
			char *name = gaistr(&ganlong,i) ;
			if (!remove_sv(workdir.s,name,LONGRUN,&gadepstoremove))
				strerr_diefu2x(111,"disable: ",name) ;
			if (!stralloc_cats(&newupdate," ")) retstralloc(111,"main") ;
			if (!stralloc_cats(&newupdate,name)) retstralloc(111,"main") ;
			if (!resolve_write(workdir.s,gaistr(&ganlong,i),"remove","",1)) 
				strerr_diefu2sys(111,"write resolve file: remove for service: ",gaistr(&ganlong,i)) ;
			
		}
		for (unsigned int i = 0 ; i < genalloc_len(stralist,&gadepstoremove) ; i++ )
		{
			if (!stralloc_cats(&newupdate," ")) retstralloc(111,"main") ;
			if (!stralloc_cats(&newupdate,gaistr(&gadepstoremove,i))) retstralloc(111,"main") ;
		
			/** modify the resolve file for 66-stop*/
			if (!resolve_write(workdir.s,gaistr(&gadepstoremove,i),"remove","",1)) 
				strerr_diefu2sys(111,"write resolve file: remove for service: ",gaistr(&gadepstoremove,i)) ;
		}
		if (!stralloc_0(&newupdate)) retstralloc(111,"main") ;
			
		if (db_cmd_master(VERBOSITY,newupdate.s) != 1)
		{
				cleanup(workdir.s) ;
				strerr_diefu1x(111,"update bundle Start") ;
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
			VERBO3 strerr_warnwu3x("switch ",treename," to backup") ;
			return 0 ;
		}
		
	}
	
	size_t svdirlen ;
	char svdir[tlen + SS_SVDIRS_LEN + SS_SVC_LEN + 1] ;
	memcpy(svdir,tree.s,tlen) ;
	memcpy(svdir + tlen,SS_SVDIRS,SS_SVDIRS_LEN) ;
	svdirlen = tlen + SS_SVDIRS_LEN ;
	memcpy(svdir + svdirlen,SS_SVC, SS_SVC_LEN) ;
	svdir[svdirlen + SS_SVC_LEN] = 0 ;
	
	stralloc swap = stralloc_zero ;
	
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
		if (!resolve_pointo(&saresolve,base.s,live.s,tree.s,treename,0,SS_RESOLVE_SRC))
			strerr_diefu1x(111,"set revolve pointer to source") ;
		
		if (!resolve_pointo(&swap,base.s,live.s,tree.s,treename,0,SS_RESOLVE_BACK))
			strerr_diefu1x(111,"set revolve pointer to source") ;
		if (!hiercopy(swap.s,saresolve.s))
		{
			cleanup(workdir.s) ;
			strerr_diefu4sys(111,"to copy tree: ",saresolve.s," to ", swap.s) ;
		}
		cleanup(workdir.s) ;
		strerr_diefu4sys(111,"to copy tree: ",workdir.s," to ", svdir) ;
	}
	
	cleanup(workdir.s) ;
	
	stralloc_free(&base) ;
	stralloc_free(&tree) ;
	stralloc_free(&workdir) ;
	stralloc_free(&live) ;
	stralloc_free(&livetree) ;
	stralloc_free(&swap) ;
	genalloc_deepfree(stralist,&ganlong,stra_free) ;
	genalloc_deepfree(stralist,&ganclassic,stra_free) ;
	
	return 0 ;		
}
	

