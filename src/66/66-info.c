/* 
 * 66-info.c
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
#include <locale.h>
#include <langinfo.h>

#include <oblibs/obgetopt.h>
#include <oblibs/error2.h>
#include <oblibs/stralist.h>
#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/directory.h>

#include <skalibs/buffer.h>
#include <skalibs/stralloc.h>
#include <skalibs/unix-transactional.h>
#include <skalibs/direntry.h>
#include <skalibs/types.h>
#include <skalibs/bytestr.h>
#include <skalibs/lolstdio.h>
#include <skalibs/djbunix.h>

#include <66/utils.h>
#include <66/tree.h>
#include <66/constants.h>
#include <66/enum.h>
#include <66/graph.h>
#include <66/resolve.h>

#include <s6/s6-supervise.h>//s6_svc_ok

#include <stdio.h>

unsigned int VERBOSITY = 1 ;
static stralloc base = STRALLOC_ZERO ;
static stralloc live = STRALLOC_ZERO ;
static uid_t owner ;

#define MAXSIZE 4096
#define DBG(...) bprintf(buffer_1, __VA_ARGS__) ;\
				buffer_putflush(buffer_1,"\n",1) ;


#define USAGE "66-info [ -h ] [ -T ] [ -S ] sub-options (use -h as sub-options for futher informations)"

#define TREE_USAGE "66-info -T [ -h ] [ -v verbosity ] [ -r ] [ -d depth ] tree "
#define exit_tree_usage() exitusage(TREE_USAGE)
#define SV_USAGE "66-info -S [ -h ] [ -v verbosity ] [ -l live ] [ -p n lines ] [ -r ] [ -d depth ] service"
#define exit_sv_usage() exitusage(SV_USAGE)

unsigned int REVERSE = 0 ;

graph_style *STYLE = &graph_default ;
unsigned int MAXDEPTH = 1 ;

static inline void info_help (void)
{
  static char const *help =
"66-info <options> sub-options \n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-v: increase/decrease verbosity\n"
"	-T: get informations about tree(s)\n"
"	-S: get informations about service(s)\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
}

static inline void tree_help (void)
{
  static char const *help =
"66-info -T <options> tree\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-v: increase/decrease verbosity\n"
"	-r: reserve the dependencies graph\n" 
"	-d: limit the depth of the graph recursion\n" 
;

 if (buffer_putsflush(buffer_1, help) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
}

static inline void sv_help (void)
{
  static char const *help =
"66-info -S <options> service\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-l: live directory\n"
"	-p: print n last lines of the associated log file\n"
"	-r: reserve the dependencies graph\n" 
"	-d: limit the depth of the graph recursion\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
}

char *print_nlog(char *str, int n) 
{ 
	int r = 0 ;
	int DELIM ='\n' ;
	size_t slen = strlen(str) ;
	
	if (n <= 0) return NULL; 

	size_t ndelim = 0;  
	char *target_pos = NULL;   
	
	r = get_rlen_until(str,DELIM,slen) ;
	
	target_pos = str + r ; 

	if (target_pos == NULL) return NULL; 

	while (ndelim < n) 
	{ 
		while (str < target_pos && *target_pos != DELIM) 
			--target_pos; 

		if (*target_pos ==  DELIM) 
			--target_pos, ++ndelim; 
		else break; 
	} 

	if (str < target_pos) 
		target_pos += 2; 

	return target_pos ;
} 

int print_status(ss_resolve_t *res,char const *treename, char const *const *envp)
{
	int r ;
	int wstat ;
	pid_t pid ;
	
		
	if (res->type == CLASSIC || res->type == LONGRUN)
	{
		r = s6_svc_ok(res->sa.s + res->runat) ;
		if (r != 1)
		{
			if (buffer_putsflush(buffer_1,"not running\n") < 0) return 0 ;
			return 0 ;
		}
		char const *newargv[3] ;
		unsigned int m = 0 ;
		
		newargv[m++] = SS_BINPREFIX "s6-svstat" ;
		newargv[m++] = res->sa.s + res->runat ;
		newargv[m++] = 0 ;
					
		pid = child_spawn0(newargv[0],newargv,envp) ;
		if (waitpid_nointr(pid,&wstat, 0) < 0)
			strerr_diefu2sys(111,"wait for ",newargv[0]) ;
		
		if (wstat)
			strerr_diefu2x(111,"status for service: ",res->sa.s + res->name) ;
	}
	else
	{
		if (buffer_putsflush(buffer_1,"nothing to display\n") < 0) return 0 ;
		return 0 ;
	}
		
	return 1 ;
	
}

/** what = 0 -> only classic
 * what = 1 -> only atomic
 * what = 2 -> both*/
int graph_display(char const *tree,char const *treename,char const *svname,unsigned int what)
{
	
	int r, e ;
	
	graph_t g = GRAPH_ZERO ;
	stralloc sagraph = STRALLOC_ZERO ;
	genalloc tokeep = GENALLOC_ZERO ; //stralist
	
	e = 1 ;
	
	size_t treelen = strlen(tree) ;
	char dir[treelen + SS_SVDIRS_LEN + 1] ;
	memcpy(dir,tree,treelen) ;
	memcpy(dir + treelen,SS_SVDIRS,SS_SVDIRS_LEN) ;
	dir[treelen + SS_SVDIRS_LEN] = 0 ;
	
	r = graph_type_src(&tokeep,dir,what) ;
	if (!r)
	{
		VERBO3 strerr_warnwu2x("resolve source of graph for tree: ",treename) ;
		e = 0 ; goto err ;
	}
	if (r < 0)
		goto empty ;
		
	if (!graph_build(&g,&sagraph,&tokeep,dir))
	{
		VERBO3 strerr_warnwu1x("make dependencies graph") ;
		e = 0 ; goto err ;
	}
	if (graph_sort(&g) < 0)
	{
		VERBO3 strerr_warnw1x("cyclic graph detected") ;
		e= 0 ; goto err ;
	}
	
	if(genalloc_len(vertex_graph_t,&g.stack))
	{	
		if (REVERSE) stack_reverse(&g.stack) ;
		if (buffer_putflush(buffer_1,"",1) < 0){ e = 0 ; goto err ; }
		graph_tree(dir,&g,svname,treename) ;
	}
	
	graph_free(&g) ;
	stralloc_free(&sagraph) ;	
	genalloc_deepfree(stralist,&tokeep,stra_free) ;
	
	return e ;
	
	empty:
		e = -1 ;
	err:
		graph_free(&g) ;
		stralloc_free(&sagraph) ;	
		genalloc_deepfree(stralist,&tokeep,stra_free) ;
		return e ;
}

int tree_args(int argc, char const *const *argv)
{
	int r ;
	genalloc gatree = GENALLOC_ZERO ;// stralist, all tree
	stralloc tree = STRALLOC_ZERO ;
	stralloc sacurrent = STRALLOC_ZERO ;
	stralloc currname = STRALLOC_ZERO ;
	int todisplay = 0 ;
	
	{
		subgetopt_t l = SUBGETOPT_ZERO ;
			
		for (;;)
		{
			int opt = getopt_args(argc,argv, ">hv:rd:", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			
			switch (opt)
			{
				case 'h' : 	tree_help(); return 1 ;
				case 'v' :  if (!uint0_scan(l.arg, &VERBOSITY)) exit_tree_usage() ; break ;
				case 'r' : 	REVERSE = 1 ; break ;
				case 'd' : 	if (!uint0_scan(l.arg, &MAXDEPTH)) exit_tree_usage(); break ;
				default : exit_tree_usage() ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	if (argc > 1) exit_tree_usage();
	
	if (argv[0]) todisplay = 1 ;
	
	r = tree_find_current(&sacurrent,base.s,MYUID) ;
	if (r)
	{
		if (!tree_setname(&currname,sacurrent.s)) strerr_diefu1x(111,"set the tree name") ;
	}
	
	size_t baselen = base.len ;
	char src[baselen + SS_SYSTEM_LEN + 1] ;
	memcpy(src,base.s,baselen) ;
	memcpy(src + baselen,SS_SYSTEM, SS_SYSTEM_LEN) ;
	src[baselen + SS_SYSTEM_LEN] = 0 ;
	
	if (!dir_get(&gatree, src,SS_BACKUP + 1, S_IFDIR)) goto err ;
	if (genalloc_len(stralist,&gatree))
	{
		for (unsigned int i = 0 ; i < genalloc_len(stralist,&gatree) ; i++)
		{
			tree.len = 0 ;
				
			char *treename = gaistr(&gatree,i) ;
			int cu = 0 ;
			
			if (todisplay)
				if (!obstr_equal(treename,argv[0])) continue ;
			
			int enabled = tree_cmd_state(VERBOSITY,"-s",treename) ; 
			if (currname.len)
				cu = obstr_equal(treename,currname.s) ;
			
			if (!bprintf(buffer_1,"%s%s%s%s%s%s%s\n","[Name:",treename,",Current:",cu ? "yes":"no",",Enabled:",enabled?"yes":"no","]")) goto err ;
		
			
			if(!stralloc_cats(&tree,treename)) retstralloc(0,"tree_args") ;
			if(!stralloc_0(&tree)) retstralloc(0,"tree_args") ;
	
			r = tree_sethome(&tree,base.s,MYUID) ;
			if (!r) strerr_diefu2sys(111,"find tree: ", tree.s) ;
			
			r = graph_display(tree.s,treename,"",2) ;
			if (r < 0)
			{
				if (!bprintf(buffer_1," %s ","nothing to display")) goto err ;
				if (buffer_putflush(buffer_1,"\n",1) < 0) goto err ;
			}else if (!r) goto err ;
			if (buffer_putflush(buffer_1,"\n",1) < 0) goto err ;		
		}
	}
	else 
	{
		if (!bprintf(buffer_1," %s ","nothing to display")) goto err ;
		if (buffer_putflush(buffer_1,"\n",1) < 0) goto err ;
	}
	
	stralloc_free(&tree) ;
	stralloc_free(&sacurrent) ;
	stralloc_free(&currname) ;
	genalloc_deepfree(stralist,&gatree,stra_free) ;
	
	return 1 ;
	
	err:
		stralloc_free(&tree) ;
		stralloc_free(&sacurrent) ;
		stralloc_free(&currname) ;
		genalloc_deepfree(stralist,&gatree,stra_free) ;
		return 0 ;
}

int sv_args(int argc, char const *const *argv,char const *const *envp)
{
	int r, found ; 
	unsigned int nlog = 0 ;
	
	stralloc tree = STRALLOC_ZERO ;
	genalloc gawhat = GENALLOC_ZERO ;//stralist
	genalloc gatree = GENALLOC_ZERO ;
	
	ss_resolve_t res = RESOLVE_ZERO ;
	
	char const *svname = 0 ;
	char const *treename = 0 ;
	
	r = found = 0 ;
	
	{
		subgetopt_t l = SUBGETOPT_ZERO ;
		
		for (;;)
		{
			int opt = getopt_args(argc,argv, ">hv:l:p:rd:", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'h' : 	sv_help(); return 1 ;
				case 'v' :  if (!uint0_scan(l.arg, &VERBOSITY)) exit_sv_usage() ; break ;
				case 'l' : 	if (!stralloc_cats(&live,l.arg)) retstralloc(0,"sv_args") ;
							if (!stralloc_0(&live)) retstralloc(0,"sv_args") ;
							break ;
				case 'p' :	if (!uint0_scan(l.arg, &nlog)) exit_sv_usage() ; break ;
				case 'r' : 	REVERSE = 1 ; break ;
				case 'd' : 	if (!uint0_scan(l.arg, &MAXDEPTH)) exit_sv_usage(); break ;
				default : exit_sv_usage() ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	if (argc > 1 || !argc) exit_sv_usage() ;

	svname = *argv ;
	
	r = set_livedir(&live) ;
	if (!r) retstralloc(0,"sv_args") ;
	if (r < 0 ) strerr_dief3x(111,"live: ",live.s," must be an absolute path") ;
	
	size_t baselen = base.len ;
	size_t newlen ;
	
	char src[MAXSIZE] ;
	memcpy(src,base.s,baselen) ;
	memcpy(src + baselen,SS_SYSTEM, SS_SYSTEM_LEN) ;
	baselen = baselen + SS_SYSTEM_LEN ;
	src[baselen] = 0 ;
		
	if (!dir_get(&gatree, src,SS_BACKUP + 1, S_IFDIR)) 
		strerr_diefu2x(111,"get tree from directory: ",src) ;
		
	if (genalloc_len(stralist,&gatree))
	{
		for (unsigned int i = 0 ; i < genalloc_len(stralist,&gatree) ; i++)
		{
			treename = gaistr(&gatree,i) ;
			size_t treelen = gaistrlen(&gatree,i) ;
			src[baselen] = '/' ;
			memcpy(src + baselen + 1,treename,treelen) ;
			memcpy(src + baselen + 1 + treelen,SS_SVDIRS,SS_SVDIRS_LEN) ;
			newlen = baselen + 1 + treelen + SS_SVDIRS_LEN ;
			memcpy(src + baselen + 1 + treelen + SS_SVDIRS_LEN,SS_RESOLVE,SS_RESOLVE_LEN) ;
			src[baselen + 1 + treelen + SS_SVDIRS_LEN + SS_RESOLVE_LEN] = 0 ;
			if (dir_search(src,svname,S_IFREG))
			{
				if(!stralloc_cats(&tree,treename)) retstralloc(0,"sv_args") ;
				if(!stralloc_0(&tree)) retstralloc(0,"sv_args") ;
				src[newlen] = 0 ;
				found = 1 ;
				break ;
			}
		}
	}
	else 
	{
		if (!bprintf(buffer_1," %s ","nothing to display")) goto err ;
		if (buffer_putflush(buffer_1,"\n",1) < 0) goto err ;
		return 1 ;
	}
	if (!found)
	{
		strerr_warnw2x("unknow service: ",svname) ;
		goto err ;
	}
	r = tree_sethome(&tree,base.s,MYUID) ;
	if (!r) strerr_diefu2sys(111,"find tree: ", tree.s) ;
	
	if (!bprintf(buffer_1,"%s%s%s\n","[",svname,"]")) goto err ;
	if (!bprintf(buffer_1,"%s%s\n","on tree : ",treename)) goto err ;
	
	if (!ss_resolve_read(&res,src,svname)) strerr_diefu2sys(111,"read resolve file of: ",svname) ;
	
	/** status */
	if (buffer_putsflush(buffer_1,"status : ") < 0) goto err ;
	print_status(&res,treename,envp) ;
	
	/** print the type */	
	if (!bprintf(buffer_1,"%s %s\n","type :",get_keybyid(res.type))) goto err ;
	
	if (!bprintf(buffer_1,"%s %s\n","description :",res.sa.s + res.description)) goto err ;
	if (!bprintf(buffer_1,"%s%s\n","source : ",res.sa.s + res.src)) goto err ;
	if (!bprintf(buffer_1,"%s%s\n","run at : ",res.sa.s + res.runat)) goto err ;
	if (res.exec_run)
	{
		if (!bprintf(buffer_1,"%s%s\n","start script :",res.sa.s + res.exec_run)) goto err ;
	}
	if (res.exec_finish)
	{
		if (!bprintf(buffer_1,"%s%s\n","finish script :",res.sa.s + res.exec_run)) goto err ;
	}
	
	/** dependencies */
//	if (res.type > CLASSIC) 
	{	
		if (res.type == BUNDLE) 
		{
			if (!bprintf(buffer_1,"%s\n","contents :")) goto err ;
		}
		else if (!bprintf(buffer_1,"%s%i%s\n","direct dependencies : ",res.ndeps," service(s)")) goto err ;
		
		if (res.ndeps)
		{
			if (!graph_display(tree.s,treename,svname,2)) strerr_diefu2x(111,"display graph of tree: ", treename) ;
		}
	}
	
	/** logger */
	if (res.type == CLASSIC || res.type == LONGRUN) 
	{
		if (res.logger)
		{
			if (!bprintf(buffer_1,"%s%s\n","logger associated : ",res.sa.s + res.logger)) goto err ;
			if (!bprintf(buffer_1,"%s ","log destination :")) goto err ;
			if (!bprintf(buffer_1,"%s \n",res.sa.s + res.dstlog)) goto err ;
			if (nlog)
			{
				stralloc log = STRALLOC_ZERO ;
				/** the file current may not exist if the service was never started*/
				if (!dir_search(res.sa.s + res.dstlog,"current",S_IFREG)){	if (!bprintf(buffer_1,"%s \n","log file is empty")) goto err ; }
				else
				{
					if (!file_readputsa(&log,res.sa.s + res.dstlog,"current")) retstralloc(0,"sv_args") ;
					if (!bprintf(buffer_1,"\n")) goto err ;
					if (!bprintf(buffer_1,"%s \n",print_nlog(log.s,nlog))) goto err ;
					stralloc_free(&log) ;
				}
			}
		}
	}
	if (buffer_putsflush(buffer_1,"") < 0) goto err ;
	
	stralloc_free(&tree) ;
	genalloc_deepfree(stralist,&gawhat,stra_free) ;
	genalloc_deepfree(stralist,&gatree,stra_free) ;
	ss_resolve_free(&res) ;
	return 1 ;
	
	err:
		stralloc_free(&tree) ;
		genalloc_deepfree(stralist,&gawhat,stra_free) ;
		genalloc_deepfree(stralist,&gatree,stra_free) ;
		ss_resolve_free(&res) ;
		return 0 ;
}
int main(int argc, char const *const *argv, char const *const *envp)
{
	int what ;
	
	what = -1 ;
		
	PROG = "66-info" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(2,argv, ">hTS", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'h' : 	info_help(); return 0 ;
				case 'T' : 	what = 0 ; break ;
				case 'S' :	what = 1 ; break ;
				default : exitusage(USAGE) ; 
			}
		}
	}
	if (what<0) exitusage(USAGE) ;
	
	owner = MYUID ;
		
	setlocale(LC_ALL, "");
		
	if(!str_diff(nl_langinfo(CODESET), "UTF-8")) {
		STYLE = &graph_utf8;
	}
		
	if (!set_ownersysdir(&base,owner)) strerr_diefu1sys(111, "set owner directory") ;
	
	if (!what)
		if (!tree_args(--argc,++argv)) strerr_diefu1x(111,"display trees informations") ;
	
	if (what)
		if (!sv_args(--argc,++argv,envp)) strerr_diefu1x(111,"display services informations") ;
	
	stralloc_free(&base) ;
	stralloc_free(&live) ;
		
	return 0 ;
}
