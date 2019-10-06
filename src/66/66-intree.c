/* 
 * 66-intree.c
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
#include <locale.h>
#include <langinfo.h>
#include <sys/types.h>
#include <wchar.h>
#include <unistd.h>//access

#include <oblibs/sastr.h>
#include <oblibs/error2.h>
#include <oblibs/obgetopt.h>
#include <oblibs/types.h>
#include <oblibs/string.h>

#include <skalibs/strerr2.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/lolstdio.h>
#include <skalibs/bytestr.h>
#include <skalibs/buffer.h>

#include <66/info.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/tree.h>
#include <66/enum.h>
#include <66/resolve.h>

unsigned int VERBOSITY = 1 ;
static unsigned int REVERSE = 0 ;
unsigned int MAXDEPTH = 1 ;
static unsigned int GRAPH = 0 ;
static uid_t OWNER ;
static char OWNERSTR[UID_FMT] ;

static stralloc base = STRALLOC_ZERO ;
static stralloc live = STRALLOC_ZERO ;
static stralloc src = STRALLOC_ZERO ;

static wchar_t const field_suffix[] = L" :" ;
static char fields[ENDOFKEY][INFO_FIELD_MAXLEN] = {{ 0 }} ;
static void info_display_name(char const *field,char const *treename) ;
static void info_display_init(char const *field,char const *treename) ;
static void info_display_enabled(char const *field,char const *treename) ;
static void info_display_current(char const *field,char const *treename) ;
static void info_display_contains(char const *field,char const *treename) ;
ss_resolve_graph_style *STYLE = &graph_default ;

info_opts_map_t const opts_tree_table[] =
{
	{ .str = "name", .func = &info_display_name, .id = 0 },
	{ .str = "init", .func = &info_display_init, .id = 1 },
	{ .str = "enabled", .func = &info_display_enabled, .id = 2 },
	{ .str = "current", .func = &info_display_current, .id = 3 },
	{ .str = "contains", .func = &info_display_contains, .id = 4 },
	{ .str = 0, .func = 0, .id = -1 }
} ;

#define MAXOPTS 6
#define checkopts(n) if (n >= MAXOPTS) strerr_dief1x(100, "too many options")
#define DELIM ','

#define USAGE "66-intree [ -h ] [ -v verbosity ] [ -l live ] [ -c ] [ -o name,init,enabled,... ] [ -g ] [ -d depth ] [ -r ] tree"

static inline void info_help (void)
{
  static char const *help =
"66-intree <options> tree \n"
"\n"
"options :\n"
"	-h: print this help\n"
"	-v: increase/decrease verbosity\n"
"	-l: live directory\n"
"	-c: use color\n"
"	-o: comma separated list of field to display\n"
"	-g: displays the contains field as graph\n"
"	-d: limit the depth of the contains field recursion\n"
"	-r: reserve the contains field\n" 
"\n"
"valid field for -o options are:\n"
"\n"
"	name: displays the name of the tree\n"
"	init: displays a boolean value of the initialization state\n"
"	enabled: displays a boolean value of the enable state\n"
"	current: displays a boolean value of the current state\n"
"	contains: displays the contain of the tree\n"
"\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
}

static int info_cmpnsort(stralloc *sa)
{
	size_t pos = 0 ;
	if (!sa->len) return 0 ;
	size_t salen = sa->len ;
	size_t nel = sastr_len(sa), idx = 0, a = 0, b = 0 ;
	char names[nel][4096] ;
	char tmp[4096] ;
	
	for (; pos < salen && idx < nel ; pos += strlen(sa->s + pos) + 1,idx++)
	{
		memcpy(names[idx],sa->s + pos,strlen(sa->s + pos)) ;
		names[idx][strlen(sa->s+pos)] = 0 ;
	}
	for (; a < nel - 1 ; a++)
	{
		for (b = a + 1 ; b < idx ; b++)
		{
			if (strcmp(names[a],names[b]) > 0)
			{
				strcpy(tmp,names[a]) ;
				strcpy(names[a],names[b]);
				strcpy(names[b],tmp);
			}
		}
	}
	sa->len = 0 ;
	for (a = 0 ; a < nel ; a++)
	{
		if (!sastr_add_string(sa,names[a])) return 0 ;
	}	
	return 1 ;
}

static void info_display_name(char const *field, char const *treename)
{
	info_display_field_name(field) ;
	if (!bprintf(buffer_1," %s",treename)) 
		strerr_diefu1sys(111,"write to stdout") ;
	if (buffer_putsflush(buffer_1,"\n") == -1) 
		strerr_diefu1sys(111,"write to stdout") ;
}

static void info_display_init(char const *field,char const *treename)
{
	unsigned int init = 0 ;
	int r = set_livedir(&live) ;
	if (!r) exitstralloc("display_init") ;
	if (r == -1) strerr_dief3x(111,"live: ",live.s," must be an absolute path") ;
	
	if (!stralloc_cats(&live,SS_STATE + 1) ||
	!stralloc_cats(&live,"/") ||
	!stralloc_cats(&live,OWNERSTR) ||
	!stralloc_cats(&live,"/") ||
	!stralloc_cats(&live,treename) ||
	!stralloc_cats(&live,"/init") ||
	!stralloc_0(&live)) exitstralloc("display_init") ;
	if (!access(live.s, F_OK)) init = 1 ;

	info_display_field_name(field) ;
	if (!bprintf(buffer_1," %s%s%s",init ? info_color->valid : info_color->warning, init ? "yes":"no",info_color->off)) 
		strerr_diefu1sys(111,"write to stdout") ;
	
	if (buffer_putsflush(buffer_1,"\n") == -1) 
		strerr_diefu1sys(111,"write to stdout") ;

}

static void info_display_current(char const *field,char const *treename)
{
	stralloc sacurr = STRALLOC_ZERO ;
	int current = 0 ;
	
	if (tree_find_current(&sacurr,base.s,OWNER))
	{
		char name[sacurr.len] ;
		if (!basename(name,sacurr.s)) strerr_diefu2x(111,"basename of: ",sacurr.s) ;
		current = obstr_equal(treename,name) ;
	}
	info_display_field_name(field) ;
	if (!bprintf(buffer_1," %s%s%s", current ? info_color->blink : info_color->warning, current ? "yes":"no",info_color->off))
		strerr_diefu1sys(111,"write to stdout") ;
	
	if (buffer_putsflush(buffer_1,"\n") == -1) 
		strerr_diefu1sys(111,"write to stdout") ;	
	
	stralloc_free(&sacurr) ;
}

static void info_display_enabled(char const *field,char const *treename)
{
	int enabled = tree_cmd_state(VERBOSITY,"-s",treename) ;
	info_display_field_name(field) ;
	if (!bprintf(buffer_1," %s%s%s",enabled == 1 ? info_color->valid : info_color->warning, enabled == 1 ? "yes":"no",info_color->off)) 
		strerr_diefu1sys(111,"write to stdout") ;
	
	if (buffer_putsflush(buffer_1,"\n") == -1) 
		strerr_diefu1sys(111,"write to stdout") ;
}

static void info_get_graph_src(ss_resolve_graph_t *graph,char const *src,unsigned int reverse)
{
	
	size_t srclen = strlen(src), pos ;
	stralloc sa = STRALLOC_ZERO ;
	ss_resolve_t res = RESOLVE_ZERO ;
	char solve[srclen + SS_RESOLVE_LEN + 1] ;
	memcpy(solve,src,srclen) ;
	memcpy(solve + srclen, SS_RESOLVE,SS_RESOLVE_LEN) ;
	solve[srclen + SS_RESOLVE_LEN] = 0 ;
	
	if (!sastr_dir_get(&sa,solve,"",S_IFREG))
		strerr_diefu2sys(111,"get source service file at: ",solve) ;
	
	for (pos = 0 ;pos < sa.len; pos += strlen(sa.s + pos) + 1)
	{
		char *name = sa.s + pos ;
		if (!ss_resolve_read(&res,src,name))
			strerr_diefu2x(111,"read resolve file of: ",name) ;
		if (!ss_resolve_graph_build(graph,&res,src,reverse)) 
			strerr_diefu2x(111,"build the graph from: ",src) ; 
	}
	
	stralloc_free(&sa) ;
	ss_resolve_free(&res) ;
}

static void info_display_contains(char const *field, char const *treename)
{
	int r ;
	size_t padding = 1 ;
	ss_resolve_t res = RESOLVE_ZERO ;
	ss_resolve_graph_t graph = RESOLVE_GRAPH_ZERO ;
	stralloc salist = STRALLOC_ZERO ;

	if (!stralloc_cats(&src,treename) ||
	!stralloc_cats(&src,SS_SVDIRS) ||
	!stralloc_0(&src)) exitstralloc("display_contains") ;
	
	info_get_graph_src(&graph,src.s,0) ;
	
	padding = info_display_field_name(field) ;
	
	if (!genalloc_len(ss_resolve_t,&graph.name)) goto empty ;
	
	r = ss_resolve_graph_publish(&graph,0) ;
	if (r < 0) strerr_dief2x(110,"cyclic graph detected at tree: ", treename) ;
	else if (!r) strerr_diefu2sys(111,"publish service graph of tree: ",treename) ;
	
	for (size_t i = 0 ; i < genalloc_len(ss_resolve_t,&graph.sorted) ; i++)
	{
		char *string = genalloc_s(ss_resolve_t,&graph.sorted)[i].sa.s ;
		char *name = string + genalloc_s(ss_resolve_t,&graph.sorted)[i].name ;
		if (!stralloc_catb(&salist,name,strlen(name)+1)) exitstralloc("display_contains") ;
	}
		
	if (GRAPH)
	{
		if (!bprintf(buffer_1," %s\n","/")) 
			strerr_diefu1sys(111,"write to stdout") ;
		size_t el = sastr_len(&salist) ;
		if (!sastr_rebuild_in_oneline(&salist)) strerr_diefu1x(111,"rebuild dependencies list") ;
		ss_resolve_init(&res) ;
		res.ndeps = el ;
		res.deps = ss_resolve_add_string(&res,salist.s) ;
		if (!info_graph_init(&res,src.s,REVERSE, padding, STYLE))
			strerr_dief2x(111,"display graph of: ",treename) ;
		goto freed ;
	}
	else
	{
		if (!bprintf(buffer_1,"%s"," ")) 
			strerr_diefu1sys(111,"write to stdout") ;
		if (REVERSE) 
			if (!sastr_reverse(&salist))
				strerr_diefu1x(111,"reverse dependencies list") ;
		info_display_list(field,&salist) ;
		goto freed ;
	}
	empty:
		if (GRAPH)
		{
			if (!bprintf(buffer_1," %s\n","/")) 
				strerr_diefu1sys(111,"write to stdout") ;
			if (!bprintf(buffer_1,"%*s%s%s",padding,"",STYLE->last," empty tree")) 
				strerr_diefu1sys(111,"write to stdout") ;
		}
		else
		{
			if (!bprintf(buffer_1,"%s"," empty tree")) 
				strerr_diefu1sys(111,"write to stdout") ;
		}
		if (buffer_putsflush(buffer_1,"\n") == -1)
			strerr_diefu1sys(111,"write to stdout") ;
	freed:
		ss_resolve_free(&res) ;
		ss_resolve_graph_free(&graph) ;
		stralloc_free(&salist) ;
}

static void info_display_all(char const *treename,int *what)
{
	
	unsigned int i = 0 ;
	for (; what[i] >= 0 ; i++)
	{
		unsigned int idx = what[i] ;
		(*opts_tree_table[idx].func)(fields[opts_tree_table[idx].id],treename) ;
	}
	
}

static void info_parse_options(char const *str,int *what)
{
	size_t pos = 0 ;
	stralloc sa = STRALLOC_ZERO ;
	
	if (!sastr_clean_string_wdelim(&sa,str,DELIM)) strerr_diefu1x(111,"parse options") ;
	unsigned int n = sastr_len(&sa), nopts = 0 , old ;
	checkopts(n) ;
	info_opts_map_t const *t ;
	
	for (;pos < sa.len; pos += strlen(sa.s + pos) + 1)
	{
		char *o = sa.s + pos ;
		t = opts_tree_table ;
		old = nopts ;
		for (; t->str; t++)
		{			
			if (obstr_equal(o,t->str))
			{
				/*if (!t->id){ what[0] = t->id ; old++ ; }
				else*/ what[nopts++] = t->id ;
			}
		}
		if (old == nopts) strerr_dief2x(111,"invalid option: ",o) ;
	}
	
	stralloc_free(&sa) ;
}

int main(int argc, char const *const *argv, char const *const *envp)
{
	unsigned int legacy = 1 ;
	
	size_t pos, newlen, livelen ;
	int what[MAXOPTS] = { 0 } ;
	
	info_color = &no_color ;
	
	char const *treename = 0 ;
	
	for (int i = 0 ; i < MAXOPTS ; i++)
		what[i] = -1 ;
		
	
	char buf[MAXOPTS][INFO_FIELD_MAXLEN] = {
		"Name",
		"Initialized",
		"Enabled",
		"Current",
		"Contains" } ;
	
	
	stralloc satree = STRALLOC_ZERO ;
	
	PROG = "66-intree" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">hv:co:grd:l:", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'h' : 	info_help(); return 0 ;
				case 'v' :  if (!uint0_scan(l.arg, &VERBOSITY)) exitusage(USAGE) ; break ;
				case 'c' :	info_color = !isatty(1) ? &no_color : &use_color ; break ;
				case 'o' : 	legacy = 0 ; info_parse_options(l.arg,what) ; break ;
				case 'g' :	GRAPH = 1 ; break ;
				case 'r' : 	REVERSE = 1 ; break ;
				case 'd' : 	if (!uint0_scan(l.arg, &MAXDEPTH)) exitusage(USAGE) ; break ;
				case 'l' : 	if (!stralloc_cats(&live,l.arg)) exitusage(USAGE) ;
							if (!stralloc_0(&live)) exitusage(USAGE) ;
							break ;
				default : exitusage(USAGE) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argv[0]) treename = argv[0] ;
	
	if (legacy)
	{
		unsigned int i = 0 ;
		for (; i < MAXOPTS - 1 ; i++)
			what[i] = i ;
			
		what[i] = -1 ;
	}
	
	OWNER = getuid() ;
	size_t ownerlen = uid_fmt(OWNERSTR,OWNER) ;
	OWNERSTR[ownerlen] = 0 ;
	
	info_field_align(buf,fields,field_suffix,MAXOPTS) ;
		
	setlocale(LC_ALL, "");
	
	if(!str_diff(nl_langinfo(CODESET), "UTF-8")) {
		STYLE = &graph_utf8;
	}
	
	if (!set_ownersysdir(&base,OWNER)) strerr_diefu1sys(111, "set owner directory") ;
	if (!stralloc_copy(&src,&base) ||
	!stralloc_cats(&src,SS_SYSTEM) ||
	!stralloc_0(&src))  exitstralloc("main") ;
	src.len-- ;
	
	if (!scan_mode(src.s,S_IFDIR))
	{
		strerr_warni1x("no tree exist yet") ;
		goto freed ;
	}
	
	if (!stralloc_cats(&src,"/")) exitstralloc("main") ;
	
	newlen = src.len ;
	livelen = live.len ;

	if (treename)
	{
		
		if (!stralloc_cats(&src,treename) ||
		!stralloc_0(&src)) exitstralloc("main") ;
		if (!scan_mode(src.s,S_IFDIR)) strerr_diefu2sys(111,"find tree: ", src.s) ;
		src.len = newlen ;
		info_display_all(treename,what) ;		
	}
	else
	{
		
	    if (!sastr_dir_get(&satree, src.s,SS_BACKUP + 1, S_IFDIR)) strerr_diefu2sys(111,"get list of tree at: ",src.s) ;
		if (satree.len)
		{
			if (!info_cmpnsort(&satree)) strerr_diefu1x(111,"sort list of tree") ;
			for(pos = 0 ; pos < satree.len ; pos += strlen(satree.s + pos) +1 )
			{
				src.len = newlen ;
				live.len = livelen ;
				char *name = satree.s + pos ;
				info_display_all(name,what) ;
				if (buffer_puts(buffer_1,"\n") == -1) 
					strerr_diefu1sys(111,"write to stdout") ;	
			}
		}
		else 
		{
			strerr_warni1x("no tree exist yet") ;
			goto freed ;
		}
	}

	if (buffer_putsflush(buffer_1,"\n") == -1) 
		strerr_diefu1sys(111, "write to stdout") ;
	
		
	freed:
	stralloc_free(&base) ;
	stralloc_free(&live) ;
	stralloc_free(&src) ;
	stralloc_free(&satree) ;
		
	return 0 ;
}
