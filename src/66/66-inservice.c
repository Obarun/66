/* 
 * 66-inservice.c
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
#include <unistd.h>
//#include <stdio.h>

#include <oblibs/sastr.h>
#include <oblibs/error2.h>
#include <oblibs/obgetopt.h>
#include <oblibs/types.h>
#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/directory.h>

#include <skalibs/strerr2.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/lolstdio.h>
#include <skalibs/bytestr.h>
#include <skalibs/buffer.h>
#include <skalibs/djbunix.h>

#include <66/info.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/tree.h>
#include <66/enum.h>
#include <66/resolve.h>
#include <66/environ.h>

#include <s6/s6-supervise.h>

unsigned int VERBOSITY = 1 ;
static unsigned int REVERSE = 0 ;
unsigned int MAXDEPTH = 1 ;
static unsigned int GRAPH = 0 ;
static char const *const *ENVP ;
static unsigned int nlog = 20 ;
static stralloc src = STRALLOC_ZERO ;

static wchar_t const field_suffix[] = L" :" ;
static char fields[ENDOFKEY][INFO_FIELD_MAXLEN] = {{ 0 }} ;
static void info_display_string(char const *str) ;
static void info_display_name(char const *field, ss_resolve_t *res) ;
static void info_display_intree(char const *field, ss_resolve_t *res) ;
static void info_display_status(char const *field, ss_resolve_t *res) ;
static void info_display_type(char const *field, ss_resolve_t *res) ;
static void info_display_description(char const *field, ss_resolve_t *res) ;
static void info_display_source(char const *field, ss_resolve_t *res) ;
static void info_display_live(char const *field, ss_resolve_t *res) ;
static void info_display_deps(char const *field, ss_resolve_t *res) ;
static void info_display_start(char const *field, ss_resolve_t *res) ;
static void info_display_stop(char const *field, ss_resolve_t *res) ;
static void info_display_envat(char const *field, ss_resolve_t *res) ;
static void info_display_envfile(char const *field, ss_resolve_t *res) ;
static void info_display_logname(char const *field, ss_resolve_t *res) ;
static void info_display_logdst(char const *field, ss_resolve_t *res) ;
static void info_display_logfile(char const *field, ss_resolve_t *res) ;

ss_resolve_graph_style *STYLE = &graph_default ;

info_opts_map_t const opts_sv_table[] =
{
	{ .str = "name", .svfunc = &info_display_name, .id = 0 },
	{ .str = "intree", .svfunc = &info_display_intree, .id = 1 },
	{ .str = "status", .svfunc = &info_display_status, .id = 2 },
	{ .str = "type", .svfunc = &info_display_type, .id = 3 },
	{ .str = "description", .svfunc = &info_display_description, .id = 4 },
	{ .str = "source", .svfunc = &info_display_source, .id = 5 },
	{ .str = "live", .svfunc = &info_display_live, .id = 6 },
	{ .str = "depends", .svfunc = &info_display_deps, .id = 7 },
	{ .str = "start", .svfunc = &info_display_start, .id = 8 },
	{ .str = "stop", .svfunc = &info_display_stop, .id = 9 },
	{ .str = "envat", .svfunc = &info_display_envat, .id = 10 },
	{ .str = "envfile", .svfunc = &info_display_envfile, .id = 11 },
	{ .str = "logname", .svfunc = &info_display_logname, .id = 12 },
	{ .str = "logdst", .svfunc = &info_display_logdst, .id = 13 },
	{ .str = "logfile", .svfunc = &info_display_logfile, .id = 14 },
	{ .str = 0, .svfunc = 0, .id = -1 }
} ;

#define MAXOPTS 16
#define checkopts(n) if (n >= MAXOPTS) strerr_dief1x(100, "too many options")
#define DELIM ','

#define USAGE "66-inservice [ -h ] [ -v verbosity ] [ -c ] [ -o name,instree,status,... ] [ -g ] [ -d depth ] [ -r ] [ -t tree ] [ -p nline ] service"

static inline void info_help (void)
{
  static char const *help =
"66-inservice <options> service \n"
"\n"
"options :\n"
"	-h: print this help\n"
"	-v: increase/decrease verbosity\n"
"	-c: use color\n"
"	-o: comma separated list of field to display\n"
"	-g: displays the contents field as graph\n"
"	-d: limit the depth of the contents field recursion\n"
"	-r: reverse the contents field\n"
"	-t: only search service at the specified tree\n"
"	-p: print n last lines of the log file\n"
"\n"
"valid field for -o options are:\n"
"\n"
"	name: displays the name\n"
"	intree: displays the service's tree name\n"
"	status: displays the status\n"
"	type: displays the service type\n"
"	description: displays the description\n"
"	source: displays the source of the service's frontend file\n"
"	live: displays the service's live directory\n"
"	depends: displays the service's dependencies\n"
"	start: displays the service's start script\n"
"	stop: displays the service's stop script\n"
"	envat: displays the source of the environment file\n"
"	envfile: displays the contents of the environment file\n"
"	logname: displays the logger's name\n"
"	logdst: displays the logger's destination\n"
"	logfile: displays the contents of the log file\n"
"\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
}

char *print_nlog(char *str, int n) 
{ 
	int r = 0 ;
	int delim ='\n' ;
	size_t slen = strlen(str) ;
	
	if (n <= 0) return NULL; 

	size_t ndelim = 0;  
	char *target_pos = NULL;   
	
	r = get_rlen_until(str,delim,slen) ;
	
	target_pos = str + r ; 

	if (target_pos == NULL) return NULL; 

	while (ndelim < n) 
	{ 
		while (str < target_pos && *target_pos != delim) 
			--target_pos; 

		if (*target_pos ==  delim) 
			--target_pos, ++ndelim; 
		else break; 
	} 

	if (str < target_pos) 
		target_pos += 2; 

	return target_pos ;
} 

static void info_display_string(char const *str)
{
	if (!bprintf(buffer_1," %s",str))
		strerr_diefu1sys(111,"write to stdout") ;
	
	if (buffer_putsflush(buffer_1,"\n") == -1) 
		strerr_diefu1sys(111,"write to stdout") ;
}

static void info_display_name(char const *field, ss_resolve_t *res)
{
	info_display_field_name(field) ;
	info_display_string(res->sa.s + res->name) ;
}

static void info_display_intree(char const *field,ss_resolve_t *res)
{
	info_display_field_name(field) ;
	info_display_string(res->sa.s + res->treename) ;
}

static void info_get_status(ss_resolve_t *res)
{
	int r ;
	int wstat ;
	pid_t pid ;
	
		
	if (res->type == CLASSIC || res->type == LONGRUN)
	{
		r = s6_svc_ok(res->sa.s + res->runat) ;
		if (r != 1)
		{
			if (!bprintf(buffer_1,"%s%s%s",info_color->warning,"not running\n",info_color->off))
				strerr_diefu1sys(111,"write to stdout") ;
			return ;
		}
		char const *newargv[3] ;
		unsigned int m = 0 ;
		
		newargv[m++] = SS_BINPREFIX "s6-svstat" ;
		newargv[m++] = res->sa.s + res->runat ;
		newargv[m++] = 0 ;
					
		pid = child_spawn0(newargv[0],newargv,ENVP) ;
		if (waitpid_nointr(pid,&wstat, 0) < 0)
			strerr_diefu2sys(111,"wait for ",newargv[0]) ;
		
		if (wstat)
			strerr_diefu2x(111,"status for service: ",res->sa.s + res->name) ;
	}
	else
	{
		if (!bprintf(buffer_1,"%s%s%s\n",info_color->warning,"nothing to display",info_color->off))
				strerr_diefu1sys(111,"write to stdout") ;
	}
}

static void info_display_status(char const *field,ss_resolve_t *res)
{
	
	info_display_field_name(field) ;
		
	if (!bprintf(buffer_1," %s%s%s%s",res->disen ? info_color->valid : info_color->error,res->disen ? "enabled" : "disabled",info_color->off,", "))
		strerr_diefu1sys(111,"write to stdout") ;
	
	if (buffer_putsflush(buffer_1,"") == -1) 
		strerr_diefu1sys(111,"write to stdout") ;
	
	info_get_status(res) ;

}

static void info_display_type(char const *field,ss_resolve_t *res)
{
	info_display_field_name(field) ;
	info_display_string(get_keybyid(res->type)) ;
}

static void info_display_description(char const *field,ss_resolve_t *res)
{
	info_display_field_name(field) ;
	info_display_string(res->sa.s + res->description) ;
}

static void info_display_source(char const *field,ss_resolve_t *res)
{
	info_display_field_name(field) ;
	info_display_string(res->sa.s + res->src) ;
}

static void info_display_live(char const *field,ss_resolve_t *res)
{
	info_display_field_name(field) ;
	info_display_string(res->sa.s + res->runat) ;
}

static void info_display_deps(char const *field, ss_resolve_t *res)
{
	size_t padding = 1 ;
	
	stralloc salist = STRALLOC_ZERO ;
	
	padding = info_display_field_name(field) ;
	
	if (!res->ndeps) goto empty ;
	
	if (GRAPH)
	{
		if (!bprintf(buffer_1," %s\n","/")) 
			strerr_diefu1sys(111,"write to stdout") ;
	
		if (!info_graph_init(res,src.s,REVERSE, padding, STYLE))
			strerr_dief2x(111,"display graph of: ",res->sa.s + res->name) ;
		goto freed ;
	}
	else
	{
		if (!bprintf(buffer_1,"%s"," ")) 
			strerr_diefu1sys(111,"write to stdout") ;
		if (!sastr_clean_string(&salist,res->sa.s + res->deps)) 
			strerr_diefu1x(111,"build dependencies list") ;
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
			if (!bprintf(buffer_1,"%*s%s%s%s%s\n",padding, "", STYLE->last, info_color->warning," no dependencies",info_color->off))
				strerr_diefu1sys(111,"write to stdout") ;
		}
		else
		{
			if (!bprintf(buffer_1,"%s%s%s\n",info_color->warning," no dependencies",info_color->off))
				strerr_diefu1sys(111,"write to stdout") ;
		}
		
	freed:
		stralloc_free(&salist) ;
}

static void info_display_start(char const *field,ss_resolve_t *res)
{
	info_display_field_name(field) ;
	if (res->exec_run)
	{
		info_display_nline(field,res->sa.s + res->exec_run) ;
	}
	else
	{
		if (!bprintf(buffer_1,"%s%s%s\n",info_color->warning," nothing to display",info_color->off))
			strerr_diefu1sys(111,"write to stdout") ;
	}
}

static void info_display_stop(char const *field,ss_resolve_t *res)
{	
	info_display_field_name(field) ;
	if (res->exec_finish)
	{
		info_display_nline(field,res->sa.s + res->exec_finish) ;
	}
	else
	{
		if (!bprintf(buffer_1,"%s%s%s\n",info_color->warning," nothing to display",info_color->off))
			strerr_diefu1sys(111,"write to stdout") ;
	}
	
}

static void info_display_envat(char const *field,ss_resolve_t *res)
{
	info_display_field_name(field) ;
	char *name = res->sa.s + res->name ;
	if (!res->srconf) goto empty ;
	{
		char *src = res->sa.s + res->srconf ;
		size_t srclen = strlen(src) ;
		size_t namelen = strlen(name) ;
		char tmp[namelen + srclen + 1] ;
		memcpy(tmp,src,srclen) ;
		tmp[srclen] = '/' ;
		memcpy(tmp + srclen,name,namelen) ;
		tmp[srclen + namelen] = 0 ;
		info_display_string(tmp) ;
		return ;
	}
	empty:
	
	if (!bprintf(buffer_1,"%s%s%s\n",info_color->warning," environment was not set",info_color->off))
		strerr_diefu1sys(111,"write to stdout") ;
}

static void info_display_envfile(char const *field,ss_resolve_t *res)
{
	info_display_field_name(field) ;
	if (!res->srconf) goto empty ;
	{
		stralloc sa = STRALLOC_ZERO ;
		char *name = res->sa.s + res->name ;
		char *src = res->sa.s + res->srconf ;
		if (!file_readputsa(&sa,src,name)) strerr_diefu1sys(111,"read environment file") ;
		info_display_nline(field,sa.s) ;
		stralloc_free(&sa) ;
		return ;
	}
	empty:
	if (!bprintf(buffer_1,"%s%s%s\n",info_color->warning," environment was not set",info_color->off))
		strerr_diefu1sys(111,"write to stdout") ;
}

static void info_display_logname(char const *field,ss_resolve_t *res)
{
	info_display_field_name(field) ;
	if (res->type == CLASSIC || res->type == LONGRUN) 
	{
		if (res->logger)
		{
			info_display_string(res->sa.s + res->logger) ;
		}
		else goto empty ;
	}
	else goto empty ;
	
	return ;
	empty:
		if (!bprintf(buffer_1,"%s%s%s\n",info_color->warning," logger doesn't exist",info_color->off))
			strerr_diefu1sys(111,"write to stdout") ;
}

static void info_display_logdst(char const *field,ss_resolve_t *res)
{
	info_display_field_name(field) ;
	if (res->type == CLASSIC || res->type == LONGRUN) 
	{
		if (res->logger)
		{
			info_display_string(res->sa.s + res->dstlog) ;
		}
		else goto empty ;
	}
	else goto empty ;
	
	return ;
	empty:
		if (!bprintf(buffer_1,"%s%s%s\n",info_color->warning," logger doesn't exist",info_color->off))
			strerr_diefu1sys(111,"write to stdout") ;
}

static void info_display_logfile(char const *field,ss_resolve_t *res)
{
	info_display_field_name(field) ;
	if (res->type == CLASSIC || res->type == LONGRUN) 
	{
		if (res->logger)
		{
			if (nlog)
			{
				stralloc log = STRALLOC_ZERO ;
				/** the file current may not exist if the service was never started*/
				size_t dstlen = strlen(res->sa.s + res->dstlog) ;
				char scan[dstlen + 9] ;
				memcpy(scan,res->sa.s + res->dstlog,dstlen) ;
				memcpy(scan + dstlen,"/current",8) ;
				scan[dstlen + 8] = 0 ;
				
				int r = scan_mode(scan,S_IFREG) ;
				if (r < 0) { errno = EEXIST ; strerr_dief2sys(111,"conflicting format of: ",scan) ; }
				if (!r)
				{ 
					if (!bprintf(buffer_1,"%s%s%s\n",info_color->error," unable to find the log file",info_color->off)) 
					goto err ; 
				}
				else
				{
					if (!file_readputsa(&log,res->sa.s + res->dstlog,"current")) strerr_diefu2sys(111,"read log file of: ",res->sa.s + res->name) ;
					if (log.len < 10) 
					{
						if (!bprintf(buffer_1,"%s%s%s",info_color->warning," log file is empty \n",info_color->off)) goto err ;
					}
					else
					{
						if (!bprintf(buffer_1,"\n")) goto err ;
						if (!bprintf(buffer_1,"%s\n",print_nlog(log.s,nlog))) goto err ;
					}
				}
				stralloc_free(&log) ;
			}
		}else goto empty ;
	}
	else goto empty ;
	
	return ;
	empty:
		if (!bprintf(buffer_1,"%s%s%s\n",info_color->warning," logger doesn't exist",info_color->off))
			strerr_diefu1sys(111,"write to stdout") ;
		return ;
	err:
		strerr_diefu1sys(111,"write to stdout") ;
}

static void info_display_all(ss_resolve_t *res,int *what)
{
	
	unsigned int i = 0 ;
	for (; what[i] >= 0 ; i++)
	{
		unsigned int idx = what[i] ;
		(*opts_sv_table[idx].svfunc)(fields[opts_sv_table[idx].id],res) ;
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
		t = opts_sv_table ;
		old = nopts ;
		for (; t->str; t++)
		{			
			if (obstr_equal(o,t->str))
				what[nopts++] = t->id ;
		}
		if (old == nopts) strerr_dief2x(111,"invalid option: ",o) ;
	}
	
	stralloc_free(&sa) ;
}

int main(int argc, char const *const *argv, char const *const *envp)
{
	unsigned int legacy = 1 ;
	int found = 0 ;
	size_t pos, newlen ;
	int what[MAXOPTS] = { 0 } ;
	
	uid_t owner ;
	char ownerstr[UID_FMT] ;
	
	ss_resolve_t res = RESOLVE_ZERO ;
	stralloc satree = STRALLOC_ZERO ;
	
	info_color = &no_color ;
	
	char const *svname = 0 ;
	char const *tname = 0 ;
	
	ENVP = envp ;
	
	for (int i = 0 ; i < MAXOPTS ; i++)
		what[i] = -1 ;
		
	
	char buf[MAXOPTS][INFO_FIELD_MAXLEN] = {
		"Name",
		"In tree",
		"Status",
		"Type",
		"Description",
		"Source",
		"Live",
		"Depends on",
		"Start script",
		"Stop script",
		"Environment source",
		"Environment file",
		"Log name",
		"Log destination",
		"Log file" } ;
	
	PROG = "66-inservice" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">hv:co:grd:t:p:", &l) ;
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
				case 't' : 	tname = l.arg ; break ;
				case 'p' : 	if (!uint0_scan(l.arg, &nlog)) exitusage(USAGE) ; break ;
				default : exitusage(USAGE) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (!argc) exitusage(USAGE) ;
	svname = *argv ;
	
	if (legacy)
	{
		unsigned int i = 0 ;
		for (; i < MAXOPTS - 1 ; i++)
			what[i] = i ;
			
		what[i] = -1 ;
	}
	
	owner = getuid() ;
	size_t ownerlen = uid_fmt(ownerstr,owner) ;
	ownerstr[ownerlen] = 0 ;
	
	info_field_align(buf,fields,field_suffix,MAXOPTS) ;
		
	setlocale(LC_ALL, "");
	
	if(!str_diff(nl_langinfo(CODESET), "UTF-8")) {
		STYLE = &graph_utf8;
	}
	
	if (!set_ownersysdir(&src,owner)) strerr_diefu1sys(111, "set owner directory") ;
	if (!stralloc_cats(&src,SS_SYSTEM) ||
	!stralloc_0(&src))  exitstralloc("main") ;
	src.len-- ;
	
	if (!scan_mode(src.s,S_IFDIR))
	{
		strerr_warni1x("no tree exist yet") ;
		goto freed ;
	}
	
	if (!stralloc_cats(&src,"/")) exitstralloc("main") ;
	newlen = src.len ;
	
	if (!tname)
	{	
		stralloc tmp = STRALLOC_ZERO ;
		if (!stralloc_0(&src) ||
		!stralloc_copy(&tmp,&src)) exitstralloc("main") ;
		
		if (!sastr_dir_get(&satree, src.s,SS_BACKUP+1, S_IFDIR)) 
			strerr_diefu2x(111,"get tree from directory: ",src.s) ;
		
		if (satree.len)
		{
			for(pos = 0 ; pos < satree.len ; pos += strlen(satree.s + pos) + 1)
			{
				tmp.len = newlen ;
				char *name = satree.s + pos ;
				
				if (!stralloc_cats(&tmp,name) ||
				!stralloc_cats(&tmp,SS_SVDIRS) ||
				!stralloc_0(&tmp)) exitstralloc("main");
				if (ss_resolve_check(tmp.s,svname))
				{
					if (!found)
						if (!stralloc_copy(&src,&tmp)) exitstralloc("main") ;
					found++ ;
				}
			}
		}
		else 
		{
			if (!bprintf(buffer_1,"%s%s%s\n",info_color->warning," nothing to display",info_color->off))
				strerr_diefu1sys(111,"write to stdout") ;
			if (buffer_putsflush(buffer_1,"\n") < 0)
				strerr_diefu1sys(111,"write to stdout") ;
		}
		stralloc_free(&tmp) ;
	}
	else
	{
		if (!stralloc_cats(&src,tname) ||
		!stralloc_cats(&src,SS_SVDIRS) ||
		!stralloc_0(&src)) exitstralloc("main") ;
		if (ss_resolve_check(src.s,svname)) found++;
	}

	if (!found)
	{
		strerr_dief2x(111,"unknown service: ",svname) ;
	
	}
	else if (found > 1)
	{
		strerr_dief2x(111,svname," is set on different tree -- please use -t options") ;
	}
	
	if (!ss_resolve_read(&res,src.s,svname)) 
		strerr_diefu2sys(111,"read resolve file of: ",svname) ;
	
	info_display_all(&res,what) ;
	
	if (buffer_putsflush(buffer_1,"\n") == -1) 
		strerr_diefu1sys(111, "write to stdout") ;
	
		
	freed:
	ss_resolve_free(&res) ;
	stralloc_free(&src) ;
	stralloc_free(&satree) ;
		
	return 0 ;

}
