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
#include <stdio.h>

#include <oblibs/obgetopt.h>
#include <oblibs/error2.h>
#include <oblibs/stralist.h>
#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/directory.h>
#include <oblibs/types.h>

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
#include <66/resolve.h>
#include <66/environ.h>

#include <s6/s6-supervise.h>//s6_svc_ok



unsigned int VERBOSITY = 1 ;
static stralloc base = STRALLOC_ZERO ;
static stralloc live = STRALLOC_ZERO ;
static uid_t OWNER ;
static char OWNERSTR[UID_FMT] ;
static int force_color = 0 ;

#define MAXSIZE 4096
#define DBG_INFO(...) bprintf(buffer_1, __VA_ARGS__) ;\
				buffer_putflush(buffer_1,"\n",1) ;


#define USAGE "66-info [ -h ] [ -T ] [ -S ] sub-options (use -h as sub-options for futher informations)"

#define TREE_USAGE "66-info -T [ -c | -C ] [ -h ] [ -v verbosity ] [ -r ] [ -d depth ] tree "
#define exit_tree_usage() exitusage(TREE_USAGE)
#define SV_USAGE "66-info -S [ -c | -C ] [ -h ] [ -v verbosity ] [-t tree ] [ -l live ] [ -p n lines ] [ -r ] [ -d depth ] service"
#define exit_sv_usage() exitusage(SV_USAGE)

unsigned int REVERSE = 0 ;

ss_resolve_graph_style *STYLE = &graph_default ;
unsigned int MAXDEPTH = 1 ;

typedef struct depth_s depth_t ;
struct depth_s
{
	depth_t *prev ;
	depth_t *next ;
	int level ;
} ;

struct set_color {
	const char *bold_white ;
	const char *back_blue ;
	const char *white_underline ; 
	const char *yellow ;
	const char *blue ;
	const char *blink_blue ;
	const char *red ;
	const char *off ;
} ;

static struct set_color use_color = {
	"\033[1;37m", // bold_white
	"\033[44m", // background blue 
	"\033[3;4;1;117m", // white underline 
	"\033[1;38;5;226m", // yellow 
	"\033[1;38;5;117m", // blue 
	"\033[1;5;38;5;117m", // blink blue
	"\033[1;31m", // red
	"\033[0m" //off
} ;

static struct set_color no_color = {
	"",
	"",
	"",
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
"	-c: disable colorization\n" 
"	-C: force colorization\n"
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
"	-c: disable colorization\n" 
"	-C: force colorization\n"
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

int info_print_title(char const *name)
{	
	size_t half = 17 ;
	size_t tlen = strlen(name) ;
	size_t htlen = tlen/2 ;
	size_t paddingl ;
	size_t paddingr ;
	if (tlen > half) paddingr = 0 ;
	paddingl = half + htlen ;
	paddingr = paddingl - htlen < half ? half : (half-htlen) ;
	
	if (!bprintf(buffer_1,"%s%*s%*s",color->white_underline,paddingl,name,paddingr,color->off)) return 0 ;
	if (buffer_putsflush(buffer_1,"\n") < 0) return 0 ; 
	return 1 ;
}

int info_print_tree(char const *treename,int init,int current,int enabled)
{
	int indent = init ? 0 : 1 ;
	if (!info_print_title(treename)) return 0 ;
	if (!bprintf(buffer_1,"%s%s%s%*s%s%s","Initialized: ",init ? color->blue : color->yellow, init ? "yes":"no",indent,"",color->off,"| ")) return 0 ;
	if (!bprintf(buffer_1,"%s%s%s%s","Current: ",current ? color->blink_blue : color->yellow ,current ? "yes":"no",color->off)) return 0 ;
	if (buffer_putsflush(buffer_1,"\n") < 0) return 0 ; 
	if (!bprintf(buffer_1,"%s%9s%s%s%s%s","Contains:"," | ","Enabled: ",enabled == 1 ? color->blue : color->yellow ,enabled ? "yes":"no",color->off)) return 0 ;
	if (buffer_putsflush(buffer_1,"\n") < 0) return  0 ;
	return 1 ;
} 

int info_print_status(ss_resolve_t *res,char const *treename, char const *const *envp)
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

static void info_print_graph(ss_resolve_t *res, depth_t *depth, int last)
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
		if (!bprintf(buffer_1,"%*s%s(%s%i%s,%s%s%s,%s) %s", STYLE->indent * (depth->level - level), "", tip, status.pid ? color->blue : color->off,status.pid ? status.pid : 0,color->off, res->disen ? color->off : color->red, res->disen ? "Enabled" : "Disabled",color->off,get_keybyid(res->type), name)) return ;
	}
	else if (!bprintf(buffer_1,"%s(%i,%s) %s",tip, status.pid ? status.pid : 0, get_keybyid(res->type),name)) return ;

	if (buffer_putsflush(buffer_1,"\n") < 0) return ; 
}

int info_cmpnsort(genalloc *ga)
{
	unsigned int len = genalloc_len(stralist,ga) ;
	if (!len) return 0 ;
	char names[len][4096] ;
	char tmp[4096] ;
	for (unsigned int i = 0 ; i < genalloc_len(stralist,ga) ; i++)
	{
		memcpy(names[i],gaistr(ga,i),gaistrlen(ga,i)) ;
	}
	genalloc_deepfree(stralist,ga,stra_free) ;
	for (unsigned int i = 0 ; i < len - 1 ; i++)
	{
		for (unsigned int j = i+1 ; j < len ; j++)
		{
			if (strcmp(names[i],names[j]) > 0)
			{
				strcpy(tmp,names[i]) ;
				strcpy(names[i],names[j]);
				strcpy(names[j],tmp);
			}
		}
	}
	
	for (unsigned int i = 0 ; i < len ; i++)
	{
		if (!stra_add(ga,names[i])) return 0 ;
	}	
	return 1 ;
}

int info_walk(ss_resolve_t *res,char const *src,int reverse, depth_t *depth)
{
	genalloc gadeps = GENALLOC_ZERO ;
	ss_resolve_t dres = RESOLVE_ZERO ;
	
	if((!res->ndeps) || (depth->level > MAXDEPTH))
		goto freed ;

	if (!clean_val(&gadeps,res->sa.s + res->deps)) goto err ;
	if (reverse) genalloc_reverse(stralist,&gadeps) ;
	for(unsigned int i = 0 ; i < res->ndeps ; i++ )
	{	
		int last =  i + 1 < res->ndeps  ? 0 : 1 ;		
		char *name = gaistr(&gadeps,i) ;
		
		if (!ss_resolve_check(src,name)) goto err ;	
		if (!ss_resolve_read(&dres,src,name)) goto err ;
		
		info_print_graph(&dres, depth, last) ;
						
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
			if (!info_walk(&dres,src,reverse,&d)) goto err;
			depth->next = NULL;
		}
	}
	freed:
	ss_resolve_free(&dres) ;
	genalloc_deepfree(stralist,&gadeps,stra_free) ;
	return 1 ;
	err: 
		ss_resolve_free(&dres) ;
		genalloc_deepfree(stralist,&gadeps,stra_free) ;
		return 0 ;
}

/** what = 0 -> complete tree
 * what = 1 -> only name */
int graph_display(char const *tree,char const *name,unsigned int what)
{
	int e ;
	ss_resolve_t res = RESOLVE_ZERO ;
	genalloc tokeep = GENALLOC_ZERO ; //stralist
	ss_resolve_graph_t graph = RESOLVE_GRAPH_ZERO ;
	stralloc inres = STRALLOC_ZERO ;
	e = 0 ;
	
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
				
				if (!ss_resolve_check(src,gaistr(&tokeep,i))) goto err ;
				if (!ss_resolve_read(&res,src,gaistr(&tokeep,i))) goto err ;
				if (!ss_resolve_graph_build(&graph,&res,src,REVERSE)) goto err ;
			}
			if (!ss_resolve_graph_publish(&graph,REVERSE)) goto err ;
			for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,&graph.sorted) ; i++)
			{
				char *string = genalloc_s(ss_resolve_t,&graph.sorted)[i].sa.s ;
				char *name = string + genalloc_s(ss_resolve_t,&graph.sorted)[i].name ;
				
				if (!stralloc_cats(&inres,name)) goto err ;
				if (!stralloc_cats(&inres," ")) goto err ;
			}
			inres.len-- ;
			if (!stralloc_0(&inres)) goto err ;
			ss_resolve_init(&res) ;
			res.ndeps = genalloc_len(ss_resolve_t,&graph.sorted) ;
			res.deps = ss_resolve_add_string(&res,inres.s) ;
			if(!info_walk(&res,src,0,&d)) goto err ;
		}
		else
		{
			goto empty ;
		}
	}
	else
	{
		if (!ss_resolve_check(src,name)) goto err ;
		if (!ss_resolve_read(&res,src,name)) goto err ;
		/*if (res.disen)*/ if(!info_walk(&res,src,REVERSE,&d)) goto err ;
		
	}
	
	ss_resolve_free(&res) ;
	genalloc_deepfree(stralist,&tokeep,stra_free) ;
	ss_resolve_graph_free(&graph) ;
	stralloc_free(&inres) ;
	return 1 ;
	
	empty:
		e = -1 ;
	err:
		ss_resolve_free(&res) ;
		genalloc_deepfree(stralist,&tokeep,stra_free) ;
		ss_resolve_graph_free(&graph) ;
		stralloc_free(&inres) ;
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
			int opt = getopt_args(argc,argv, ">hcCv:rd:l:", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			
			switch (opt)
			{
				case 'h' : 	tree_help(); return 1 ;
				case 'c' :	color = &no_color ; break ;
				case 'C' :	force_color = 1 ; break ;
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

	// -c and -C cannot be added togheter
	if (color == &no_color && force_color) exit_tree_usage() ;

	if (!isatty(1) && !force_color)
		color = &no_color ;
	
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
	if (!scan_mode(src.s,S_IFDIR)) goto empty ;
	if (todisplay)
		if (!dir_search(src.s,argv[0],S_IFDIR)) strerr_dief2x(110,"unknown tree: ",argv[0]) ;
	
	if (!dir_get(&gatree, src.s,SS_BACKUP + 1, S_IFDIR)) goto err ;
	if (genalloc_len(stralist,&gatree))
	{
		if (!info_cmpnsort(&gatree)) strerr_diefu1x(111,"sort list of tree") ;
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
			if (!stralloc_cats(&live,"/init")) goto err ;
			if (!stralloc_0(&live)) goto err ;
			if (!access(live.s, F_OK)) init = 1 ;
		
			int enabled = tree_cmd_state(VERBOSITY,"-s",treename) ; 
			if (currname.len)
				current = obstr_equal(treename,currname.s) ;
			
			if (!info_print_tree(treename,init,current,enabled)) goto err ;
					
			if(!stralloc_cats(&tree,treename)) retstralloc(0,"tree_args") ;
			if(!stralloc_0(&tree)) retstralloc(0,"tree_args") ;
	
			r = tree_sethome(&tree,base.s,OWNER) ;
			if (!r) strerr_diefu2sys(111,"find tree: ", tree.s) ;
			
			r = graph_display(tree.s,"",0) ;
			if (r < 0)
			{
				if (!bprintf(buffer_1,"%s","nothing to display")) goto err ;
				if (buffer_putflush(buffer_1,"\n",1) < 0) goto err ;
			}else if (!r) goto err ;
			if (buffer_putflush(buffer_1,"\n",1) < 0) goto err ;		
		}
	}
	else 
	{
		empty:
		strerr_warni1x("no tree exist yet") ;
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
	genalloc gatree = GENALLOC_ZERO ;
	stralloc src = STRALLOC_ZERO ;
	stralloc conf = STRALLOC_ZERO ;
	stralloc env = STRALLOC_ZERO ;
	
	ss_resolve_t res = RESOLVE_ZERO ;
	
	char const *svname = 0 ;
	char const *tname = 0 ;
	
	r = found = 0 ;
	
	{
		subgetopt_t l = SUBGETOPT_ZERO ;
		
		for (;;)
		{
			int opt = getopt_args(argc,argv, ">hcCv:l:p:rd:t:", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'h' : 	sv_help(); return 1 ;
				case 'c' :	color = &no_color ; break ;
				case 'C' :	force_color = 1 ; break ;
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

	// -c and -C cannot be added togheter
	if (color == &no_color && force_color) exit_sv_usage() ;

	if (!isatty(1) && !force_color)
		color = &no_color ;

	svname = *argv ;
	
	r = set_livedir(&live) ;
	if (!r) retstralloc(0,"sv_args") ;
	if (r < 0 ) strerr_dief3x(111,"live: ",live.s," must be an absolute path") ;
	
	size_t newlen ;
	
	if (!stralloc_copy(&src,&base)) goto err ;
	if (!stralloc_cats(&src,SS_SYSTEM)) goto err ;
	if (!stralloc_cats(&src,"/")) goto err ;
	newlen = src.len ;

	if (!tname)
	{	
		
		if (!stralloc_0(&src)) goto err ;
		
		if (!dir_get(&gatree, src.s,SS_BACKUP+1, S_IFDIR)) 
			strerr_diefu2x(111,"get tree from directory: ",src.s) ;
		
		if (genalloc_len(stralist,&gatree))
		{
			for (unsigned int i = 0 ; i < genalloc_len(stralist,&gatree) ; i++)
			{
				src.len = newlen ;
				char *name = gaistr(&gatree,i) ;
				
				if (!stralloc_cats(&src,name)) goto err ;
				if (!stralloc_cats(&src,SS_SVDIRS)) goto err ;
				if (!stralloc_0(&src)) goto err ;
				if (ss_resolve_check(src.s,svname))
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
		if (!stralloc_cats(&treename,tname)) retstralloc(0,"sv_args") ;
		if (!stralloc_0(&treename)) retstralloc(0,"sv_args") ;
		if (!stralloc_cats(&src,treename.s)) goto err ;
		if (!stralloc_cats(&src,SS_SVDIRS)) goto err ;
		if (!stralloc_0(&src)) goto err ;
		if (ss_resolve_check(src.s,svname)) found++;
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
	
	if (!info_print_title(svname)) goto err ;
	if (!bprintf(buffer_1,"%s%s\n","on tree : ",treename.s)) goto err ;
	
	src.len = newlen ;
	if (!stralloc_cats(&src,treename.s)) goto err ;
	if (!stralloc_cats(&src,SS_SVDIRS)) goto err ;
	if (!stralloc_0(&src)) goto err ;
	if (!ss_resolve_read(&res,src.s,svname)) strerr_diefu2sys(111,"read resolve file of: ",svname) ;
	
	/** status */
	if (!bprintf(buffer_1,"%s%s%s%s%s","status : ",res.disen ? color->off : color->red,res.disen ? "Enabled" : "Disabled",color->off,", ")) goto err ;
	if (buffer_putsflush(buffer_1,"") < 0) goto err ;
	if (!info_print_status(&res,treename.s,envp)) goto err ;

	/** print the type */	
	if (!bprintf(buffer_1,"%s %s\n","type :",get_keybyid(res.type))) goto err ;
	
	if (!bprintf(buffer_1,"%s %s\n","description :",res.sa.s + res.description)) goto err ;
	if (!bprintf(buffer_1,"%s%s\n","source : ",res.sa.s + res.src)) goto err ;
	if (!bprintf(buffer_1,"%s%s\n","run at : ",res.sa.s + res.runat)) goto err ;
	/** dependencies */
	if (res.ndeps) 
	{	
		if (res.type == BUNDLE){ if (!info_print_title("contents")) goto err ; }
		else if (!info_print_title("dependencies")) goto err ;
	
		if (!graph_display(tree.s,svname,1)) strerr_diefu2x(111,"display dependencies of: ", svname) ;
		
	}
	/** scripts*/
	if (res.exec_run||res.exec_finish)
	{
		if (!info_print_title("scripts")) goto err ;
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
	if (res.srconf)
	{
		if (!env_resolve_conf(&env,MYUID)) goto err ;
		if (!file_readputsa(&conf,env.s,svname)) goto err ;
		if (!info_print_title("environment")) goto err ;
		if (!bprintf(buffer_1,"%s%s\n","source : ",env.s)) goto err ;
		if (!bprintf(buffer_1,"%s",conf.s)) goto err ;
	}
	
	/** logger */
	if (res.type == CLASSIC || res.type == LONGRUN) 
	{
		if (res.logger)
		{
			if (!info_print_title("logger")) goto err ;
			if (!bprintf(buffer_1,"%s%s\n","logger associated : ",res.sa.s + res.logger)) goto err ;
			if (!bprintf(buffer_1,"%s ","log destination :")) goto err ;
			if (!bprintf(buffer_1,"%s \n",res.sa.s + res.dstlog)) goto err ;
			if (nlog)
			{
				if (!info_print_title("log file")) goto err ;
				stralloc log = STRALLOC_ZERO ;
				/** the file current may not exist if the service was never started*/
				if (!dir_search(res.sa.s + res.dstlog,"current",S_IFREG)){	if (!bprintf(buffer_1,"%s%s%s \n",color->red,"unable to find the log file",color->off)) goto err ; }
				else
				{
					if (!file_readputsa(&log,res.sa.s + res.dstlog,"current")) retstralloc(0,"sv_args") ;
					if (log.len < 10) 
					{
						if (!bprintf(buffer_1,"\n")) goto err ;
						if (!bprintf(buffer_1,"%s%s%s",color->yellow,"log file is empty \n",color->off)) goto err ;
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
	genalloc_deepfree(stralist,&gatree,stra_free) ;
	ss_resolve_free(&res) ;
	stralloc_free(&src) ;
	stralloc_free(&conf) ;
	stralloc_free(&env) ;
	return 1 ;
	
	err:
		stralloc_free(&tree) ;
		stralloc_free(&treename) ;
		genalloc_deepfree(stralist,&gatree,stra_free) ;
		ss_resolve_free(&res) ;
		stralloc_free(&src) ;
		stralloc_free(&conf) ;
		stralloc_free(&env) ;
		return 0 ;
}

int main(int argc, char const *const *argv, char const *const *envp)
{
	int what ;
	
	what = -1 ;
	color = &use_color ;
	
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
	
	argc-- ; argv++ ;
		
	OWNER = MYUID ;
	size_t ownerlen = uid_fmt(OWNERSTR,OWNER) ;
	OWNERSTR[ownerlen] = 0 ;
	
		
	setlocale(LC_ALL, "");
	
	if(!str_diff(nl_langinfo(CODESET), "UTF-8")) {
		STYLE = &graph_utf8;
	}
		
	if (!set_ownersysdir(&base,OWNER)) strerr_diefu1sys(111, "set owner directory") ;
	
	if (!what)
		if (!tree_args(argc,argv)) strerr_diefu1x(111,"display trees informations") ;
	
	if (what)
		if (!sv_args(argc,argv,envp)) strerr_diefu1x(111,"display services informations") ;
	
	stralloc_free(&base) ;
	stralloc_free(&live) ;
		
	return 0 ;
}
