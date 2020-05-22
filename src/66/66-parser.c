/* 
 * 66-parser.c
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
 
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
//#include <stdio.h>

#include <oblibs/log.h>
#include <oblibs/files.h>
#include <oblibs/obgetopt.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>
#include <oblibs/string.h>
#include <oblibs/sastr.h>

#include <skalibs/buffer.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/utils.h>
#include <66/parser.h>
#include <66/constants.h>

#define USAGE "66-parser [ -h ] [ -z ] [ -v verbosity ] [ -f ] [ -c|C ] service destination"

static inline void info_help (void)
{
  static char const *help =
"66-parser <options> service destination\n"
"\n"
"options :\n"
"	-h: print this help\n"
"	-z: use color\n"
"	-v: increase/decrease verbosity\n"
"	-f: force to overwrite existing destination\n"
"	-c: only appends new key=value pairs at environment configuration file from frontend file\n"
"	-m: appends new key=value and merge existing one at environment configuration file from frontend file\n"
"	-C: overwrite it environment configuration file from frontend file\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
}

static void check_dir(char const *dir,uint8_t force,int main)
{
	int r ;
	
	r = scan_mode(dir,S_IFDIR) ;
	if (r < 0){ errno = ENOTDIR ; log_diesys(LOG_EXIT_SYS,"conflicting format of: ",dir) ; }
	
	if (r && force && main)
	{
		if ((rm_rf(dir) < 0) || !r ) log_dieusys(LOG_EXIT_SYS,"sanitize directory: ",dir) ;
		r = 0 ;
	}
	else if (r && !force && main) log_die(LOG_EXIT_SYS,"destination: ",dir," already exist") ;
	if (!r)
		if (!dir_create_parent(dir, 0755)) log_dieusys(LOG_EXIT_SYS,"create: ",dir) ;
}

int main(int argc, char const *const *argv,char const *const *envp)
{
	int ista ;
	stralloc src = STRALLOC_ZERO ;
	stralloc dst = STRALLOC_ZERO ;
	stralloc insta = STRALLOC_ZERO ;
	sv_alltype service = SV_ALLTYPE_ZERO ;
	size_t srcdirlen, namelen ;
	char const *dir ;
	char const *sv  ;
	char name[4095+1] ;
	char srcdir[4095+1] ;
	unsigned int type ;
	uint8_t force = 0 , conf = 0 ;

	log_color = &log_color_disable ;

	PROG = "66-parser" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">hv:fcCz", &l) ;
			if (opt == -1) break ;
			if (opt == -2) log_die(LOG_EXIT_USER,"options must be set first") ;
			switch (opt)
			{
				case 'h' : 	info_help(); return 0 ;
				case 'v' : 	if (!uint0_scan(l.arg, &VERBOSITY)) log_usage(USAGE) ; break ;
				case 'f' : 	force = 1 ; break ;
				case 'c' : 	if (conf) log_usage(USAGE) ; conf = 1 ; break ;
				case 'm' :	if (conf) log_usage(USAGE) ; conf = 2 ; break ;
				case 'C' : 	if (conf) log_usage(USAGE) ; conf = 3 ; break ;
				case 'z' :	log_color = !isatty(1) ? &log_color_disable : &log_color_enable ; break ;
				default : 	log_usage(USAGE) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc < 2) log_usage(USAGE) ;
	
	sv = argv[0] ;
	dir = argv[1] ;

	if (dir[0] != '/') log_die(LOG_EXIT_USER, "directory: ",dir," must be an absolute path") ;
	if (sv[0] != '/') log_die(LOG_EXIT_USER, "service: ",sv," must be an absolute path") ;

	if (!ob_basename(name,sv)) log_dieu(LOG_EXIT_SYS,"set name");

	namelen = strlen(name) ;

	if (!ob_dirname(srcdir,sv)) log_dieu(LOG_EXIT_SYS,"set directory name") ;

	check_dir(dir,force,0) ;

	if (!auto_stra(&insta,name)) log_die_nomem("stralloc") ;

	ista = instance_check(insta.s) ;
	if (!ista) log_die(LOG_EXIT_SYS,"invalid instance name: ",insta.s) ;

	if (ista > 0)
	{
		if (!instance_splitname(&insta,name,ista,SS_INSTANCE_TEMPLATE)) log_dieu(LOG_EXIT_SYS,"split instance name of: ",name) ;
	}
	
	log_trace("read service file of: ",srcdir,insta.s) ;
	if (read_svfile(&src,insta.s,srcdir) <= 0) log_dieusys(LOG_EXIT_SYS,"open: ",sv) ;

	if (!get_svtype(&service,src.s)) log_dieu(LOG_EXIT_SYS,"get service type of: ",sv) ;

	if (ista > 0)
	{
		if (!instance_create(&src,name,SS_INSTANCE_REGEX,ista))
			log_dieu(LOG_EXIT_SYS,"create instance service: ",name) ;
		memcpy(name,insta.s,insta.len) ;
		name[insta.len] = 0 ;
		
	}
	
	if (!parser(&service,&src,sv,service.cname.itype)) log_dieu(LOG_EXIT_SYS,"parse service file: ",sv) ;

	if (!auto_stra(&dst,dir,"/",name)) log_die_nomem("stralloc") ;

	check_dir(dst.s,force,1) ;

	type = service.cname.itype ;
	srcdirlen = strlen(srcdir) ;
	service.src = keep.len ;

	if (!stralloc_catb(&keep,srcdir,srcdirlen + 1)) log_die_nomem("stralloc") ;
	/**quick fix
	 * WIP on parser this will change soon*/
	if (ista > 0 && service.cname.name >= 0 )
	{
		stralloc sainsta = STRALLOC_ZERO ;
		stralloc saname = STRALLOC_ZERO ;
		if (!stralloc_cats(&saname,keep.s + service.cname.name)) log_die_nomem("stralloc") ;
		
		if (!instance_splitname(&sainsta,name,ista,SS_INSTANCE_TEMPLATE)) log_dieu(LOG_EXIT_SYS,"split instance name: ",name) ;
		if (sastr_find(&saname,sainsta.s) == -1)
			log_die(LOG_EXIT_USER,"invalid instantiated service name: ", keep.s + service.cname.name) ;
			
		stralloc_free(&sainsta) ;
		stralloc_free(&saname) ;
	}
	else
	{
		service.cname.name = keep.len ;
		if (!stralloc_catb(&keep,name,namelen + 1)) log_die_nomem("stralloc") ;
	}

	/* save and prepare environment file */
	if (service.opts[2])
	{
		
		stralloc conf = STRALLOC_ZERO ;
		if (!stralloc_catb(&conf,dst.s,dst.len-1) ||
		!stralloc_cats(&conf,"/env/") || 
		!stralloc_0(&conf)) log_die_nomem("stralloc") ;
		if (!scan_mode(conf.s,S_IFDIR))
		{
			if (!dir_create_parent(conf.s,0755)) log_dieusys(LOG_EXIT_SYS,"environment directory: ",conf.s) ;
		}
		service.srconf = keep.len ;
		if (!stralloc_catb(&keep,conf.s,conf.len + 1)) log_die_nomem("stralloc") ;
		stralloc_free(&conf) ;
	}

	switch(type)
	{
		case TYPE_CLASSIC:
			if (!write_classic(&service, dst.s, force, conf))
				log_dieu(LOG_EXIT_SYS,"write: ",name) ;
			break ;
		case TYPE_LONGRUN:
			if (!write_longrun(&service, dst.s, force, conf))
				log_dieu(LOG_EXIT_SYS,"write: ",name) ;
			break ;
		case TYPE_ONESHOT:
			if (!write_oneshot(&service, dst.s, conf))
				log_dieu(LOG_EXIT_SYS,"write: ",name) ;
			break ;
		case TYPE_MODULE:
			if (!write_common(&service,dst.s, conf))
				log_warnu_return(LOG_EXIT_ZERO,"write common files") ;
		case TYPE_BUNDLE:
			if (!write_bundle(&service, dst.s))
				log_dieu(LOG_EXIT_SYS,"write: ",name) ;
			break ;
		default: break ;
	}

	log_info("Written successfully: ",name, " at: ",dir) ;

	sv_alltype_free(&service) ;
	stralloc_free(&keep) ;
	stralloc_free(&deps) ;
	stralloc_free(&src) ;
	stralloc_free(&dst) ;

	return 0 ;
	
}
