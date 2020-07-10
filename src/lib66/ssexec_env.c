/* 
 * ssexec_env.c
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
#include <stdlib.h>//getenv

#include <oblibs/obgetopt.h>
#include <oblibs/log.h>
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/types.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/buffer.h>
#include <skalibs/diuint32.h>
#include <skalibs/djbunix.h>
#include <skalibs/unix-transactional.h>//atomic_symlink

#include <66/ssexec.h>
#include <66/utils.h>
#include <66/config.h>
#include <66/parser.h>
#include <66/environ.h>
#include <66/constants.h>
#include <66/resolve.h>

enum tasks_e
{
	T_UNSET = 0 ,
	T_EDIT ,
	T_LIST ,
	T_REPLACE ,
	T_SWITCH ,
	T_IMPORT
} ;

int ssexec_env(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
	int r ;

	stralloc satmp = STRALLOC_ZERO ;
	stralloc sasrc = STRALLOC_ZERO ;
	stralloc saversion = STRALLOC_ZERO ;
	stralloc savar = STRALLOC_ZERO ;
	stralloc salist = STRALLOC_ZERO ;
	ss_resolve_t res = RESOLVE_ZERO ;

	uint8_t todo = T_UNSET ;

	char const *sv = 0, *src = 0, *treename = 0, *editor = 0 ;

	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">Ld:r:es:", &l) ;
			if (opt == -1) break ;
			if (opt == -2) log_die(LOG_EXIT_USER,"options must be set first") ;
			switch (opt)
			{
				case 'L' : 	if (todo != T_UNSET) log_usage(usage_env) ;
							todo = T_LIST ; break ;
				case 'd' :	src = l.arg ; break ;
				case 'r' :	if (!stralloc_cats(&savar,l.arg) ||
							!stralloc_0(&savar)) log_die_nomem("stralloc") ; 
							if (todo != T_UNSET) log_usage(usage_env) ;
							todo = T_REPLACE ; break ;
				case 'e' :	break ;
				case 's' :	if (todo != T_UNSET) log_usage(usage_env) ;
							todo = T_SWITCH ;
							r = version_scan(&saversion,l.arg,SS_CONFIG_VERSION_NDOT) ;
							if (r == -1) log_die_nomem("stralloc") ;
							if (!r) log_die(LOG_EXIT_USER,"invalid version format") ;
							break ;
				default : 	log_usage(usage_env) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	if (argc < 1) log_usage(usage_env) ;
	sv = argv[0] ;

	if (todo == T_UNSET) todo = T_EDIT ;

	treename = !info->opt_tree ? 0 : info->treename.s ;

	if (!src)
	{
		stralloc salink = STRALLOC_ZERO ;
		r = ss_resolve_svtree(&sasrc,sv,treename) ;
		if (r == -1) log_dieu(LOG_EXIT_SYS,"resolve tree source of sv: ",sv) ;
		else if (!r) {
			log_info("no tree exist yet") ;
			goto freed ;
		}
		else if (r > 2) {
			log_die(LOG_EXIT_SYS,sv," is set on different tree -- please use -t options") ;
		}
		else if (r == 1) log_die(LOG_EXIT_SYS,"unknown service: ",sv, !info->opt_tree ? " in current tree: " : " in tree: ", info->treename.s) ;

		if (!ss_resolve_read(&res,sasrc.s,sv))
			log_dieusys(LOG_EXIT_SYS,"read resolve file of: ",sv) ;

		sasrc.len = 0 ;

		if (!auto_stra(&sasrc,res.sa.s + res.srconf,SS_SYM_VERSION)) log_die_nomem("stralloc") ;
		if (sareadlink(&salink,sasrc.s) == -1) log_dieusys(LOG_EXIT_SYS,"readlink: ",sasrc.s) ;
		if (!stralloc_cats(&salink,"/") ||
		!stralloc_cats(&salink,sv) ||
		!stralloc_0(&salink) ||
		!stralloc_copy(&sasrc,&salink)) log_die_nomem("stralloc") ;
		src = sasrc.s ;
		stralloc_free(&salink) ;
	}

	if (!file_readputsa_g(&salist,src)) log_dieusys(LOG_EXIT_SYS,"read: ",src) ;

	satmp.len = 0 ;

	switch(todo)
	{
		case T_LIST:
			if (buffer_putsflush(buffer_1, salist.s) < 0)
				log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
			break ;
		case T_REPLACE:
			if (!env_merge_conf(&satmp,&salist,&savar,2))
				log_dieu(LOG_EXIT_SYS,"merge environment file with: ",savar.s) ;
			if (!openwritenclose_unsafe(src,satmp.s,satmp.len))
				log_dieusys(LOG_EXIT_SYS,"create file: ",src,sv) ;
			break ;
		case T_EDIT:
			editor = getenv("EDITOR") ;
			if (!editor) {
				editor = getenv("SUDO_USER") ;
				if (editor) log_dieu(LOG_EXIT_SYS,"get EDITOR with sudo command -- please try to use the -E sudo option e.g. sudo -E 66-env -e <service>") ;
				else log_dieusys(LOG_EXIT_SYS,"get EDITOR") ;
			}
			char const *const newarg[3] = { editor, src, 0 } ;
			xpathexec_run (newarg[0],newarg,envp) ;
			break ;
		/** Can't happen */
		case T_SWITCH:
			{
				size_t conflen = strlen(res.sa.s + res.srconf) ;  
				char sym[conflen + SS_SYM_VERSION_LEN + 1] ;
				auto_strings(sym,res.sa.s + res.srconf,SS_SYM_VERSION) ;
			
				if (!stralloc_inserts(&saversion,0,"/") ||
				!stralloc_inserts(&saversion,0,res.sa.s + res.srconf) ||
				!stralloc_0(&saversion))
					log_die_nomem("stralloc") ;
				r = scan_mode(saversion.s,S_IFDIR) ;
				if (r == -1 || !r) log_dieusys(LOG_EXIT_USER,"find the versioned directory: ",saversion.s) ; 

				if (!atomic_symlink(saversion.s,sym,"ssexec_env"))
					log_warnu_return(LOG_EXIT_ZERO,"symlink: ",sym," to: ",saversion.s) ;
			}
			break ;
		default: break ;
	}

	freed:
		stralloc_free(&satmp) ;
		stralloc_free(&sasrc) ;
		stralloc_free(&saversion) ;
		stralloc_free(&savar) ;
		stralloc_free(&salist) ;
		ss_resolve_free(&res) ;

	return 0 ;
}
