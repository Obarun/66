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

#include <oblibs/string.h>
#include <oblibs/stralist.h>
#include <oblibs/error2.h>
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

unsigned int VERBOSITY = 1 ;
static stralloc SAENV = STRALLOC_ZERO ;
static genalloc GAENV = GENALLOC_ZERO ; //diuint32, pos in senv
static int MAXVAR = 50 ;
static int MAXFILE = 500 ;
static int MAXENV = 4096 ;

#define USAGE "execl-envfile [ -h help ] [ -f file ] [ -l ] dir prog"

typedef struct exlsn_s exlsn_t, *exlsn_t_ref ;
struct exlsn_s
{
  stralloc vars ;
  stralloc values ;
  genalloc data ; // array of elsubst
  stralloc modifs ;
} ;

#define EXLSN_ZERO { .vars = STRALLOC_ZERO, .values = STRALLOC_ZERO, .data = GENALLOC_ZERO, .modifs = STRALLOC_ZERO }

static inline void info_help (void)
{
  static char const *help =
"execl-envfile <options> dir prog\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-f: file to parse\n"
"	-l: loose\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
}

static int env_substitute(char const *key, char const *val,exlsn_t *info, char const *const *envp,int unexport)
{
	char const *defaultval = "" ;
	char const *x ;
	int insist = 0 ;
		
	eltransforminfo_t si = ELTRANSFORMINFO_ZERO ;
	elsubst_t blah ;
	
	blah.var = info->vars.len ;
	blah.value = info->values.len ;
	
	if (el_vardupl(key, info->vars.s, info->vars.len)) strerr_dief1x(111, "bad substitution key") ;
	if (!stralloc_catb(&info->vars,key, strlen(key) + 1)) retstralloc(111,"env_substitute") ;
	
	x = env_get2(envp, key) ;
	if (!x)
	{
		if (insist) strerr_dief2x(111, val,": is not set") ;
		x = defaultval ;
	}
	else if (unexport)
	{
		if (!stralloc_catb(&info->modifs, key, strlen(key) + 1)) goto err ;
	}
	if (!x) blah.n = 0 ;
	else
	{
		int r ;
		if (!stralloc_cats(&info->values, x)) goto err ;
		r = el_transform(&info->values, blah.value, &si) ;
		if (r < 0) goto err ;
		blah.n = r ;
	}
	
	if (!genalloc_append(elsubst_t, &info->data, &blah)) goto err ;
		
	return 1 ;
	
	err:
		info->vars.len = blah.var ;
		info->values.len = blah.value ;
		return 0 ;
}

int main (int argc, char const *const *argv, char const *const *envp)
{
	int r, i, one, unexport  ;
	int insist = 1 ;
	
	char const *path = 0 ;
	char const *file = 0 ;
	
	stralloc src = STRALLOC_ZERO ;
	stralloc modifs = STRALLOC_ZERO ;
	stralloc dst = STRALLOC_ZERO ;
	genalloc toparse = GENALLOC_ZERO ; //stralist
	
	exlsn_t info = EXLSN_ZERO ;
	
	r = i = one = unexport = 0 ;
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
				case 'f' :  file = l.arg ; one = 1 ; break ;
				case 'l' : 	insist = 0 ; break ;
				default : exitusage(USAGE) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc < 2) exitusage(USAGE) ;
	
	path = *argv ;
	
	if (path[0] != '/') strerr_dief3x(111,"directory: ",path," must be an absolute path") ;
	
	r = dir_get(&toparse,path,"",S_IFREG) ;
	if (!r && insist) strerr_diefu2sys(111,"get file from: ",path) ;
	else if ((!r && !insist) || !genalloc_len(stralist,&toparse))
	{
		argv++;
		argc--;
		xpathexec_run(argv[0],argv,envp) ;
	}
	
	if (one)
	{
		r = stra_findidx(&toparse,file) ;
		if (r < 0) 
		{
			if (insist) strerr_diefu2x(111,"find: ",file) ;
			else
			{
				argv++;
				argc--;
				xpathexec_run(argv[0],argv,envp) ;
			}
		}
		if (!file_readputsa(&src,path,file)) strerr_diefu4sys(111,"read file: ",path,"/",file) ;
		if (!env_parsenclean(&modifs,&src)) strerr_diefu4x(111,"parse and clean environment of: ",path,"/",file)  ;
		if (!env_split(&GAENV,&SAENV,&src)) strerr_diefu4x(111,"split environment of: ",path,"/",file) ;
	}
	else
	{
		for (i = 0 ; i < genalloc_len(stralist,&toparse) ; i++)
		{
			src.len = 0 ;
			if (i > MAXFILE) strerr_dief2x(111,"to many file to parse in: ",path) ;
			if (!file_readputsa(&src,path,gaistr(&toparse,i))) strerr_diefu4sys(111,"read file: ",path,"/",gaistr(&toparse,i)) ;
			if (!env_parsenclean(&modifs,&src)) strerr_diefu4x(111,"parse and clean environment of: ",path,"/",gaistr(&toparse,i)) ;
			if (!env_split(&GAENV,&SAENV,&src)) strerr_diefu4x(111,"split environment of: ",path,"/",gaistr(&toparse,i)) ;
		}
	}
	genalloc_deepfree(stralist,&toparse,stra_free) ;
	stralloc_free(&src) ;
	
	size_t n = env_len(envp) + 1 + byte_count(modifs.s,modifs.len,'\0') ;
	if (n > MAXENV) strerr_dief1x(111,"environment string too long") ;
	char const *newenv[n] ;
	if (!env_merge (newenv, n ,envp,env_len(envp),modifs.s, modifs.len)) strerr_diefu1sys(111,"build environment") ;

	for (i = 0 ; i < genalloc_len(diuint32,&GAENV) ; i++)
	{
			if (i > MAXVAR) strerr_dief1x(111,"to many variables") ;
			unexport = 0 ;
			int key = genalloc_s(diuint32,&GAENV)[i].left ;
			int val = genalloc_s(diuint32,&GAENV)[i].right ;
			if ((SAENV.s+key)[0] == '!')
			{
				key++ ;
				unexport = 1 ;
			}
			env_substitute(SAENV.s + key,SAENV.s + val,&info,newenv,unexport) ;
	}
	genalloc_free(diuint32,&GAENV) ;
	stralloc_free(&SAENV) ;
	
	argv++;
	argc--;

	modifs.len = 0 ;
	if (!env_string (&modifs, argv, (unsigned int) argc)) strerr_diefu1x(111,"make environment string") ;
	r = el_substitute (&dst, modifs.s, modifs.len, info.vars.s, info.values.s,
		genalloc_s (elsubst_t const, &info.data),genalloc_len (elsubst_t const, &info.data)) ;
	if (r < 0) strerr_diefu1sys(111,"el_substitute") ;
	else if (!r) _exit(0) ;

	stralloc_free(&modifs) ;
	{
		char const *v[r + 1] ;
		if (!env_make (v, r ,dst.s, dst.len)) strerr_diefu1sys(111,"make environment") ;
		v[r] = 0 ;
		pathexec_r (v, newenv, env_len(newenv),info.modifs.s,info.modifs.len) ;
	}
}
