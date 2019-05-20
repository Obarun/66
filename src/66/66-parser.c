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
#include <stdio.h>
#include <errno.h>

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

#define USAGE "66-parser [ -h ] [ -v verbosity ] [ -f ] service destination"

static inline void info_help (void)
{
  static char const *help =
"66-parser <options> service destination\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-v: increase/decrease verbosity\n"
"	-f: force to overwrite existing destination\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
}

static void setname(char *name,char const *sv)
{
	size_t slen = strlen(sv) ;
	ssize_t svlen = get_rlen_until(sv,'/',slen) ;
	if (svlen <= 0) strerr_diefu1x(111,"parse service name") ;
	svlen++ ;
	size_t svnamelen = slen - svlen ;
	memcpy(name,sv+svlen,svnamelen) ;
	name[svnamelen] = 0 ;
}

static void setsrc(char *src,char const *dir)
{
	size_t dirlen = strlen(dir) ;
	char name[dirlen+1] ;
	setname(name,dir) ;
	size_t namelen = strlen(name) ;
	memcpy(src,dir,dirlen-namelen-1) ;
	src[dirlen-namelen-1] = 0 ;
}

static void check_dir(char const *dir,int force,int main)
{
	int r ;
	size_t dirlen = strlen(dir) ;
	char tmp[dirlen+1] ;
	setsrc(tmp,dir) ;
	char name[dirlen+1] ;
	setname(name,dir) ;
	
	r = scan_mode(dir,S_IFDIR) ;
	if (r < 0){ errno = ENOTDIR ; strerr_dief2sys(111,"conflicting format of: ",dir) ; }
	
	if (r && force && main)
	{
		if ((rm_rf(dir) < 0) || !r ) strerr_diefu2sys(111,"sanitize directory: ",dir) ;
		r = 0 ;
	}
	else if (r && !force && main) strerr_dief3x(111,"destination: ",dir," already exist") ;
	if (!r)
		if (!dir_create_under(tmp,name, 0755)) strerr_diefu2sys(111,"create: ",dir) ;
}

int main(int argc, char const *const *argv,char const *const *envp)
{
	int r ;
	stralloc src = STRALLOC_ZERO ;
	stralloc dst = STRALLOC_ZERO ;
	stralloc insta = STRALLOC_ZERO ;
	sv_alltype service = SV_ALLTYPE_ZERO ;
	size_t srcdirlen ;
	char const *dir ;
	char const *sv  ;
	char name[4095+1] ;
	char srcdir[4095+1] ;
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
	setname(name,sv) ;
	size_t svlen = strlen(sv) ;
	size_t namelen = strlen(name) ;
	char tmp[svlen + 1 + namelen + 1] ;
	if (scan_mode(sv,S_IFDIR))
	{
		memcpy(tmp,sv,svlen) ;
		tmp[svlen] = '/' ;
		memcpy(tmp + svlen + 1, name,namelen) ;
		tmp[svlen + 1 + namelen] = 0 ;
		sv = tmp ;
	}
	setsrc(srcdir,sv) ;
	check_dir(dir,force,0) ;
	if (!stralloc_cats(&insta,name) ||
	!stralloc_0(&insta)) retstralloc(111,"main") ;
	r = insta_check(insta.s) ;
	if (!r) strerr_dief2x(111,"invalid instance name: ",insta.s) ;
	if (r > 0)
	{
		if (!insta_create(&src,&insta,srcdir,r))
			strerr_diefu2x(111,"create instance service: ",name) ;
		memcpy(name,insta.s,insta.len) ;
		name[insta.len] = 0 ;
		
	}
	else if (!read_svfile(&src,name,srcdir)) strerr_dief2sys(111,"open: ",sv) ;
		
	VERBO1 strerr_warni2x("Parsing service file: ", sv) ;
	if (!parser(&service,&src,sv)) strerr_diefu2x(111,"parse service file: ",sv) ;
	if (!stralloc_cats(&dst,dir) ||
	!stralloc_cats(&dst,"/") ||
	!stralloc_cats(&dst,name) ||
	!stralloc_0(&dst)) retstralloc(111,"main") ;
	check_dir(dst.s,force,1) ;
	VERBO1 strerr_warni4x("Write service file: ", name," at: ",dst.s) ;
	type = service.cname.itype ;
	srcdirlen = strlen(srcdir) ;
	service.src = keep.len ;
	if (!stralloc_catb(&keep,srcdir,srcdirlen + 1)) retstralloc(111,"main") ;
	switch(type)
	{
		case CLASSIC:
			if (!write_classic(&service, dst.s, force))
				strerr_diefu2x(111,"write: ",name) ;
			break ;
		case LONGRUN:
			if (!write_longrun(&service, dst.s, force))
				strerr_diefu2x(111,"write: ",name) ;
			break ;
		case ONESHOT:
			if (!write_oneshot(&service, dst.s, force))
				strerr_diefu2x(111,"write: ",name) ;
			break ;
		case BUNDLE:
			if (!write_bundle(&service, dst.s, force))
				strerr_diefu2x(111,"write: ",name) ;
			break ;
		default: break ;
	}	
	sv_alltype_free(&service) ;
	stralloc_free(&src) ;
	stralloc_free(&dst) ;
	return 0 ;
	
}
