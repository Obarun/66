/* 
 * 66-intree.c
 * 
 * Copyright (c) 2018-2020 Eric Vidal <eric@obarun.org>
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
#include <stdio.h>

#include <oblibs/sastr.h>
#include <oblibs/log.h>
#include <oblibs/obgetopt.h>
#include <oblibs/types.h>
#include <oblibs/string.h>
#include <oblibs/files.h>

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
#include <66/backup.h>

static unsigned int REVERSE = 0 ;
static unsigned int NOFIELD = 1 ;
static unsigned int GRAPH = 0 ;
static uid_t OWNER ;
static char OWNERSTR[UID_FMT] ;

static stralloc base = STRALLOC_ZERO ;
static stralloc live = STRALLOC_ZERO ;
static stralloc src = STRALLOC_ZERO ;

static wchar_t const field_suffix[] = L" :" ;
static char fields[INFO_NKEY][INFO_FIELD_MAXLEN] = {{ 0 }} ;
static void info_display_name(char const *field,char const *treename) ;
static void info_display_init(char const *field,char const *treename) ;
static void info_display_order(char const *field,char const *treename) ;
static void info_display_enabled(char const *field,char const *treename) ;
static void info_display_current(char const *field,char const *treename) ;
static void info_display_allow(char const *field,char const *treename) ;
static void info_display_symlink(char const *field,char const *treename) ;
static void info_display_contents(char const *field,char const *treename) ;
ss_resolve_graph_style *STYLE = &graph_default ;

info_opts_map_t const opts_tree_table[] =
{
	{ .str = "name", .func = &info_display_name, .id = 0 },
	{ .str = "init", .func = &info_display_init, .id = 1 },
	{ .str = "enabled", .func = &info_display_enabled, .id = 2 },
	{ .str = "start", .func = &info_display_order, .id = 3 },
	{ .str = "current", .func = &info_display_current, .id = 4 },
	{ .str = "allowed", .func = &info_display_allow, .id = 5 },
	{ .str = "symlinks", .func = &info_display_symlink, .id = 6 },
	{ .str = "contents", .func = &info_display_contents, .id = 7},
	{ .str = 0, .func = 0, .id = -1 }
} ;

#define MAXOPTS 9
#define checkopts(n) if (n >= MAXOPTS) log_die(100, "too many options")
#define DELIM ','

#define USAGE "66-intree [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -n ] [ -o name,init,enabled,... ] [ -g ] [ -d depth ] [ -r ] tree"

static inline void info_help (void)
{
  static char const *help =
"66-intree <options> tree \n"
"\n"
"options :\n"
"	-h: print this help\n"
"	-z: use color\n"
"	-v: increase/decrease verbosity\n"
"	-l: live directory\n"
"	-n: do not display the names of fields\n"
"	-o: comma separated list of field to display\n"
"	-g: displays the contents field as graph\n"
"	-d: limit the depth of the contents field recursion\n"
"	-r: reverse the contents field\n" 
"\n"
"valid field for -o options are:\n"
"\n"
"	name: displays the name of the tree\n"
"	init: displays a boolean value of the initialization state\n"
"	enabled: displays a boolean value of the enable state\n"
"	start: displays the list of tree(s) started before\n"
"	current: displays a boolean value of the current state\n"
"	allowed: displays a list of allowed user to use the tree\n"
"	symlinks: displays the target of tree's symlinks\n"
"	contents: displays the contents of the tree\n"
"\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
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
	if (NOFIELD) info_display_field_name(field) ;
	if (!bprintf(buffer_1,"%s",treename)) 
		log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
	if (buffer_putsflush(buffer_1,"\n") == -1) 
		log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
}

static void info_display_init(char const *field,char const *treename)
{
	unsigned int init = 0 ;
	int r = set_livedir(&live) ;
	if (!r) log_die_nomem("stralloc") ;
	if (r == -1) log_die(LOG_EXIT_SYS,"live: ",live.s," must be an absolute path") ;
	
	if (!stralloc_cats(&live,SS_STATE + 1) ||
	!stralloc_cats(&live,"/") ||
	!stralloc_cats(&live,OWNERSTR) ||
	!stralloc_cats(&live,"/") ||
	!stralloc_cats(&live,treename) ||
	!stralloc_cats(&live,"/init") ||
	!stralloc_0(&live)) log_die_nomem("stralloc") ;
	if (!access(live.s, F_OK)) init = 1 ;

	if (NOFIELD) info_display_field_name(field) ;
	if (!bprintf(buffer_1,"%s%s%s",init ? log_color->valid : log_color->warning, init ? "yes":"no",log_color->off)) 
		log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
	
	if (buffer_putsflush(buffer_1,"\n") == -1)
		log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

}

static void info_display_current(char const *field,char const *treename)
{
	stralloc sacurr = STRALLOC_ZERO ;
	int current = 0 ;
	
	if (tree_find_current(&sacurr,base.s,OWNER))
	{
		char name[sacurr.len + 1] ;//be paranoid +1
		if (!ob_basename(name,sacurr.s)) log_dieu(LOG_EXIT_SYS,"basename of: ",sacurr.s) ;
		current = obstr_equal(treename,name) ;
	}
	if (NOFIELD) info_display_field_name(field) ;
	if (!bprintf(buffer_1,"%s%s%s", current ? log_color->blink : log_color->warning, current ? "yes":"no",log_color->off))
		log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
	
	if (buffer_putsflush(buffer_1,"\n") == -1) 
		log_dieusys(LOG_EXIT_SYS,"write to stdout") ;	
	
	stralloc_free(&sacurr) ;
}

static void info_display_enabled(char const *field,char const *treename)
{
	int enabled = tree_cmd_state(VERBOSITY,"-s",treename) ;
	if (NOFIELD) info_display_field_name(field) ;
	if (!bprintf(buffer_1,"%s%s%s",enabled == 1 ? log_color->valid : log_color->warning, enabled == 1 ? "yes":"no",log_color->off)) 
		log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
	
	if (buffer_putsflush(buffer_1,"\n") == -1) 
		log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
}

static void info_display_order(char const *field,char const *treename)
{
	int r ;
	size_t pos = 0 ;
	stralloc contents = STRALLOC_ZERO ;
	stralloc tmp = STRALLOC_ZERO ;
	if (NOFIELD) info_display_field_name(field) ;
	int enabled = tree_cmd_state(VERBOSITY,"-s",treename) ;
	
	r = file_readputsa(&tmp,src.s,SS_STATE + 1) ;
	if(!r) log_dieusys(LOG_EXIT_SYS,"open: ", src.s,SS_STATE) ;
	
	if (tmp.len && enabled == 1)
	{
		if (!sastr_rebuild_in_oneline(&tmp) ||
		!sastr_clean_element(&tmp))
			log_dieusys(LOG_EXIT_SYS,"rebuilt state list") ;
		for (pos = 0 ;pos < tmp.len; pos += strlen(tmp.s + pos) + 1)
		{
			char *name = tmp.s + pos ;
			if (obstr_equal(name,treename)) break ;
			if (!sastr_add_string(&contents,name))
				log_dieusys(LOG_EXIT_SYS,"rebuilt state list") ;
		}
	}
	if (contents.len) {
		 info_display_list(field,&contents) ;
	}
	else
	{
		if (!bprintf(buffer_1,"%s%s%s",log_color->warning,"None",log_color->off)) 
			log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
	
		if (buffer_putsflush(buffer_1,"\n") == -1) 
			log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
	}
	stralloc_free(&contents) ;
	stralloc_free(&tmp) ;
}

static void info_get_graph_src(ss_resolve_graph_t *graph,char const *src,unsigned int reverse)
{
	size_t srclen = strlen(src), pos ;

	stralloc sa = STRALLOC_ZERO ;
	genalloc gares = GENALLOC_ZERO ;

	char solve[srclen + SS_RESOLVE_LEN + 1] ;
	memcpy(solve,src,srclen) ;
	memcpy(solve + srclen, SS_RESOLVE,SS_RESOLVE_LEN) ;
	solve[srclen + SS_RESOLVE_LEN] = 0 ;

	if (!sastr_dir_get(&sa,solve,SS_MASTER+1,S_IFREG))
		log_dieusys(LOG_EXIT_SYS,"get source service file at: ",solve) ;

	if (!ss_resolve_sort_bytype(&gares,&sa,src))
		log_dieu(LOG_EXIT_SYS,"sort list by type") ;

	for (pos = 0 ; pos < genalloc_len(ss_resolve_t,&gares) ; pos++)
		if (!ss_resolve_graph_build(graph,&genalloc_s(ss_resolve_t,&gares)[pos],src,reverse))
				log_dieu(LOG_EXIT_SYS,"build the graph from: ",src) ;

	genalloc_deepfree(ss_resolve_t,&gares,ss_resolve_free) ;
	stralloc_free(&sa) ;
}

static void info_display_allow(char const *field, char const *treename)
{
	if (NOFIELD) info_display_field_name(field) ;
	size_t treenamelen = strlen(treename) , pos ;
	stralloc sa = STRALLOC_ZERO ;
	stralloc salist = STRALLOC_ZERO ;
	char tmp[src.len + treenamelen + SS_RULES_LEN + 1 ] ;
	// force to close the string
	auto_strings(tmp,src.s) ;
	auto_string_from(tmp,src.len,treename,SS_RULES) ;

	if (!sastr_dir_get(&sa,tmp,"",S_IFREG))
		log_dieusys(LOG_EXIT_SYS,"get permissions of tree at: ",tmp) ;
	
	for (pos = 0 ;pos < sa.len; pos += strlen(sa.s + pos) + 1)
	{
		char *suid = sa.s + pos ;
		uid_t uid = 0 ;
		if (!uid0_scan(suid, &uid))
			log_dieusys(LOG_EXIT_SYS,"get uid of: ",suid) ;
		if (pos) 
			if (!stralloc_cats(&salist," ")) log_die_nomem("stralloc") ;
		if (!get_namebyuid(uid,&salist))
			log_dieusys(LOG_EXIT_SYS,"get name of uid: ",suid) ;
	}
	if (!stralloc_0(&salist)) log_die_nomem("stralloc") ;
	if (!sastr_rebuild_in_oneline(&salist)) log_dieu(LOG_EXIT_SYS,"rebuild list: ",salist.s) ;
	if (!stralloc_0(&salist)) log_die_nomem("stralloc") ;
	info_display_list(field,&salist) ;
	
	stralloc_free(&sa) ;
	stralloc_free(&salist) ;
}

static void info_display_symlink(char const *field, char const *treename)
{
	if (NOFIELD) info_display_field_name(field) ;
	ssexec_t info = SSEXEC_ZERO ;
	if (!auto_stra(&info.treename,treename)) log_die_nomem("stralloc") ;
	if (!auto_stra(&info.base,base.s)) log_die_nomem("stralloc") ;
	int db , svc ;
	size_t typelen ;
	char type[UINT_FMT] ;
	typelen = uint_fmt(type, TYPE_BUNDLE) ;
	type[typelen] = 0 ;
	
	char cmd[typelen + 6 + 1] ;
	
	auto_strings(cmd,"-t",type," -b") ;
	db = backup_cmd_switcher(VERBOSITY,cmd,&info) ;
	if (db < 0) log_dieu(LOG_EXIT_SYS,"find realpath of symlink for db of tree: ",info.treename.s) ;
	
	typelen = uint_fmt(type, TYPE_CLASSIC) ;
	type[typelen] = 0 ;
		
	auto_strings(cmd,"-t",type," -b") ;
	svc = backup_cmd_switcher(VERBOSITY,cmd,&info) ;
	if (svc < 0) log_dieu(LOG_EXIT_SYS,"find realpath of symlink for svc of tree: ",info.treename.s) ;

	if (!bprintf(buffer_1,"%s%s%s%s%s%s%s%s", "svc->",!svc ? log_color->valid : log_color->warning , !svc ? "source" : "backup",log_color->off, " db->", !db ? log_color->valid : log_color->warning, !db ? "source" : "backup", log_color->off)) 
		log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
	
	if (buffer_putsflush(buffer_1,"\n") == -1) 
		log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
	
	ssexec_free(&info) ;	
}

static void info_display_contents(char const *field, char const *treename)
{
	int r ;
	size_t padding = 1, treenamelen = strlen(treename) ;
	ss_resolve_t res = RESOLVE_ZERO ;
	ss_resolve_graph_t graph = RESOLVE_GRAPH_ZERO ;
	stralloc salist = STRALLOC_ZERO ;
	
	char tmp[src.len + treenamelen + SS_SVDIRS_LEN + 1] ;
	memcpy(tmp,src.s,src.len) ;
	memcpy(tmp + src.len,treename,treenamelen) ;
	memcpy(tmp + src.len + treenamelen,SS_SVDIRS,SS_SVDIRS_LEN) ;
	tmp[src.len + treenamelen + SS_SVDIRS_LEN] = 0 ;
	
	info_get_graph_src(&graph,tmp,0) ;
	
	if (NOFIELD) padding = info_display_field_name(field) ;
	else { field = 0 ; padding = 0 ; }
	
	if (!genalloc_len(ss_resolve_t,&graph.name)) goto empty ;
	
	r = ss_resolve_graph_publish(&graph,0) ;
	if (r < 0) log_die(LOG_EXIT_USER,"cyclic graph detected at tree: ", treename) ;
	else if (!r) log_dieusys(LOG_EXIT_SYS,"publish service graph of tree: ",treename) ;
	
	for (size_t i = 0 ; i < genalloc_len(ss_resolve_t,&graph.sorted) ; i++)
	{
		char *string = genalloc_s(ss_resolve_t,&graph.sorted)[i].sa.s ;
		char *name = string + genalloc_s(ss_resolve_t,&graph.sorted)[i].name ;
		if (!stralloc_catb(&salist,name,strlen(name)+1)) log_die_nomem("stralloc") ;
	}
		
	if (GRAPH)
	{
		if (!bprintf(buffer_1,"%s\n","/")) 
			log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
		size_t el = sastr_len(&salist) ;
		if (!sastr_rebuild_in_oneline(&salist)) log_dieu(LOG_EXIT_SYS,"rebuild dependencies list") ;
		ss_resolve_init(&res) ;
		res.ndeps = el ;
		res.deps = ss_resolve_add_string(&res,salist.s) ;
		if (!info_graph_init(&res,tmp,REVERSE, padding, STYLE))
			log_die(LOG_EXIT_SYS,"display graph of: ",treename) ;
		goto freed ;
	}
	else
	{
		if (REVERSE) 
			if (!sastr_reverse(&salist))
				log_dieusys(LOG_EXIT_SYS,"reverse dependencies list") ;
		info_display_list(field,&salist) ;
		goto freed ;
	}
	empty:
		if (GRAPH)
		{
			if (!bprintf(buffer_1,"%s\n","/")) 
				log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
			if (!bprintf(buffer_1,"%*s%s%s",padding,"",STYLE->last,"None")) 
				log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
		}
		else
		{
			if (!bprintf(buffer_1,"%s","None")) 
				log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
		}
		if (buffer_putsflush(buffer_1,"\n") == -1)
			log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
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
	
	if (!sastr_clean_string_wdelim(&sa,str,DELIM)) log_dieu(LOG_EXIT_SYS,"parse options") ;
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
		if (old == nopts) log_die(LOG_EXIT_SYS,"invalid option: ",o) ;
	}
	
	stralloc_free(&sa) ;
}

int main(int argc, char const *const *argv, char const *const *envp)
{
	unsigned int legacy = 1 ;
	
	size_t pos, newlen, livelen ;
	int what[MAXOPTS] = { 0 } ;
	
	log_color = &log_color_disable ;
	
	char const *treename = 0 ;
	
	for (int i = 0 ; i < MAXOPTS ; i++)
		what[i] = -1 ;
		
	
	char buf[MAXOPTS][INFO_FIELD_MAXLEN] = {
		"Name",
		"Initialized",
		"Enabled",
		"Starts after",
		"Current",
		"Allowed",
		"Symlinks",
		"Contents" } ;
	
	
	stralloc satree = STRALLOC_ZERO ;
	
	PROG = "66-intree" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">hzv:no:grd:l:c", &l) ;
			if (opt == -1) break ;
			if (opt == -2) log_die(LOG_EXIT_USER,"options must be set first") ;
			switch (opt)
			{
				case 'h' : 	info_help(); return 0 ;
				case 'v' :  if (!uint0_scan(l.arg, &VERBOSITY)) log_usage(USAGE) ; break ;
				case 'z' :	log_color = !isatty(1) ? &log_color_disable : &log_color_enable ; break ;
				case 'n' :	NOFIELD = 0 ; break ;
				case 'o' : 	legacy = 0 ; info_parse_options(l.arg,what) ; break ;
				case 'g' :	GRAPH = 1 ; break ;
				case 'r' : 	REVERSE = 1 ; break ;
				case 'd' : 	if (!uint0_scan(l.arg, &MAXDEPTH)) log_usage(USAGE) ; break ;
				case 'l' : 	if (!stralloc_cats(&live,l.arg)) log_usage(USAGE) ;
							if (!stralloc_0(&live)) log_usage(USAGE) ;
							break ;
				case 'c' :	log_die(LOG_EXIT_SYS,"deprecated option -- please use -z instead") ;
				default : 	log_usage(USAGE) ; 
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
	
	if (!set_ownersysdir(&base,OWNER)) log_dieusys(LOG_EXIT_SYS, "set owner directory") ;
	if (!stralloc_copy(&src,&base) ||
	!stralloc_cats(&src,SS_SYSTEM) ||
	!stralloc_0(&src)) log_die_nomem("stralloc") ;
	src.len-- ;
	
	if (!scan_mode(src.s,S_IFDIR))
	{
		log_info("no tree exist yet") ;
		goto freed ;
	}
	
	if (!stralloc_cats(&src,"/")) log_die_nomem("stralloc") ;
	
	newlen = src.len ;
	livelen = live.len ;

	if (treename)
	{
		
		if (!auto_stra(&src,treename)) log_die_nomem("stralloc") ;
		if (!scan_mode(src.s,S_IFDIR)) log_dieusys(LOG_EXIT_SYS,"find tree: ", src.s) ;
		src.len = newlen ;
		if (!stralloc_0(&src)) log_die_nomem("stralloc") ;
		src.len-- ;
		info_display_all(treename,what) ;		
	}
	else
	{
		if (!stralloc_0(&src)) log_die_nomem("stralloc") ;
	    if (!sastr_dir_get(&satree, src.s,SS_BACKUP + 1, S_IFDIR)) log_dieusys(LOG_EXIT_SYS,"get list of tree at: ",src.s) ;
		if (satree.len)
		{
			if (!info_cmpnsort(&satree)) log_dieu(LOG_EXIT_SYS,"sort list of tree") ;
			for(pos = 0 ; pos < satree.len ; pos += strlen(satree.s + pos) +1 )
			{
				src.len = newlen ;
				live.len = livelen ;
				char *name = satree.s + pos ;
				info_display_all(name,what) ;
				if (buffer_puts(buffer_1,"\n") == -1) 
					log_dieusys(LOG_EXIT_SYS,"write to stdout") ;	
			}
		}
		else 
		{
			log_info("no tree exist yet") ;
			goto freed ;
		}
	}

	if (buffer_flush(buffer_1) == -1) 
		log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
	
		
	freed:
	stralloc_free(&base) ;
	stralloc_free(&live) ;
	stralloc_free(&src) ;
	stralloc_free(&satree) ;
		
	return 0 ;
}
