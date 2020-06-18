/* 
 * 66-state.c
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
#include <wchar.h>

#include <oblibs/log.h>
#include <oblibs/sastr.h>
#include <oblibs/string.h>
#include <oblibs/obgetopt.h>
#include <oblibs/types.h>

#include <skalibs/buffer.h>
#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/lolstdio.h>

#include <66/resolve.h>
#include <66/info.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/state.h>

#define MAXOPTS 7

static wchar_t const field_suffix[] = L" :" ;
static char fields[INFO_NKEY][INFO_FIELD_MAXLEN] = {{ 0 }} ;

#define USAGE "66-instate [ -h ] [ -z ] [ -t tree ] [ -l ] service"

static inline void info_help (void)
{
  static char const *help =
"66-instate <options> service \n"
"\n"
"options :\n"
"	-h: print this help\n"
"	-z: use color\n"
"	-t: only search service at the specified tree\n"
"	-l: prints information of the associated logger if exist\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
}

static void info_display_string(char const *field,char const *str)
{
	info_display_field_name(field) ;

	if (!*str)
	{
		if (!bprintf(buffer_1,"%s%s",log_color->warning,"None"))
			log_dieusys(LOG_EXIT_SYS,"write to stdout") ;		
	}
	else
	{
		if (!buffer_puts(buffer_1,str))
			log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
	}
	if (buffer_putsflush(buffer_1,"\n") == -1) 
		log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
}

static void info_display_int(char const *field,unsigned int id)
{
	char *str = 0 ;
	char ival[UINT_FMT] ;
	ival[uint_fmt(ival,id)] = 0 ;
	str = ival ;

	info_display_string(field,str) ;		
}

int main(int argc, char const *const *argv) 
{
	int found = 0 ;
	uint8_t logger = 0 ;
	ss_resolve_t res = RESOLVE_ZERO ;
	stralloc satree = STRALLOC_ZERO ;
	stralloc src = STRALLOC_ZERO ;
	stralloc tmp = STRALLOC_ZERO ;
	ss_state_t sta = STATE_ZERO ;
	char const *tname = 0 ;
	char const *svname = 0 ;
	char const *ste = 0 ;

	log_color = &log_color_disable ;

	char buf[MAXOPTS][INFO_FIELD_MAXLEN] = {
		"Reload",
		"Init",
		"Unsupervise",
		"State",
		"Pid",
		"Name" ,
		"Real logger name" } ;

	PROG = "66-instate" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">hzlt:", &l) ;
			if (opt == -1) break ;
			if (opt == -2) log_die(LOG_EXIT_USER,"options must be set first") ;
			switch (opt)
			{
				case 'h' : info_help(); return 0 ;
				case 'z' : log_color = !isatty(1) ? &log_color_disable : &log_color_enable ; break ;
				case 't' : tname = l.arg ; break ;
				case 'l' : logger = 1 ; break ;
				default : log_usage(USAGE) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}

	if (!argc) log_usage(USAGE) ;
	svname = *argv ;

	found = ss_resolve_svtree(&src,svname,tname) ;
	if (found == -1) log_dieu(LOG_EXIT_SYS,"resolve tree source of sv: ",svname) ;
	else if (!found) {
		log_info("no tree exist yet") ;
		goto freed ;
	}
	else if (found > 2) {
		log_die(LOG_EXIT_SYS,svname," is set on different tree -- please use -t options") ;
	}
	else if (found == 1) log_die(LOG_EXIT_SYS,"unknown service: ",svname) ;

	if (!ss_resolve_read(&res,src.s,svname)) log_dieusys(111,"read resolve file of: ",src.s,"/.resolve/",svname) ;

	info_field_align(buf,fields,field_suffix,MAXOPTS) ;

	ste = res.sa.s + res.state ;

	if (!ss_state_check(ste,svname)) log_diesys(111,"unitialized service: ",svname) ;
	if (!ss_state_read(&sta,ste,svname)) log_dieusys(111,"read state file of: ",ste,"/",svname) ;

	info_display_string(fields[5],svname) ;
	info_display_int(fields[0],sta.reload) ;
	info_display_int(fields[1],sta.init) ;
	info_display_int(fields[2],sta.unsupervise) ;
	info_display_int(fields[3],sta.state) ;
	info_display_int(fields[4],sta.pid) ;

	if (res.logger && logger)
	{
		svname = res.sa.s + res.logger ;
		if (!ss_state_check(ste,svname)) log_dieusys(111,"unitialized: ",svname) ;
		if (!ss_state_read(&sta,ste,svname)) log_dieusys(111,"read state file of: ",ste,"/",svname) ;

		if (buffer_putsflush(buffer_1,"\n") == -1)
			log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

		info_display_string(fields[6],res.sa.s + res.logreal) ;
		info_display_int(fields[0],sta.reload) ;
		info_display_int(fields[1],sta.init) ;
		info_display_int(fields[2],sta.unsupervise) ;
		info_display_int(fields[3],sta.state) ;
		info_display_int(fields[4],sta.pid) ;
	
	}

	freed:
	ss_resolve_free(&res) ;
	stralloc_free(&satree) ;
	stralloc_free(&src) ;
	stralloc_free(&tmp) ;
	return 0 ;
}
