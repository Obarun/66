/* 
 * 66-parser.c
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
 
#include <stddef.h>
#include <stdlib.h>
//#include <stdio.h>

#include <oblibs/error2.h>
#include <oblibs/files.h>
#include <oblibs/obgetopt.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>
#include <oblibs/string.h>

#include <skalibs/buffer.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>


#include <66/utils.h>
#include <66/parser.h>

unsigned int VERBOSITY = 1 ;

#define USAGE "66-parser [ -h ] [ -v verbosity ] service destination"

static inline void info_help (void)
{
  static char const *help =
"66-parser <options> dir service\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-v: increase/decrease verbosity\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
}

static int check_dir(char const *dir,int force)
{
	int r ;
	r = scan_mode(dir,S_IFDIR) ;
	if (r < 0) return 0 ;
	
	if (r && force)
	{
		if (rm_rf(dir) < 0) return 0 ;
		if (!r)	return 0 ;
		r = 0 ;
	}
	else if (r && !force) return -1 ;
	if (!r)
		if (!dir_create(dir, 0755)) return 0 ;
	return 1 ;
}

static int setname(char *name,char const *sv)
{
	size_t slen = strlen(sv) ;
	ssize_t svlen = get_rlen_until(sv,'/',slen) ;
	if (svlen <= 0) return 0 ;
	svlen++ ;
	size_t svnamelen = slen - svlen ;
	memcpy(name,sv+svlen,svnamelen) ;
	return 1 ;
}

int main(int argc, char const *const *argv,char const *const *envp)
{
	int r ;
	size_t filesize ;
	stralloc src = STRALLOC_ZERO ;
	sv_alltype service = SV_ALLTYPE_ZERO ;
	char const *dir ;
	char const *sv  ;
	char name[4095+1] ;
	int type, force = 0 ;
	PROG = "66-parser" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">hv:f", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'h' : info_help(); return 0 ;
				case 'v' : if (!uint0_scan(l.arg, &VERBOSITY)) exitusage(USAGE) ; break ;
				case 'f' : force = 1 ; break ;
				default : exitusage(USAGE) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc < 2) exitusage(USAGE) ;
	
	sv = argv[0] ;
	dir = argv[1] ;
	if (dir[0] != '/') strerr_dief3x(110, "directory: ",dir," must be an absolute path") ;
	if (sv[0] != '/') strerr_dief3x(110, "service: ",sv," must be an absolute path") ;
	if (!setname(name,sv)) strerr_diefu1x(111,"parse service name") ;
	r = check_dir(dir,force) ;
	if (r < 1) strerr_dief3x(111,"destination: ",dir," already exist") ;
	else if (!r) strerr_diefu2sys(111,"sanitize directory: ",dir) ;
	filesize=file_get_size(sv) ;
	r = openreadfileclose(sv,&src,filesize) ;
	if (!r) strerr_dief2sys(111,"open: ",sv) ;
	if (!stralloc_cats(&src,"\n")) retstralloc(111,"main") ;
	if (!stralloc_0(&src)) retstralloc(111,"main") ;
	VERBO1 strerr_warni2x("Parsing service file: ", sv) ;
	if (!parser(&service,&src,sv)) strerr_diefu2x(111,"parse service file: ",sv) ;
	VERBO1 strerr_warni4x("Write service file: ", sv," at: ",dir) ;
	type = service.cname.itype ;
	switch(type)
	{
		case CLASSIC:
			if (!write_classic(&service, dir, force))
				strerr_diefu2x(111,"write: ",name) ;
			break ;
		case LONGRUN:
			if (!write_longrun(&service, dir, force))
				strerr_diefu2x(111,"write: ",name) ;
			break ;
		case ONESHOT:
			if (!write_oneshot(&service, dir, force))
				strerr_diefu2x(111,"write: ",name) ;
			break ;
		case BUNDLE:
			if (!write_bundle(&service, dir, force))
				strerr_diefu2x(111,"write: ",name) ;
			break ;
		default: break ;
	}	
	sv_alltype_free(&service) ;
	stralloc_free(&src) ;
	
	return 0 ;
	
}
