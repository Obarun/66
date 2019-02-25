/* 
 * tree_cmd_state.c
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

#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include <oblibs/obgetopt.h>
#include <oblibs/error2.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>
#include <oblibs/files.h>
#include <oblibs/stralist.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/types.h>
#include <skalibs/djbunix.h>

#include <66/utils.h>
#include <66/constants.h>

//USAGE "tree_state [ -v verbosity ] [ -a add ] [ -d delete ] [ -s search ] tree"

int tree_state(int argc, char const *const *argv)
{
	int r, fd, err ;
	unsigned int add, del, sch, verbosity ;
	size_t statesize ;
	size_t treelen ;
	size_t statelen ;
	
	char const *tree = NULL ;
	
	stralloc base = STRALLOC_ZERO ;
	stralloc contents = STRALLOC_ZERO ; 
	genalloc in = GENALLOC_ZERO ; //type stralist
	
	verbosity = err = 1 ;
	
	uid_t owner = MYUID ;
	
	add = del = sch =  0 ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, "v:sad", &l) ;
			if (opt == -1) break ;
			if (opt == -2){ strerr_warnw1x("options must be set first") ; return -1 ; }
			switch (opt)
			{
				case 'v' :  if (!uint0_scan(l.arg, &verbosity)) return -1 ;  break ;
				case 'a' : 	add = 1 ; if (del) return -1 ; break ;
				case 'd' : 	del = 1 ;  if (add) return -1 ; break ;
				case 's' : 	sch = 1 ; break ;
				default : 	return -1 ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}

	if (argc < 1) return -1 ;

	tree = *argv ;
	treelen = strlen(tree) ;
	
	if (!set_ownersysdir(&base,owner))
	{
		VERBO3 strerr_warnwu1sys("set owner directory") ;
		return -1 ;
	}

	/** /system/state */
	//base.len-- ;
	
	char state[base.len + SS_SYSTEM_LEN + SS_STATE_LEN + 1] ;
	memcpy(state,base.s,base.len) ;
	memcpy(state + base.len,SS_SYSTEM,SS_SYSTEM_LEN) ;
	memcpy(state + base.len + SS_SYSTEM_LEN, SS_STATE ,SS_STATE_LEN) ;
	statelen = base.len + SS_SYSTEM_LEN + SS_STATE_LEN ;
	state[statelen] = 0 ;

	r = scan_mode(state,S_IFREG) ;
	if (r < 0) { errno = EEXIST ;  err = -1 ; goto out ; }
	if (!r)
	{
		VERBO3 strerr_warnwu2sys("find: ",state) ;
		{ err = -1 ; goto out ; }
	}
	
	statesize = file_get_size(state) ;

	r = openreadfileclose(state,&contents,statesize) ;
	if(!r)
	{
		VERBO3 strerr_warnwu2sys("open: ", state) ;
		{ err = -1 ; goto out ; }
	}
	/** ensure that we have an empty line at the end of the string*/
	if (!stralloc_cats(&contents,"\n")) retstralloc(-1,"tree_registrer") ;
	if (!stralloc_0(&contents)) retstralloc(-1,"tree_registrer") ;
	
	if (!clean_val(&in,contents.s))
	{
		VERBO3 strerr_warnwu2x("clean: ",contents.s) ;
		{ err = -1 ; goto out ; }
	}

	
	if (add)
	{
		if (!stra_cmp(&in,tree))
		{
			fd = open_append(state) ;
			if (fd < 0)
			{
				VERBO3 strerr_warnwu2sys("open: ",state) ;
				{ err = -1 ; goto out ; }
			}
			r = write(fd, tree,treelen);
			r = write(fd, "\n",1);
			if (r < 0)
			{
				VERBO3 strerr_warnwu5sys("write: ",state," with ", tree," as content") ;
				fd_close(fd) ;
				{ err = -1 ; goto out ; }
			}
			fd_close(fd) ;
		}
	}
		
	if (del)
	{
		
		if (stra_cmp(&in,tree))
		{
			if (!stra_remove(&in,tree))
			{
				VERBO3 strerr_warnwu4x("to remove: ",tree," in: ",state) ;
				{ err = -1 ; goto out ; }
			}
			fd = open_trunc(state) ;
			if (fd < 0)
			{
				VERBO3 strerr_warnwu2sys("open_trunc ", state) ;
				{ err = -1 ; goto out ; }
			}/*
			fd = open_append(state) ;
			if (fd < 0)
			{
				VERBO3 strerr_warnwu2sys("open: ",state) ;
				{ err = -1 ; goto out ; }
			}*/
			
			/*** replace it by write_file_unsafe*/
			for (unsigned int i = 0 ; i < genalloc_len(stralist,&in) ; i++)
			{
				r = write(fd, gaistr(&in,i),gaistrlen(&in,i));
				if (r < 0)
				{
					VERBO3 strerr_warnwu5sys("write: ",state," with ", gaistr(&in,i)," as content") ;
					fd_close(fd) ;
					{ err = -1 ; goto out ; }
				}
				r = write(fd, "\n",1);
				if (r < 0)
				{
					VERBO3 strerr_warnwu5sys("write: ",state," with ", gaistr(&in,i)," as content") ;
					fd_close(fd) ;
					{ err = -1 ; goto out ; }
				}
			}
			fd_close(fd) ;
		}
	}
	if (sch)
	{
		if (stra_cmp(&in,tree))
		{
			err = 1 ;
			goto out ;
		}
		else 
		{
			err = 0 ;
			goto out ;
		}
	}
	
	out:
	stralloc_free(&base) ;
	stralloc_free(&contents) ; 
	genalloc_deepfree(stralist,&in,stra_free) ;
	
	return err ;

}
	
int tree_cmd_state(unsigned int verbosity,char const *cmd, char const *tree)
{
	int r ;
	
	genalloc opts = GENALLOC_ZERO ;
	
	if (!clean_val(&opts,cmd))
	{
		VERBO3 strerr_warnwu2x("clean: ",cmd) ;
		return -1 ;
	}
	int newopts = 5 + genalloc_len(stralist,&opts) ;
	char const *newargv[newopts] ;
	unsigned int m = 0 ;
	char fmt[UINT_FMT] ;
	fmt[uint_fmt(fmt, verbosity)] = 0 ;
	
	newargv[m++] = "tree_state" ;
	newargv[m++] = "-v" ;
	newargv[m++] = fmt ;
	
	for (unsigned int i = 0; i < genalloc_len(stralist,&opts); i++)
		newargv[m++] = gaistr(&opts,i) ;
		
	newargv[m++] = tree ;
	newargv[m++] = 0 ;
	
	r = tree_state(newopts,newargv) ;

	genalloc_deepfree(stralist,&opts,stra_free) ;
	
	return r ;
}
