/* 
 * execl-envfile.c
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
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
//#include <stdio.h>

#include <oblibs/string.h>
#include <oblibs/stralist.h>
#include <oblibs/error2.h>
#include <oblibs/types.h>
#include <oblibs/obgetopt.h>
#include <oblibs/directory.h>
#include <oblibs/files.h>

#include <skalibs/bytestr.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/env.h>
#include <skalibs/djbunix.h>
#include <skalibs/diuint32.h>
#include <skalibs/env.h>
#include <skalibs/sgetopt.h>

#include <execline/execline.h>

#include <66/parser.h>
#include <66/environ.h>
#include <66/constants.h>

static stralloc SAENV = STRALLOC_ZERO ;
static genalloc GAENV = GENALLOC_ZERO ; //diuint32, pos in senv

#define USAGE "execl-envfile [ -h help ] [ -l ] src prog"

static inline void info_help (void)
{
  static char const *help =
"execl-envfile <options> src prog\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-f: file to parse(deprecated)\n"
"	-l: loose\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
}

int loop_stra(stralloc *sa,char const *search)
{
	size_t i = 0, len = sa->len ;
	for (;i < len; i += strlen(sa->s + i) + 1)
		if (!strcmp(sa->s+i,search)) return 1 ;
	
	return 0 ;
}

int main (int argc, char const *const *argv, char const *const *envp)
{
	int r, i, unexport  ;
	int insist = 1 ;
	size_t pathlen ;
	char const *path = 0 ;
	char const *file = 0 ;
	char tpath[MAXENV + 1] ;
	char tfile[MAXENV+1] ;
	stralloc src = STRALLOC_ZERO ;
	stralloc modifs = STRALLOC_ZERO ;
	stralloc dst = STRALLOC_ZERO ;
	genalloc toparse = GENALLOC_ZERO ; //stralist
	
	exlsn_t info = EXLSN_ZERO ;
	
	r = i = unexport = 0 ;
	insist = 1 ;
	
	PROG = "execl-envfile" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = subgetopt_r(argc,argv, ">hlf:", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'h' : 	info_help(); return 0 ;
				case 'f' :  file = l.arg ;  break ;
				case 'l' : 	insist = 0 ; break ;
				default : exitusage(USAGE) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc < 2) exitusage(USAGE) ;
	
	path = *argv ;
	argv++;
	argc--;
	
	/** Mark -f as deprecated */	
	if (file)
	{
		strerr_warnw1x("-f options is deprecated") ;
		if (path[0] != '/') strerr_dief3x(111,"path of: ",path,": must be absolute") ;
	}
	else
	{
		r = scan_mode(path,S_IFREG) ;
		if (!r) strerr_diefu2sys(111,"find: ",path) ;
		if (r < 0)
		{
			r = scan_mode(path,S_IFDIR) ;
			if (!r) strerr_diefu2sys(111,"find: ",path) ;
			if (r < 0) strerr_dief2x(111,"invalid format of: ", path) ;
			if (path[0] == '.')
			{
				if (!dir_beabsolute(tpath,path)) 
					strerr_diefu2sys(111,"find absolute path of: ",path) ;
			}
			else if (path[0] != '/') strerr_dief3x(111,"path of: ",path,": must be absolute") ;
			else
			{
				pathlen = strlen(path) ;
				memcpy(tpath,path,pathlen);
				tpath[pathlen] = 0 ;
			}
			path = tpath ;
		}
		else
		{
			if (!basename(tfile,path)) strerr_diefu2x(111,"get file name of: ",path) ;
			file = tfile ;
			if (!dirname(tpath,path)) strerr_diefu2x(111,"get parent path of: ",path) ;
			path = tpath ;
		}
	}
	
	r = dir_get(&toparse,path,"",S_IFREG) ;
	if (!r && insist) strerr_diefu2sys(111,"get file from: ",path) ;
	else if ((!r && !insist) || !genalloc_len(stralist,&toparse))
	{
		xpathexec_run(argv[0],argv,envp) ;
	}
	
	if (file)
	{
		r = stra_findidx(&toparse,file) ;
		if (r < 0) 
		{
			if (insist) strerr_diefu2x(111,"find: ",file) ;
			else
			{
				xpathexec_run(argv[0],argv,envp) ;
			}
		}
		
		if (!file_readputsa(&src,path,file)) strerr_diefu4sys(111,"read file: ",path,"/",file) ;
		if (!env_parsenclean(&modifs,&src)) strerr_diefu4x(111,"parse and clean environment of: ",path,"/",file)  ;
		if (!env_split(&GAENV,&SAENV,&src)) strerr_diefu4x(111,"split environment of: ",path,"/",file) ;
		if (genalloc_len(diuint32,&GAENV) > MAXVAR) strerr_dief4x(111,"to many variables in file: ",path,"/",file) ;
	}
	else
	{
		for (i = 0 ; i < genalloc_len(stralist,&toparse) ; i++)
		{
			src.len = 0 ;
			size_t n = genalloc_len(diuint32,&GAENV) + MAXVAR ;
			if (i > MAXFILE) strerr_dief2x(111,"to many file to parse in: ",path) ;
			if (!file_readputsa(&src,path,gaistr(&toparse,i))) strerr_diefu4sys(111,"read file: ",path,"/",gaistr(&toparse,i)) ;
			if (!env_parsenclean(&modifs,&src)) strerr_diefu4x(111,"parse and clean environment of: ",path,"/",gaistr(&toparse,i)) ;
			if (!env_split(&GAENV,&SAENV,&src)) strerr_diefu4x(111,"split environment of: ",path,"/",gaistr(&toparse,i)) ;
			if (genalloc_len(diuint32,&GAENV) > n) strerr_dief4x(111,"to many variables in file: ",path,"/",gaistr(&toparse,i)) ;
		}
	}

	genalloc_deepfree(stralist,&toparse,stra_free) ;
	stralloc_free(&src) ;
	
	/** be able to freed the stralloc before existing */
	char tmp[modifs.len+1] ;
	memcpy(tmp,modifs.s,modifs.len) ;
	tmp[modifs.len] = 0 ;
	
	size_t n = env_len(envp) + 1 + byte_count(modifs.s,modifs.len,'\0') ;
	if (n > MAXENV) strerr_dief1x(111,"environment string too long") ;
	char const *newenv[n + 1] ;
	if (!env_merge (newenv, n ,envp,env_len(envp),tmp, modifs.len)) strerr_diefu1sys(111,"build environment") ;
	
	for (i = 0 ; i < genalloc_len(diuint32,&GAENV) ; i++)
	{
			unexport = 0 ;
			int key = genalloc_s(diuint32,&GAENV)[i].left ;
			int val = genalloc_s(diuint32,&GAENV)[i].right ;
			if ((SAENV.s+val)[0] == SS_VAR_UNEXPORT)
			{
				val++ ;
				unexport = 1 ;
			}
			if(!loop_stra(&info.vars,SAENV.s + key))
				if (!env_substitute(SAENV.s + key,SAENV.s + val,&info,newenv,unexport)) 
					strerr_diefu4x(111,"substitute value of: ",SAENV.s + key," by: ",SAENV.s + val) ;
	}
	genalloc_free(diuint32,&GAENV) ;
	stralloc_free(&SAENV) ;
	
	modifs.len = 0 ;
	if (!env_string (&modifs, argv, (unsigned int) argc)) strerr_diefu1x(111,"make environment string") ;
	
	r = el_substitute (&dst, modifs.s, modifs.len, info.vars.s, info.values.s,
		genalloc_s (elsubst_t const, &info.data),genalloc_len (elsubst_t const, &info.data)) ;
	if (r < 0) strerr_diefu1sys(111,"el_substitute") ;
	else if (!r) _exit(0) ;
	
	stralloc_free(&modifs) ;
	
	char const *v[r + 1] ;
	if (!env_make (v, r ,dst.s, dst.len)) strerr_diefu1sys(111,"make environment") ;
	v[r] = 0 ;
	pathexec_r (v, newenv, env_len(newenv),info.modifs.s,info.modifs.len) ;
}
