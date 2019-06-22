/* 
 * environ.c
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
#include <stdio.h>

#include <oblibs/string.h>
#include <oblibs/stralist.h>
#include <oblibs/directory.h>
#include <oblibs/files.h>
#include <oblibs/types.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/diuint32.h>
#include <skalibs/env.h>
#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>
#include <skalibs/bytestr.h>

#include <66/parser.h>
#include <66/environ.h>
#include <66/utils.h>

#include <execline/execline.h>
/* @Return 1 on success
 * @Return 0 on fail
 * @Return -1 for empty line */
int env_clean(stralloc *src)
{
	int r, e = 1 ;
	unsigned int i ;
	genalloc gatmp =GENALLOC_ZERO ;//stralist
	stralloc kp = STRALLOC_ZERO ;
	stralloc tmp = STRALLOC_ZERO ;
		
	size_t pos = 0 ;
	char const *file = "env_clean" ;
	parse_mill_t line = { .open = '@', .close = '=', \
							.skip = " \t\r", .skiplen = 3, \
							.end = "\n", .endlen = 1, \
							.jump = "#", .jumplen = 1,\
							.check = 0, .flush = 0, \
							.forceskip = 0, .force = 1, \
							.inner = PARSE_MILL_INNER_ZERO } ;
	parse_mill_t line_end = { .open = '@', .close = '\n', \
							.skip = " \t\r", .skiplen = 3, \
							.end = "\n", .endlen = 1, \
							.jump = "#", .jumplen = 1,\
							.check = 0, .flush = 0, \
							.forceskip = 0, .force = 1, \
							.inner = PARSE_MILL_INNER_ZERO } ;
	
	size_t blen = src->len, n = 0 ;
	if (!stralloc_inserts(src,0,"@")) goto err ;
	while(pos < (blen+n) && n < 2)
	{
		kp.len = 0 ;
		genalloc_deepfree(stralist,&gatmp,stra_free) ;
		line.inner.nopen = line.inner.nclose = 0 ;
		r = parse_config(!n?&line:&line_end,file,src,&kp,&pos) ;
		if (!r) goto err ;
		if (r < 0 && !n){ e = -1 ; goto freed ; }
		if (!stralloc_0(&kp)) goto err ;
		if (!clean_val(&gatmp,kp.s)) goto err ;
		for (i = 0 ; i < genalloc_len(stralist,&gatmp) ; i++)
		{	
			if ((i+1) < genalloc_len(stralist,&gatmp))
			{
				if (!stralloc_cats(&tmp,gaistr(&gatmp,i))) goto err ;
				if (!stralloc_cats(&tmp," ")) goto err ;
			}
			else if (!stralloc_cats(&tmp,gaistr(&gatmp,i))) goto err ;
		}
		if (!n) if (!stralloc_cats(&tmp,"=")) goto err ;
		if (!stralloc_inserts(src,pos,"@")) goto err ;
		n++;
	}
	
	if (!stralloc_0(&tmp)) goto err ;
	if (!stralloc_copy(src,&tmp)) goto err ;
	
	freed:
		stralloc_free(&kp) ;
		stralloc_free(&tmp) ;
		genalloc_deepfree(stralist,&gatmp,stra_free) ;
		return e ;
	
	err:
		stralloc_free(&kp) ;
		stralloc_free(&tmp) ;
		genalloc_deepfree(stralist,&gatmp,stra_free) ;
		return 0 ;
}

int env_split_one(char *line,genalloc *ga,stralloc *sa)
{
	char *k = 0 ;
	char *v = 0 ;
	diuint32 tmp = DIUINT32_ZERO ;
	k = line ;
	v = line ;
	obstr_sep(&v,"=") ;
	if (v == NULL) return 0 ;
		
	tmp.left = sa->len ;
	if(!stralloc_catb(sa,k,strlen(k)+1)) return 0 ;
	
	if (!obstr_trim(v,'\n')) return 0 ;
	tmp.right = sa->len ;
	if(!stralloc_catb(sa,v,strlen(v)+1)) return 0 ;
	
	if (!genalloc_append(diuint32,ga,&tmp)) return 0 ;
		
	return 1 ;
}

int env_split(genalloc *gaenv,stralloc *saenv,stralloc *src)
{
	int nbline = 0, i = 0 ;
	genalloc gatmp = GENALLOC_ZERO ;//stralist
	stralloc tmp = STRALLOC_ZERO ;
	nbline = get_nbline_ga(src->s,src->len,&gatmp) ;
	for (; i < nbline ; i++)
	{
		char *line = gaistr(&gatmp,i) ;
		tmp.len = 0 ;
		stralloc_cats(&tmp,line) ;
		/** skip commented line or empty line*/
		if (env_clean(&tmp) < 0) continue ;
		if (*line)
			if (!env_split_one(tmp.s,gaenv,saenv)) goto err ;
	}
	genalloc_deepfree(stralist,&gatmp,stra_free) ;
	stralloc_free(&tmp) ;
	return 1 ;
	err: 
		genalloc_deepfree(stralist,&gatmp,stra_free) ;
		stralloc_free(&tmp) ;
		return 0 ;
}

int env_parsenclean(stralloc *modifs,stralloc *src)
{
	int nbline = 0, i = 0 ;
	size_t pos = 0 ;
	genalloc gatmp = GENALLOC_ZERO ;//stralist
	stralloc tmp = STRALLOC_ZERO ;
	nbline = get_nbline_ga(src->s,src->len,&gatmp) ;
	
	for (; i < nbline ; i++)
	{
		tmp.len = 0 ;
		if (!gaistrlen(&gatmp,i)) break ;
		if (!stralloc_cats(&tmp,gaistr(&gatmp,i))) goto err ;
		if (!parse_env(&tmp,&pos)) goto err ;
		if (!env_clean(&tmp)) goto err ;
		tmp.len--;//remove '0'
		int u = 0 ;
		if (tmp.s[0] == '!') u++ ;
		if (!stralloc_catb(modifs,tmp.s + u ,(tmp.len - u) + 1)) goto err ;// ||
//		!stralloc_0(modifs)) goto err ;
	}
	
	genalloc_deepfree(stralist,&gatmp,stra_free) ;
	stralloc_free(&tmp) ;
	return 1 ;
	err:
		genalloc_deepfree(stralist,&gatmp,stra_free) ;
		stralloc_free(&tmp) ;
		return 0 ;
}

int make_env_from_line(char const **v,stralloc *sa)
{
	genalloc gatmp = GENALLOC_ZERO ;
	stralloc copy = STRALLOC_ZERO ;
	unsigned int i = 0 ;
	if (!sa->len) goto err ;
	if (!clean_val(&gatmp,sa->s)) goto err ;
	for (;i < genalloc_len(stralist,&gatmp) ; i++)
	{
		char *line = gaistr(&gatmp,i) ;
		if (!stralloc_catb(&copy,line,gaistrlen(&gatmp,i) + 1)) goto err ;
	}
	stralloc_copy(sa,&copy) ;
	stralloc_free(&copy) ;
	if (!env_make(v,i,sa->s,sa->len)) goto err ;
	genalloc_deepfree(stralist,&gatmp,stra_free) ;
	return i ;
	err:
		stralloc_free(&copy) ;
		genalloc_deepfree(stralist,&gatmp,stra_free) ;
		return 0 ;
}

int env_substitute(char const *key, char const *val,exlsn_t *info, char const *const *envp,int unexport)
{
	char const *defaultval = "" ;
	char const *x ;
	int insist = 0 ;
		
	eltransforminfo_t si = ELTRANSFORMINFO_ZERO ;
	elsubst_t blah ;
	
	blah.var = info->vars.len ;
	blah.value = info->values.len ;
	
	if (el_vardupl(key, info->vars.s, info->vars.len)) { strerr_warnw1x("bad substitution key") ; goto err ; }
	if (!stralloc_catb(&info->vars,key, strlen(key) + 1)) { strerr_warnw1x("env_substitute") ; goto err ; }
	
	x = env_get2(envp, key) ;
	if (!x)
	{
		if (insist) { strerr_warnw2x(val,": is not set") ; goto err ; }
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

int env_addkv (const char *key, const char *val, exlsn_t *info)
{
    int r ;
    eltransforminfo_t si = ELTRANSFORMINFO_ZERO ;
    elsubst_t blah ;

    blah.var = info->vars.len ;
    blah.value = info->values.len ;

    if (el_vardupl (key, info->vars.s, info->vars.len)) goto err ;
    if (!stralloc_catb (&info->vars, key, strlen(key) + 1)) goto err ;
    if (!stralloc_cats (&info->values, val)) goto err ;
   
	r = el_transform (&info->values, blah.value, &si) ;
	if (r < 0) goto err;
	blah.n = r ;
   
	if (!genalloc_append (elsubst_t, &info->data, &blah)) goto err ;

	return 1 ;
	err:
		info->vars.len = blah.var ;
		info->values.len = blah.value ;
		return 0 ;
}

int env_get_from_src(stralloc *modifs,char const *src)
{
	int r ;
	size_t filesize ;
	unsigned int i ;
	stralloc sa = STRALLOC_ZERO ;
	genalloc toparse = GENALLOC_ZERO ;
	r = scan_mode(src,S_IFDIR) ;
	if (r < 0)
	{ 
		r = scan_mode(src,S_IFREG) ;
		if (!r || r < 0)
		{
			VERBO3 strerr_warnw2sys("invalid environment: ",src) ;
			goto err ;
		}
		filesize=file_get_size(src) ;
		if (filesize > MAXENV)
		{
			VERBO3 strerr_warnw2x("environment too long: ",src) ;
			goto err ;
		}
		if (!openreadfileclose(src,&sa,filesize))
		{
			VERBO3 strerr_warnwu2sys("open: ",src ) ;
			goto err ;
		} 
		if (!env_parsenclean(modifs,&sa))
		{
			VERBO3 strerr_warnwu2x("parse and clean environment of: ",sa.s)  ;
			goto err ;
		}
	}
	else if (!r)
	{
		VERBO3 strerr_warnw2sys("invalid environment: ",src) ;
		goto err ;
	}
	/** we parse all file of the directory*/
	else
	{
		r = dir_get(&toparse,src,"",S_IFREG) ;
		if (!r)
		{
			VERBO3 strerr_warnwu2sys("get file from: ",src) ;
			goto err ;
		}
		for (i = 0 ; i < genalloc_len(stralist,&toparse) ; i++)
		{
			sa.len = 0 ;
			if (i > MAXFILE)
			{
				VERBO3 strerr_warnw2x("to many file to parse in: ",src) ;
				goto err ;
			}
			if (!file_readputsa(&sa,src,gaistr(&toparse,i)))
			{
				VERBO3 strerr_warnw4x("read file: ",src,"/",gaistr(&toparse,i)) ;
				goto err ;
			} 
			if (!env_parsenclean(modifs,&sa))
			{
				VERBO3 strerr_warnw4x("parse and clean environment of: ",src,"/",gaistr(&toparse,i)) ;
				goto err ;
			} 
		}
	}
	genalloc_deepfree(stralist,&toparse,stra_free) ;
	stralloc_free(&sa) ;
	return 1 ;
	err:
		genalloc_deepfree(stralist,&toparse,stra_free) ;
		stralloc_free(&sa) ;
		return 0 ;
}

size_t build_env(char const *src,char const *const *envp,char const **newenv, char *tmpenv)
{
	
	stralloc modifs = STRALLOC_ZERO ;
	size_t envlen = env_len(envp) ;
	
	if (!env_get_from_src(&modifs,src)) 
	{
		VERBO3 strerr_warnw2x("get environment file from: ",src) ;
		goto err ;
	}
	size_t n = env_len(envp) + 1 + byte_count(modifs.s,modifs.len,'\0') ;
	size_t mlen = modifs.len ;
	if (mlen > MAXENV)
	{
		VERBO3 strerr_warnw2x("environment too long: ",src) ;
		goto err ;
	}
	memcpy(tmpenv,modifs.s,mlen) ;
	tmpenv[mlen] = 0 ;
	
	if (!env_merge(newenv, n, envp, envlen, tmpenv, mlen))
	{
		VERBO3 strerr_warnwu2x("merge environment from: ",src) ;
		goto err ;
	}
	
	stralloc_free(&modifs) ;
		
	return 1 ;
	err:
		stralloc_free(&modifs) ;
		return 0 ;
}
