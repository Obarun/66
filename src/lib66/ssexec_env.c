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
#include <oblibs/sastr.h>

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
	T_REPLACE
} ;

#define MAXOPTS 3
#define checkopts(n) if (n >= MAXOPTS) log_die(LOG_EXIT_USER, "too many versions number")
#define DELIM ','

static void check_version(char *result, char const *conf_dir, char const *version)
{
	int r ;
	stralloc sa = STRALLOC_ZERO ;

	r = version_scan(&sa,version,SS_CONFIG_VERSION_NDOT) ;
	if (r == -1) log_die_nomem("stralloc") ;
	if (!r) log_die(LOG_EXIT_USER,"invalid version format") ;

	auto_strings(result,conf_dir,"/",sa.s) ;

	r = scan_mode(result,S_IFDIR) ;
	if (r == -1 || !r) log_dieusys(LOG_EXIT_USER,"find the versioned directory: ",result) ;

	stralloc_free(&sa) ;
}

static void do_import(char const *svname, char const *conf_dir, char const *version)
{
	struct stat st ;
  	size_t pos = 0, conflen = strlen(conf_dir) ;
	stralloc salist = STRALLOC_ZERO ;
	stralloc sa = STRALLOC_ZERO ;
	
	char *src_version = 0 ;
	char *dst_version = 0 ;

	if (!sastr_clean_string_wdelim(&sa,version,DELIM)) log_dieu(LOG_EXIT_SYS,"clean string: ",version) ;

	unsigned int n = sastr_len(&sa) ;
	checkopts(n) ;
	
	for (;pos < sa.len; pos += strlen(sa.s + pos) + 1)
	{
		if (!pos) src_version = sa.s + pos ;
		else dst_version = sa.s + pos ;
	}
	
	char src[conflen + 1 + strlen(src_version) + 1] ;
	char dst[conflen + 1 + strlen(dst_version) + 1] ;
	
	check_version(src,conf_dir,src_version) ;
	check_version(dst,conf_dir,dst_version) ;
	
	size_t srclen = strlen(src), dstlen = strlen(dst) ;
	
	if (!sastr_dir_get(&salist,src,svname,S_IFREG))
		log_dieu(LOG_EXIT_SYS,"get configuration file from directory: ",src) ;
	
	for (pos = 0 ; pos < salist.len; pos += strlen(salist.s + pos) + 1)
	{
		char *name = salist.s + pos ;
		size_t namelen = strlen(name) ;
		
		char s[srclen + 1 + namelen + 1] ;
		auto_strings(s,src,"/",name) ;
		
		char d[dstlen + 1 + namelen + 1] ;
		auto_strings(d,dst,"/",name) ;
		
		if (lstat(s, &st) < 0) log_dieusys(LOG_EXIT_SYS,"stat: ",s) ;
		log_info("copy: ",s," to: ",d) ;
		if (!filecopy_unsafe(s, d, st.st_mode)) log_dieusys(LOG_EXIT_SYS,"copy: ", s," to: ",d) ;
	}

	stralloc_free(&sa) ;
	stralloc_free(&salist) ;
}

int ssexec_env(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
	int r ;

	stralloc satmp = STRALLOC_ZERO ;
	stralloc sasrc = STRALLOC_ZERO ;
	stralloc saversion = STRALLOC_ZERO ;
	stralloc savar = STRALLOC_ZERO ;
	stralloc salist = STRALLOC_ZERO ;
	ss_resolve_t res = RESOLVE_ZERO ;

	uint8_t todo = T_UNSET, sym_switch = 0  ;

	char const *sv = 0, *src = 0, *treename = 0, *editor = 0, *import = 0 ;

	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">Ld:r:es:i:", &l) ;
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
				case 'e' :	if (todo != T_UNSET) log_usage(usage_env) ;
							todo = T_EDIT ; break ;
				case 's' :	sym_switch = 1 ;
							r = version_scan(&saversion,l.arg,SS_CONFIG_VERSION_NDOT) ;
							if (r == -1) log_die_nomem("stralloc") ;
							if (!r) log_die(LOG_EXIT_USER,"invalid version format") ;
							break ;
				case 'i' :	import = l.arg ; break ;
				default : 	log_usage(usage_env) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	if (argc < 1) log_usage(usage_env) ;
	sv = argv[0] ;

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
	
	if (sym_switch)
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

	if (import)
		do_import(sv,res.sa.s + res.srconf,import) ;
	
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
		/** Can't happens */
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
