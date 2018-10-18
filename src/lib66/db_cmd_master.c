/* 
 * db_cmd_start.c
 * 
 * Copyright (c) 2018 Eric Vidal <eric@obarun.org>
 * 
 * All rights reserved.
 * 
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */
 
#include <66/db.h>

#include <s6-rc/config.h>//S6RC_BINPREFIX

#include <sys/stat.h>
#include <unistd.h>

#include <oblibs/error2.h>
#include <oblibs/string.h>
#include <oblibs/stralist.h>
#include <oblibs/files.h>
#include <oblibs/obgetopt.h>
#include <oblibs/directory.h>

#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/avltree.h>

#include <66/constants.h>
#include <66/parser.h>
#include <66/utils.h>


#include <stdio.h>
//USAGE "db_update_start [ -v verbosity ] [ -a add ] [ -d delete ] [ -c copy to ] [ -D directory ] service"
// -c -> copy the contents file to the given directory, in this case service is not mandatory
int db_update_master(int argc, char const *const *argv)
{
	int r ;
	unsigned int add, del, copy, verbosity ;
	
	verbosity = 1 ;
	
	add = del = copy = 0 ;
	
	stralloc dir = STRALLOC_ZERO ; 
	stralloc contents = STRALLOC_ZERO ;
	stralloc tocopy = STRALLOC_ZERO ;
	
	genalloc in = GENALLOC_ZERO ;//type stralist
	genalloc gargv = GENALLOC_ZERO ;//type stralist	
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, "v:adc:D:", &l) ;
			if (opt == -1) break ;
			if (opt == -2){ strerr_warnw1x("options must be set first") ; return -1 ; }
			switch (opt)
			{
				case 'v' : 	if (!uint0_scan(l.arg, &verbosity)) return -1 ;  break ;
				case 'a' : 	add = 1 ; if (del) return -1 ; break ;
				case 'd' :	del = 1 ; if (add) return -1 ; break ;
				case 'D' :	if(!stralloc_cats(&dir,l.arg)) return -1 ; break ;
				case 'c' : 	copy = 1 ; 
							if(!stralloc_cats(&tocopy,l.arg)) return -1 ; 
							if(!stralloc_0(&tocopy)) return -1 ; 
							break ;
				default : 	return -1 ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc < 1 && !copy) return -1 ;
	if (!add && !del && !copy) return -1 ;
	if (!dir.len) return -1 ;
	if (copy)
		if (dir_scan_absopath(tocopy.s) < 0) return -1 ;

	size_t newlen ;
	char dst[dir.len + SS_DB_LEN + SS_SRC_LEN + SS_MASTER_LEN + 1 + SS_CONTENTS_LEN + 1] ;
	
	if (dir_scan_absopath(dir.s) < 0) goto err ;
	
	for(;*argv;argv++)
		if (!stra_add(&gargv,*argv)) retstralloc(111,"main") ;
	
	memcpy(dst, dir.s, dir.len) ;
	memcpy(dst + dir.len, SS_DB, SS_DB_LEN) ;
	memcpy(dst + dir.len + SS_DB_LEN, SS_SRC, SS_SRC_LEN) ;
	memcpy(dst + dir.len + SS_DB_LEN + SS_SRC_LEN, SS_MASTER, SS_MASTER_LEN) ;
	memcpy(dst + dir.len + SS_DB_LEN + SS_SRC_LEN + SS_MASTER_LEN, "/" , 1) ;
	newlen = dir.len + SS_DB_LEN + SS_SRC_LEN + SS_MASTER_LEN + 1 ;
	memcpy(dst + dir.len + SS_DB_LEN + SS_SRC_LEN + SS_MASTER_LEN + 1, SS_CONTENTS, SS_CONTENTS_LEN) ;
	dst[dir.len + SS_DB_LEN + SS_SRC_LEN + SS_MASTER_LEN + 1 + SS_CONTENTS_LEN] = 0 ;
	
	stralloc_free(&dir) ;
	
	size_t filesize=file_get_size(dst) ;

	r = openreadfileclose(dst,&contents,filesize) ;
	if(!r)
	{
		VERBO3 strerr_warnwu2sys("open: ", dst) ;
		goto err ;
	}
	/** ensure that we have an empty line at the end of the string*/
	if (!stralloc_cats(&contents,"\n")) goto retstra ;
	if (!stralloc_0(&contents)) goto retstra ;
	
	if (!clean_val(&in,contents.s))
	{
		VERBO3 strerr_warnwu2x("clean: ",contents.s) ;
		goto err ;
	}
	contents = stralloc_zero ;

	if (add)
	{
		for (unsigned int i = 0 ; i < genalloc_len(stralist,&gargv) ; i++)
		{

			if (!stra_cmp(&in,gaistr(&gargv,i)))
			{
				if (!stra_add(&in,gaistr(&gargv,i)))
				{
					VERBO3 strerr_warnwu4x("add: ",gaistr(&gargv,i)," in ",dst) ; 
					goto err ;
				}
			}
		}
	}
	
	if (del)
	{
		for (unsigned int i = 0 ; i < genalloc_len(stralist,&gargv) ; i++)
		{
			if (stra_cmp(&in,gaistr(&gargv,i)))
			{
				if (!stra_remove(&in,gaistr(&gargv,i)))
				{
					VERBO3 strerr_warnwu4x("remove: ",gaistr(&gargv,i)," in ",dst) ; 
					goto err ;
				}
			}
		}
	}
	
	for (unsigned int i =0 ; i < genalloc_len(stralist,&in); i++)
	{
		if (!stralloc_cats(&contents,gaistr(&in,i))) goto retstra ;
		if (!stralloc_cats(&contents,"\n")) goto retstra ;
	}
	dst[newlen] = 0 ;
	
	r = file_write_unsafe(dst,"contents",contents.s,contents.len) ;
	if (!r) 
	{ 
		VERBO3 strerr_warnwu3sys("write: ",dst,"contents") ;
		goto err ;
	}

	if (copy)
	{
		r = file_write_unsafe(tocopy.s,"contents",contents.s,contents.len) ;
		if (!r) 
		{ 
			VERBO3 strerr_warnwu3sys("write: ",tocopy.s,"/contents") ;
			goto err ;
		}
	}
	stralloc_free(&contents) ;
	genalloc_deepfree(stralist,&in,stra_free) ;
	genalloc_deepfree(stralist,&gargv,stra_free) ;

	return 1 ;
	
	retstra:
		stralloc_free(&dir) ;
		stralloc_free(&contents) ;
		genalloc_deepfree(stralist,&in,stra_free) ;
		genalloc_deepfree(stralist,&gargv,stra_free) ;
		retstralloc(-1,"update_start") ;
		
	err:
		stralloc_free(&dir) ;
		stralloc_free(&contents) ;
		genalloc_deepfree(stralist,&in,stra_free) ;
		genalloc_deepfree(stralist,&gargv,stra_free) ;
		return -1 ;
}

int db_cmd_master(unsigned int verbosity,char const *cmd)
{	
	
	int r ;
	genalloc opts = GENALLOC_ZERO ;
	
	if (!clean_val(&opts,cmd))
	{
		VERBO3 strerr_warnwu2x("clean: ",cmd) ;
		return -1 ;
	}
	int newopts = 4 + genalloc_len(stralist,&opts) ;
	char const *newargv[newopts] ;
	unsigned int m = 0 ;
	char fmt[UINT_FMT] ;
	fmt[uint_fmt(fmt, verbosity)] = 0 ;
	
	newargv[m++] = "update_start" ;
	newargv[m++] = "-v" ;
	newargv[m++] = fmt ;
	
	for (unsigned int i = 0; i < genalloc_len(stralist,&opts); i++)
		newargv[m++] = gaistr(&opts,i) ;
	
	newargv[m++] = 0 ;
	
	r = db_update_master(newopts,newargv) ;
	
	genalloc_deepfree(stralist,&opts,stra_free) ;
	
	return r ;
}
