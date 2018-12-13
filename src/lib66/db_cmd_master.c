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
#include <oblibs/strakeyval.h>

#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/avltree.h>

#include <66/constants.h>
#include <66/parser.h>
#include <66/utils.h>
#include <66/graph.h>

//#include <stdio.h>
//USAGE "db_update_start [ -v verbosity ] [ -a add ] [ -d delete ] [ -c copy to ] [ -B bundle ] [ -D directory ] service"
// -c -> copy the contents file to the given directory, in this case service is not mandatory
int db_update_master(int argc, char const *const *argv)
{
	int r ;
	unsigned int add, del, copy, verbosity ;
	
	verbosity = 1 ;
	
	add = del = copy = 0 ;
	
	stralloc contents = STRALLOC_ZERO ;
	stralloc tocopy = STRALLOC_ZERO ;
	
	genalloc in = GENALLOC_ZERO ;//type stralist
	genalloc gargv = GENALLOC_ZERO ;//type stralist	
	
	char const *bundle = 0 ;
	char const *dir = 0 ;
	
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, "v:adc:D:B:", &l) ;
			if (opt == -1) break ;
			if (opt == -2){ strerr_warnw1x("options must be set first") ; return 0 ; }
			switch (opt)
			{
				case 'v' : 	if (!uint0_scan(l.arg, &verbosity)) return 0 ;  break ;
				case 'a' : 	add = 1 ; if (del) return 0 ; break ;
				case 'd' :	del = 1 ; if (add) return 0 ; break ;
				case 'D' :	dir = l.arg ; break ;
				case 'B' :	bundle = l.arg ; break ;
				case 'c' : 	copy = 1 ; 
							if(!stralloc_cats(&tocopy,l.arg)) return 0 ; 
							if(!stralloc_0(&tocopy)) return 0 ; 
							break ;
				default : 	return 0 ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc < 1 && !copy) return 0 ;
	if (!add && !del && !copy) return 0 ;
	if (!dir) return 0 ;
	if (!bundle) bundle = SS_MASTER ;
	if (copy)
		if (dir_scan_absopath(tocopy.s) < 0) return 0 ;

	size_t dirlen = strlen(dir) ;
	size_t bundlen = strlen(bundle) ;
	size_t newlen ;
	char dst[dirlen + SS_DB_LEN + SS_SRC_LEN + bundlen + 1 + SS_CONTENTS_LEN + 1] ;
	
	if (dir_scan_absopath(dir) < 0) goto err ;
	
	for(;*argv;argv++)
		if (!stra_add(&gargv,*argv)) retstralloc(111,"main") ;
	
	memcpy(dst, dir, dirlen) ;
	memcpy(dst + dirlen, SS_DB, SS_DB_LEN) ;
	memcpy(dst + dirlen + SS_DB_LEN, SS_SRC, SS_SRC_LEN) ;
	dst[dirlen + SS_DB_LEN + SS_SRC_LEN] = '/' ;
	memcpy(dst + dirlen + SS_DB_LEN + SS_SRC_LEN + 1, bundle, bundlen) ;
	dst[dirlen + SS_DB_LEN + SS_SRC_LEN + 1 + bundlen] = '/' ;
	newlen = dirlen + SS_DB_LEN + SS_SRC_LEN + 1 + bundlen + 1 ;
	memcpy(dst + newlen, SS_CONTENTS, SS_CONTENTS_LEN) ;
	dst[newlen + SS_CONTENTS_LEN] = 0 ;

	size_t filesize=file_get_size(dst) ;

	r = openreadfileclose(dst,&contents,filesize) ;
	if(!r)
	{
		VERBO3 strerr_warnwu2sys("open: ", dst) ;
		goto err ;
	}
	/** ensure that we have an empty line at the end of the string*/
	if (!stralloc_cats(&contents,"\n")) goto err ;
	if (!stralloc_0(&contents)) goto err ;
	
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
		if (!stralloc_cats(&contents,gaistr(&in,i))) goto err ;
		if (!stralloc_cats(&contents,"\n")) goto err ;
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
	
	err:
		stralloc_free(&contents) ;
		genalloc_deepfree(stralist,&in,stra_free) ;
		genalloc_deepfree(stralist,&gargv,stra_free) ;
		return 0 ;
}

int db_cmd_master(unsigned int verbosity,char const *cmd)
{	
	
	int r ;
	genalloc opts = GENALLOC_ZERO ;
	
	if (!clean_val(&opts,cmd))
	{
		VERBO3 strerr_warnwu2x("clean: ",cmd) ;
		genalloc_deepfree(stralist,&opts,stra_free) ;
		return 0 ;
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

/** action = 0 -> delete, action = 1 -> add, default delete*/
int db_bundle_modif(genalloc *bundle,unsigned int verbosity, char const *src,unsigned int action)
{
	unsigned int i = 0 ;
	size_t salen ;
	stralloc update = STRALLOC_ZERO ;
	char *what ;
	
	if (action) what = "-a" ;
	else what = "-d" ;
	 
	if (!stralloc_cats(&update,what)) return 0 ;
	if (!stralloc_cats(&update," -D ")) return 0 ;
	if (!stralloc_cats(&update,src)) return 0 ;
	if (!stralloc_cats(&update," ")) return 0 ;
	salen = update.len ;
	for (;i < genalloc_len(strakeyval,bundle) ; i++)
	{
		update.len = salen ;
		char *bname = gaikvkey(bundle,i) ;
		char *svname = gaikvval(bundle,i) ;
		
		if (!stralloc_cats(&update,"-B ")) return 0 ;
		if (!stralloc_cats(&update,bname)) return 0 ;
		if (!stralloc_cats(&update," ")) return 0 ;
		if (!stralloc_cats(&update,svname)) return 0 ;
		if (!stralloc_0(&update)) return 0 ;
		if (!db_cmd_master(verbosity,update.s))
		{
			strerr_warnwu2x("update contents of bundle: ",bname) ;
			return 0 ;
		}	
	}
	stralloc_free(&update) ;
	
	return 1 ;
}

int db_bundle_contents(graph_t *g, char const *name, char const *src, unsigned int verbosity, unsigned int action)
{
	unsigned int a, b, c ;
	int r = 0 ;
	genalloc bundle = GENALLOC_ZERO ;
	char const *string = g->string ; 
	for (a = 0 ; a < g->nvertex ; a++)
	{
		char const *bname = string + genalloc_s(vertex_graph_t,&g->vertex)[a].name ;
		
		if (genalloc_s(vertex_graph_t,&g->vertex)[a].type == BUNDLE)
		{
			
			for (b = 0 ; b < genalloc_s(vertex_graph_t,&g->vertex)[a].ndeps; b++)
			{
				char const *depname = string + genalloc_s(vertex_graph_t,&genalloc_s(vertex_graph_t,&g->vertex)[a].dps)[b].name ;
				if(obstr_equal(name,depname))
				{
					for(c = 0; c < genalloc_len(strakeyval,&bundle) ; c++)
					{
						r = obstr_equal(depname,gaikvkey(&bundle,c)) ;
						if (r) break ;
					}
					if (!r)
						if (!strakv_add(&bundle,bname,name)) return 0 ;		
				}
			}
		}
	}
	if (genalloc_len(strakeyval,&bundle))
	{
		if (!db_bundle_modif(&bundle,verbosity,src,action))
			goto err ;
	}
	genalloc_free(strakeyval,&bundle) ;
	
	return 1 ;
	
	err:
		genalloc_free(strakeyval,&bundle) ;
		return 0 ;
}

int db_write_contents(genalloc *ga, char const *bundle,char const *dir)
{
	int r ;
	
	stralloc in = STRALLOC_ZERO ;
	
	size_t dirlen = strlen(dir) ;
	size_t bundlen = strlen(bundle) ;
	
	char dst[dirlen + SS_DB_LEN + SS_SRC_LEN + 1 + bundlen + 1] ;
	memcpy(dst, dir, dirlen) ;
	memcpy(dst + dirlen, SS_DB, SS_DB_LEN) ;
	memcpy(dst + dirlen + SS_DB_LEN, SS_SRC, SS_SRC_LEN) ;
	dst[dirlen + SS_DB_LEN + SS_SRC_LEN] = '/' ;
	memcpy(dst + dirlen + SS_DB_LEN + SS_SRC_LEN + 1, bundle, bundlen) ;
	dst[dirlen + SS_DB_LEN + SS_SRC_LEN + 1 + bundlen] = 0 ;
	
	for (unsigned int i = 0 ; i < genalloc_len(stralist,ga); i++)
	{
		if (!stralloc_cats(&in,gaistr(ga,i))) goto err ;
		if (!stralloc_cats(&in,"\n")) goto err ;
	}
	
	r = file_write_unsafe(dst,SS_CONTENTS,in.s,in.len) ;
	if (!r) 
	{ 
		VERBO3 strerr_warnwu3sys("write: ",dst,"contents") ;
		goto err ;
	}
	
	stralloc_free(&in) ;
	
	return 1 ;
	
	err:
		stralloc_free(&in) ;
		return 0 ;
}
