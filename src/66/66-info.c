/* 
 * 66-info.c
 * 
 * Copyright (c) 2018-2019 Eric Vidal <eric@obarun.org>
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
static uid_t OWNER ;
static char OWNERSTR[UID_FMT] ;

#define MAXSIZE 4096
#define DBG(...) bprintf(buffer_1, __VA_ARGS__) ;\
				buffer_putflush(buffer_1,"\n",1) ;


#define USAGE "66-info [ -h ] [ -T ] [ -S ] sub-options (use -h as sub-options for futher informations)"

#define TREE_USAGE "66-info -T [ -h ] [ -v verbosity ] [ -r ] [ -d depth ] tree "
#define exit_tree_usage() exitusage(TREE_USAGE)
#define SV_USAGE "66-info -S [ -h ] [ -v verbosity ] [-t tree ] [ -l live ] [ -p n lines ] [ -r ] [ -d depth ] service"
#define exit_sv_usage() exitusage(SV_USAGE)

unsigned int REVERSE = 0 ;

graph_style *STYLE = &graph_default ;
unsigned int MAXDEPTH = 1 ;

typedef struct depth_s depth_t ;
struct depth_s
{
	depth_t *prev ;
	depth_t *next ;
	int level ;
} ;

struct set_color {
	const char *back_blue ;
	const char *yellow ;
	const char *blue ;
	const char *blink_blue ;
	const char *off ;
} ;

static struct set_color use_color = {
	"\033[44m", // background blue 
	"\033[38;5;226m", // yellow 
	"\033[38;5;117m", // blue 
	"\033[5;38;5;117m", // blink blue
	"\033[0m" //off
} ;

static struct set_color no_color = {
	"",
	"",
	"",
	"",
	""
} ;

static struct set_color *color = &no_color;

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
"	-t: tree to use\n"
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
			return 1 ;
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
	else if (buffer_putsflush(buffer_1,"nothing to display\n") < 0) return 0 ;
			
	return 1 ;
}

static void info_print_text(ss_resolve_t *res, depth_t *depth, int last)
{
	s6_svstatus_t status = S6_SVSTATUS_ZERO ;
	char *name = res->sa.s + res->name ;
	if (res->type == CLASSIC || res->type == LONGRUN)
	{
		s6_svstatus_read(res->sa.s + res->runat ,&status) ;
	}
	else status.pid = 0 ;	
	
	const char *tip = "" ;
	int level = 1 ;
		
	if(depth->level > 0)
	{
		tip = last ? STYLE->last : STYLE->tip;
	
		while(depth->prev)
			depth = depth->prev;
		
	
		while(depth->next)
		{
			if (!bprintf(buffer_1,"%*s%-*s",STYLE->indent * (depth->level - level), "", STYLE->indent, STYLE->limb)) return ;
			level = depth->level + 1;
			depth = depth->next;
		} 
	}

	if(depth->level > 0)
	{
		if (!bprintf(buffer_1,"%*s%s(%s%i%s,%s) %s", STYLE->indent * (depth->level - level), "", tip, status.pid ? color->blue : color->off,status.pid ? status.pid : 0,color->off,get_keybyid(res->type), name)) return ;
	}
	else if (!bprintf(buffer_1,"%s(%i,%s) %s",tip, status.pid ? status.pid : 0, get_keybyid(res->type),name)) return ;

	if (buffer_putsflush(buffer_1,"\n") < 0) return ; 
}
void info_print_title(char const *name)
{	
	size_t half = 17 ;
	size_t tlen = strlen(name) ;
	size_t htlen = tlen/2 ;
	size_t paddingl ;
	size_t paddingr ;
	if (tlen > half) paddingr = 0 ;
	paddingl = half + htlen ;
	paddingr = paddingl - htlen < half ? half : (half-htlen) ;
	
	if (!bprintf(buffer_1,"%s%*s%*s",color->back_blue,paddingl,name,paddingr,color->off)) return ;
	if (buffer_putsflush(buffer_1,"\n") < 0) return ; 
}
void info_print_tree(char const *treename,int init,int current,int enabled)
{
	info_print_title(treename) ;
	if (!bprintf(buffer_1,"%s%s%s%s%1s","Initialized: ",init ? color->blue : color->yellow, init ? "yes":"no",color->off," | ")) return ;
	if (!bprintf(buffer_1,"%s%s%s%s","Current: ",current ? color->blink_blue : color->yellow ,current ? "yes":"no",color->off)) return ;
	if (buffer_putsflush(buffer_1,"\n") < 0) return ; 
	if (!bprintf(buffer_1,"%s%9s%s%s%s%s","Contains:"," | ","Enabled: ",enabled ? color->blink_blue : color->yellow ,enabled ? "yes":"no",color->off)) return ;
	if (buffer_putsflush(buffer_1,"\n") < 0) return  ;
} 

void info_walk(ss_resolve_t *res,char const *src,int reverse, depth_t *depth)
{
	genalloc gadeps = GENALLOC_ZERO ;
	ss_resolve_t dres = RESOLVE_ZERO ;
	
	if((!res->ndeps) || (depth->level > MAXDEPTH))
		goto err ;

	if (!clean_val(&gadeps,res->sa.s + res->deps)) goto err ;
	if (reverse) genalloc_reverse(stralist,&gadeps) ;
	for(unsigned int i = 0 ; i < res->ndeps ; i++ )
	{	
		int last =  i + 1 < res->ndeps  ? 0 : 1 ;		
		char *name = gaistr(&gadeps,i) ;
		
		if (!ss_resolve_check(src,name)) goto err ;	
		if (!ss_resolve_read(&dres,src,name)) goto err ;
		
		info_print_text(&dres, depth, last) ;
						
		if (dres.ndeps)
		{
			depth_t d =
			{
				depth,
				NULL,
				depth->level + 1 	
			} ;
			depth->next = &d;
			
			if(last)
			{
				if(depth->prev)
				{
					depth->prev->next = &d;
					d.prev = depth->prev;
					depth = &d;
										
				}
				else 
					d.prev = NULL;
			}
			info_walk(&dres,src,reverse,&d);
			depth->next = NULL;
		}
	}
	
	err: 
		ss_resolve_free(&dres) ;
		genalloc_deepfree(stralist,&gadeps,stra_free) ;
}
/** what = 0 -> complete tree
 * what = 1 -> only name */
int graph_display(char const *tree,char const *name,unsigned int what)
{
	
	int e, found ;

	ss_resolve_t res = RESOLVE_ZERO ;
	genalloc tokeep = GENALLOC_ZERO ; //stralist
	
	e = 1 ;
	found = 0 ;
	
	size_t treelen = strlen(tree) ;
	char src[treelen + SS_SVDIRS_LEN + 1] ;
	memcpy(src,tree,treelen) ;
	memcpy(src + treelen,SS_SVDIRS,SS_SVDIRS_LEN) ;
	src[treelen + SS_SVDIRS_LEN] = 0 ;
	char srcres[treelen + SS_SVDIRS_LEN + SS_RESOLVE_LEN + 1] ;
	memcpy(srcres,tree,treelen) ;
	memcpy(srcres + treelen,SS_SVDIRS,SS_SVDIRS_LEN) ;
	memcpy(srcres + treelen + SS_SVDIRS_LEN,SS_RESOLVE,SS_RESOLVE_LEN) ;
	srcres[treelen + SS_SVDIRS_LEN + SS_RESOLVE_LEN] = 0 ;
	
	depth_t d = {
		NULL,
		NULL,
		1
	} ;	
	
	if (!what)
	{
		if (!dir_get(&tokeep,srcres,SS_MASTER+1,S_IFREG)) goto err ;
		if (genalloc_len(stralist,&tokeep))
		{
			for (unsigned int i = 0 ; i < genalloc_len(stralist,&tokeep) ; i++)
			{
				depth_t id = { NULL, NULL, 1 } ;
				if (!ss_resolve_check(src,gaistr(&tokeep,i))) goto err ;
				if (!ss_resolve_read(&res,src,gaistr(&tokeep,i))) goto err ;
				
				if (res.type == CLASSIC)
				{
					found = 1 ;
					int logname = get_rstrlen_until(res.sa.s + res.name,SS_LOG_SUFFIX) ;
					if (logname > 0) continue ;
					info_print_text(&res, &id, 0) ;
					if (res.ndeps)
					{
						if (!bprintf(buffer_1,"%s%-*s","", STYLE->indent, STYLE->limb)) goto err ;
						info_walk(&res,src,REVERSE,&id) ;
						if (buffer_putsflush(buffer_1,"") < 0) goto err ; 
					}
				}
			}
		}
		if (!ss_resolve_check(src,SS_MASTER+1)){ VERBO3 strerr_warnwu1sys("read inner bundle -- please make a bug report") ; }
		if (!ss_resolve_read(&res,src,SS_MASTER + 1)) goto err ;
		if (!res.ndeps && !found)
		{
			goto empty ;
		}
		else if (res.ndeps) info_walk(&res,src,REVERSE,&d) ;		
	}
	else
	{
		if (!ss_resolve_check(src,name)) goto err ;
		if (!ss_resolve_read(&res,src,name)) goto err ;
		info_walk(&res,src,REVERSE,&d) ;
	}
	
	ss_resolve_free(&res) ;
	genalloc_deepfree(stralist,&tokeep,stra_free) ;
	
	return e ;
	
	empty:
		e = -1 ;
	err:
		ss_resolve_free(&res) ;
		genalloc_deepfree(stralist,&tokeep,stra_free) ;
		return e ;
}

int tree_args(int argc, char const *const *argv)
{
	int r ;
	size_t newlen ;
	genalloc gatree = GENALLOC_ZERO ;// stralist, all tree
	stralloc tree = STRALLOC_ZERO ;
	stralloc sacurrent = STRALLOC_ZERO ;
	stralloc currname = STRALLOC_ZERO ;
	stralloc src = STRALLOC_ZERO ;

	int todisplay = 0 ;
	
	{
		subgetopt_t l = SUBGETOPT_ZERO ;
			
		for (;;)
		{
			int opt = getopt_args(argc,argv, ">hv:rd:l:", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			
			switch (opt)
			{
				case 'h' : 	tree_help(); return 1 ;
				case 'v' :  if (!uint0_scan(l.arg, &VERBOSITY)) exit_tree_usage() ; break ;
				case 'r' : 	REVERSE = 1 ; break ;
				case 'd' : 	if (!uint0_scan(l.arg, &MAXDEPTH)) exit_tree_usage(); break ;
				case 'l' : 	if (!stralloc_cats(&live,l.arg)) retstralloc(0,"sv_args") ;
							if (!stralloc_0(&live)) retstralloc(0,"sv_args") ;
							break ;
				default : exit_tree_usage() ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	if (argc > 1) exit_tree_usage();
	
	if (argv[0]) todisplay = 1 ;
	
	r = set_livedir(&live) ;
	if (!r) retstralloc(0,"sv_args") ;
	if (r < 0 ) strerr_dief3x(111,"live: ",live.s," must be an absolute path") ;
	
	if (!stralloc_cats(&live,SS_STATE + 1)) goto err ;
	if (!stralloc_cats(&live,"/")) goto err ;
	if (!stralloc_cats(&live,OWNERSTR)) goto err ;
	if (!stralloc_cats(&live,"/")) goto err ;
	newlen = live.len ;
	
	r = tree_find_current(&sacurrent,base.s,OWNER) ;
	if (r)
	{
		if (!tree_setname(&currname,sacurrent.s)) strerr_diefu1x(111,"set the tree name") ;
	}
	
	if (!stralloc_copy(&src,&base)) goto err ;
	if (!stralloc_cats(&src,"/" SS_SYSTEM)) goto err ;
	if (!stralloc_0(&src)) goto err ;
	
	if (todisplay)
		if (!dir_search(src.s,argv[0],S_IFDIR)) strerr_dief2x(110,"unknown tree: ",argv[0]) ;
	
	if (!dir_get(&gatree, src.s,SS_BACKUP + 1, S_IFDIR)) goto err ;
	if (genalloc_len(stralist,&gatree))
	{
		for (unsigned int i = 0 ; i < genalloc_len(stralist,&gatree) ; i++)
		{
			tree.len = 0 ;
				
			char *treename = gaistr(&gatree,i) ;
			if (todisplay)
				if (!obstr_equal(treename,argv[0])) continue ;
			
			int current = 0 ;
			int init = 0 ;
			live.len = newlen ;
			if (!stralloc_cats(&live,treename)) goto err ;
			if (!stralloc_0(&live)) goto err ;
			if (dir_search(live.s,"init",S_IFREG)) init = 1 ;
		
			int enabled = tree_cmd_state(VERBOSITY,"-s",treename) ; 
			if (currname.len)
				current = obstr_equal(treename,currname.s) ;
			
			info_print_tree(treename,init,current,enabled) ;
					
			if(!stralloc_cats(&tree,treename)) retstralloc(0,"tree_args") ;
			if(!stralloc_0(&tree)) retstralloc(0,"tree_args") ;
	
			r = tree_sethome(&tree,base.s,OWNER) ;
			if (!r) strerr_diefu2sys(111,"find tree: ", tree.s) ;
			
			r = graph_display(tree.s,"",0) ;
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
	
	stralloc_free(&live) ;
	stralloc_free(&tree) ;
	stralloc_free(&src) ;
	stralloc_free(&sacurrent) ;
	stralloc_free(&currname) ;
	genalloc_deepfree(stralist,&gatree,stra_free) ;
	
	return 1 ;
	
	err:
		stralloc_free(&live) ;
		stralloc_free(&tree) ;
		stralloc_free(&src) ;
		stralloc_free(&sacurrent) ;
		stralloc_free(&currname) ;
		genalloc_deepfree(stralist,&gatree,stra_free) ;
		return 0 ;
}

int sv_args(int argc, char const *const *argv,char const *const *envp)
{
	int r, found ; 
	unsigned int nlog = 0 ;
	
	stralloc treename = STRALLOC_ZERO ;
	stralloc tree = STRALLOC_ZERO ;
	genalloc gawhat = GENALLOC_ZERO ;//stralist
	genalloc gatree = GENALLOC_ZERO ;
	stralloc src = STRALLOC_ZERO ;
	stralloc env = STRALLOC_ZERO ;
	stralloc conf = STRALLOC_ZERO ;
	
	ss_resolve_t res = RESOLVE_ZERO ;
	
	char const *svname = 0 ;
	char const *tname = 0 ;
	
	r = found = 0 ;
	
	{
		subgetopt_t l = SUBGETOPT_ZERO ;
		
		for (;;)
		{
			int opt = getopt_args(argc,argv, ">hv:l:p:rd:t:", &l) ;
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
				case 't' :	tname = l.arg ; break ;
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
	
	size_t newlen ;
	
	if (!stralloc_copy(&src,&live)) goto err ;
	if (!stralloc_cats(&src,SS_STATE + 1)) goto err ;
	if (!stralloc_cats(&src,"/")) goto err ;
	if (!stralloc_cats(&src,OWNERSTR)) goto err ;
	if (!stralloc_cats(&src,"/")) goto err ;
	newlen = src.len ;
	
	if (!tname)
	{	
		
		if (!stralloc_0(&src)) goto err ;
		
		if (!dir_get(&gatree, src.s,"", S_IFDIR)) 
			strerr_diefu2x(111,"get tree from directory: ",src.s) ;
		
		if (genalloc_len(stralist,&gatree))
		{
			for (unsigned int i = 0 ; i < genalloc_len(stralist,&gatree) ; i++)
			{
				src.len = newlen ;
				char *name = gaistr(&gatree,i) ;
				if (!stralloc_cats(&src,name)) goto err ;
				if (!stralloc_cats(&src,SS_RESOLVE)) goto err ;
				if (!stralloc_0(&src)) goto err ;
				if (dir_search(src.s,svname,S_IFREG))
				{
					if(!stralloc_cats(&treename,name)) retstralloc(0,"sv_args") ;
					if(!stralloc_0(&treename)) retstralloc(0,"sv_args") ;
					found++ ;
				}
			}
		}
		else 
		{
			if (!bprintf(buffer_1," %s ","nothing to display")) goto err ;
			if (buffer_putflush(buffer_1,"\n",1) < 0) goto err ;
			goto end ;
		}
	}
	else
	{
		if(!stralloc_cats(&treename,tname)) retstralloc(0,"sv_args") ;
		if(!stralloc_0(&treename)) retstralloc(0,"sv_args") ;
		if (!stralloc_cats(&src,treename.s)) goto err ;
		if (!stralloc_cats(&src,SS_RESOLVE)) goto err ;
		if (!stralloc_0(&src)) goto err ;
		if (dir_search(src.s,svname,S_IFREG)) found++;
	}

	if (!found)
	{
		strerr_warnw2x("unknown service: ",svname) ;
		goto err ;
	}
	else if (found > 1)
	{
		strerr_warnw2x(svname," is set on divers tree -- please use -t options") ;
		goto err ;
	}
	if (!stralloc_cats(&tree,treename.s)) goto err ;
	r = tree_sethome(&tree,base.s,OWNER) ;
	if (r<=0) strerr_diefu2sys(111,"find tree: ", tree.s) ;
	
	info_print_title(svname) ;
	if (!bprintf(buffer_1,"%s%s\n","on tree : ",treename.s)) goto err ;
	
	src.len = newlen ;
	if (!stralloc_cats(&src,treename.s)) goto err ;
	if (!stralloc_0(&src)) goto err ;
	if (!ss_resolve_read(&res,src.s,svname)) strerr_diefu2sys(111,"read resolve file of: ",svname) ;
	
	/** status */
	if (buffer_putsflush(buffer_1,"status : ") < 0) goto err ;
	if (!print_status(&res,treename.s,envp)) goto err ;

	/** print the type */	
	if (!bprintf(buffer_1,"%s %s\n","type :",get_keybyid(res.type))) goto err ;
	
	if (!bprintf(buffer_1,"%s %s\n","description :",res.sa.s + res.description)) goto err ;
	if (!bprintf(buffer_1,"%s%s\n","source : ",res.sa.s + res.src)) goto err ;
	if (!bprintf(buffer_1,"%s%s\n","run at : ",res.sa.s + res.runat)) goto err ;
	/** dependencies */
	if (res.ndeps) 
	{	
		if (res.type == BUNDLE) info_print_title("contents") ;
		else info_print_title("dependencies") ;
	
		if (!graph_display(tree.s,svname,1)) strerr_diefu2x(111,"display graph of tree: ", treename.s) ;
		
	}
	/** scripts*/
	if (res.exec_run||res.exec_finish)
	{
		info_print_title("scripts") ;
		if (res.exec_run)
		{
			if (!bprintf(buffer_1,"%s%s\n","start script :",res.sa.s + res.exec_run)) goto err ;
		}
		if (res.exec_finish)
		{
			if (!bprintf(buffer_1,"%s%s\n","stop script :",res.sa.s + res.exec_finish)) goto err ;
		}
	}
	/** environment */
	if(!OWNER)
	{
		if (!stralloc_cats(&env,SS_SERVICE_SYSCONFDIR)) goto err ;
		if (!stralloc_0(&env)) goto err ;
	}
	else
	{
		if (!stralloc_cats(&env,base.s)) goto err ;
		if (!stralloc_cats(&env,SS_ENVDIR)) goto err ;
		if (!stralloc_0(&env)) goto err ;
	}
	
	if (dir_search(env.s,svname,S_IFREG))
	{
		if (!file_readputsa(&conf,env.s,svname)) goto err ;
		info_print_title("environment") ;
		if (!bprintf(buffer_1,"%s",conf.s)) goto err ;
	}
	
	
	/** logger */
	if (res.type == CLASSIC || res.type == LONGRUN) 
	{
		if (res.logger)
		{
			info_print_title("logger") ;
			if (!bprintf(buffer_1,"%s%s\n","logger associated : ",res.sa.s + res.logger)) goto err ;
			if (!bprintf(buffer_1,"%s ","log destination :")) goto err ;
			if (!bprintf(buffer_1,"%s \n",res.sa.s + res.dstlog)) goto err ;
			if (nlog)
			{
				stralloc log = STRALLOC_ZERO ;
				/** the file current may not exist if the service was never started*/
				if (!dir_search(res.sa.s + res.dstlog,"current",S_IFREG)){	if (!bprintf(buffer_1,"%s \n","unable to find the log file")) goto err ; }
				else
				{
					if (!file_readputsa(&log,res.sa.s + res.dstlog,"current")) retstralloc(0,"sv_args") ;
					if (!log.len) 
					{
						if (!bprintf(buffer_1,"\n")) goto err ;
						if (!bprintf(buffer_1,"log file is empty \n")) goto err ;
					}
					else
					{
						if (!bprintf(buffer_1,"\n")) goto err ;
						if (!bprintf(buffer_1,"%s \n",print_nlog(log.s,nlog))) goto err ;
					}
				}
				stralloc_free(&log) ;
			}
		}
	}
	if (buffer_putsflush(buffer_1,"") < 0) goto err ;
	
	end:
	stralloc_free(&tree) ;
	stralloc_free(&treename) ;
	genalloc_deepfree(stralist,&gawhat,stra_free) ;
	genalloc_deepfree(stralist,&gatree,stra_free) ;
	ss_resolve_free(&res) ;
	stralloc_free(&src) ;
	stralloc_free(&env) ;
	stralloc_free(&conf) ;
	return 1 ;
	
	err:
		stralloc_free(&tree) ;
		stralloc_free(&treename) ;
		genalloc_deepfree(stralist,&gawhat,stra_free) ;
		genalloc_deepfree(stralist,&gatree,stra_free) ;
		ss_resolve_free(&res) ;
		stralloc_free(&src) ;
		stralloc_free(&env) ;
		stralloc_free(&conf) ;
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
	
	OWNER = MYUID ;
	size_t ownerlen = uid_fmt(OWNERSTR,OWNER) ;
	OWNERSTR[ownerlen] = 0 ;
	color = &use_color ;
		
	setlocale(LC_ALL, "");
	
	if(!str_diff(nl_langinfo(CODESET), "UTF-8")) {
		STYLE = &graph_utf8;
	}
		
	if (!set_ownersysdir(&base,OWNER)) strerr_diefu1sys(111, "set owner directory") ;
	
	if (!what)
		if (!tree_args(--argc,++argv)) strerr_diefu1x(111,"display trees informations") ;
	
	if (what)
		if (!sv_args(--argc,++argv,envp)) strerr_diefu1x(111,"display services informations") ;
	
	stralloc_free(&base) ;
	stralloc_free(&live) ;
		
	return 0 ;
}
