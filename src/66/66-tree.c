/* 
 * 66-tree.c
 * 
 * Copyright (c) 2018-2019-2019 Eric Vidal <eric@obarun.org>
 * 
 * All rights reserved.
 * 
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <66/tree.h>

#include <string.h>
#include <sys/stat.h>
//#include <stdio.h>

#include <oblibs/obgetopt.h>
#include <oblibs/error2.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/buffer.h>
#include <skalibs/bytestr.h>//byte_count

#include <66/config.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/db.h>
#include <66/enum.h>
#include <66/state.h>
#include <66/resolve.h>

#include <s6/s6-supervise.h>
#include <s6-rc/s6rc-servicedir.h>
#include <s6-rc/s6rc-constants.h>

#define USAGE "66-tree [ -h ] [ -v verbosity ] [ -l ] [ -n|R ] [ -a|d ] [ -c ] [ -E|D ] [ -U ] [ -C clone ] tree" 

unsigned int VERBOSITY = 1 ;
static stralloc reslive = STRALLOC_ZERO ;
static char const *cleantree = 0 ;

static inline void info_help (void)
{
  static char const *help =
"66-tree <options> tree\n"
"\n"
"options :\n"
"	-h: print this help\n"
"	-v: increase/decrease verbosity\n"
"	-l: live directory\n"
"	-n: create a new empty tree\n"
"	-a: allow user(s) at tree\n"
"	-d: deny user(s) at tree\n"
"	-c: set tree as default\n"
"	-E: enable the tree\n"
"	-D: disable the tree\n"
"	-R: remove the tree\n"
"	-C: clone the tree\n"
"	-U: unsupervise the tree\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
}

static void cleanup(char const *tree){
	VERBO3 strerr_warnt3x("removing ",tree," ...") ;
	rm_rf(tree) ;
}

static void auto_string(char *string,char const *str,size_t baselen)
{
	size_t slen = strlen(str) ;
	memcpy(string + baselen,str,slen) ;
	string[baselen+slen] = 0 ;
}

static void auto_dir(char const *dst,mode_t mode)
{
	VERBO3 strerr_warnt2x("create directory: ",dst) ;
	if (!dir_create_parent(dst,mode))
	{
		if (cleantree) cleanup(cleantree) ;
		strerr_diefu2sys(111,"create directory: ",dst) ;	
	}
}

static void auto_create(char *string,char const *str,size_t baselen,mode_t mode)
{
	auto_string(string,str,baselen) ;
	auto_dir(string,mode) ;
}

static void auto_check(char *dst,char const *str,size_t baselen,mode_t mode)
{
	auto_string(dst,str,baselen) ;
	if (!scan_mode(dst,mode)) auto_dir(dst,0755) ;
}

static void inline auto_stralloc(stralloc *sa,char const *str)
{
	if (!stralloc_cats(sa,str)) strerr_diefu1sys(111,"append stralloc") ;
}

static void inline auto_stralloc0(stralloc *sa)
{
	if (!stralloc_0(sa)) strerr_diefu1sys(111,"append stralloc") ;
	sa->len-- ;
}

static void inline auto_stralloc_0(stralloc *sa,char const *str)
{
	auto_stralloc(sa,str) ;
	auto_stralloc0(sa) ;
}

int sanitize_tree(stralloc *dstree, char const *base, char const *tree,uid_t owner)
{
	
	ssize_t r ;
	size_t baselen = strlen(base) ;
	char dst[baselen + SS_SYSTEM_LEN + 1] ;
	auto_string(dst,base,0) ;
			
	/** base is /var/lib/66 or $HOME/.66*/
	/** this verification is made in case of 
	 * first use of 66-*** tools */
	r = scan_mode(dst,S_IFDIR) ;
	if (r < 0) strerr_diefu2x(111,"invalid directory: ",dst) ;
	
	if(!r)
	{
		VERBO3 strerr_warnt3x("create directory: ",dst,SS_SYSTEM) ;
		auto_create(dst,SS_SYSTEM,baselen,0755) ;
		/** create extra directory for service part */
		if (!owner)
		{
			auto_dir(SS_LOGGER_SYSDIR,0755) ;
			auto_dir(SS_SERVICE_SYSDIR,0755) ;
			auto_dir(SS_SERVICE_ADMDIR,0755) ;
			auto_dir(SS_SERVICE_ADMCONFDIR,0755) ;	
		}
		else
		{
			size_t extralen ;
			stralloc extra = STRALLOC_ZERO ;
			if (!set_ownerhome(&extra,owner))
				strerr_diefu1sys(111,"set home directory") ;
			
			extralen = extra.len ;
			auto_stralloc_0(&extra,SS_LOGGER_USERDIR) ;
			auto_dir(extra.s,0755) ;
			extra.len = extralen ;
			auto_stralloc_0(&extra,SS_SERVICE_USERDIR) ;
			auto_dir(extra.s,0755) ;
			extra.len = extralen ;
			auto_stralloc_0(&extra,SS_SERVICE_USERCONFDIR) ;
			auto_dir(extra.s,0755) ;
			stralloc_free(&extra) ;
		}
	}
	auto_check(dst,SS_TREE_CURRENT,baselen,0755) ;
	auto_string(dst,SS_SYSTEM,baselen) ;
	auto_check(dst,SS_BACKUP,baselen + SS_SYSTEM_LEN,0755) ;
	auto_string(dst,SS_STATE,baselen + SS_SYSTEM_LEN) ;
	if (!scan_mode(dst,S_IFREG))
	{
		auto_string(dst,SS_SYSTEM,baselen) ;
		if(!file_create_empty(dst,SS_STATE + 1,0644))
			strerr_diefu3sys(111,"create ",dst,SS_STATE) ;
	}
	auto_string(dst,"/",baselen + SS_SYSTEM_LEN) ;
	auto_string(dst,tree,baselen + SS_SYSTEM_LEN + 1) ;
	r = scan_mode(dst,S_IFDIR) ;
	if (r == -1) strerr_dief2x(111,"invalid directory: ",dst) ;
	/** we have one, keep it*/
	if (!stralloc_cats(dstree,dst)) retstralloc(0,"sanitize_tree") ;
	if (!stralloc_0(dstree)) retstralloc(0,"sanitize_tree") ;
	
	if (!r) return 0 ;
	
	return 1 ;
}

void create_tree(char const *tree,char const *treename)
{
	size_t newlen = 0 ;
	size_t treelen = strlen(tree) ;
		
	char dst[treelen + SS_DB_LEN + SS_SRC_LEN + 13 + 1] ;
	ss_resolve_t res = RESOLVE_ZERO ;
	ss_resolve_init(&res) ;
	
	memcpy(dst, tree, treelen) ;
	newlen = treelen ;
	dst[newlen] = 0 ;
	
	res.name = ss_resolve_add_string(&res,SS_MASTER+1) ;
	res.description = ss_resolve_add_string(&res,"inner bundle - do not use it") ;
	res.tree = ss_resolve_add_string(&res,dst) ;
	res.treename = ss_resolve_add_string(&res,treename) ;
	res.type = BUNDLE ;
	res.disen = 1 ;
	
	auto_create(dst,SS_SVDIRS,newlen,0755) ;
	auto_create(dst,SS_RULES,newlen,0755) ;
	auto_string(dst,SS_SVDIRS,newlen) ;
	newlen = newlen + SS_SVDIRS_LEN ;
	auto_create(dst,SS_DB,newlen,0755) ;
	auto_create(dst,SS_SVC,newlen,0755) ;
	auto_create(dst,SS_RESOLVE,newlen,0755) ;
	dst[newlen] = 0 ;
	VERBO3 strerr_warnt1x("write resolve file of inner bundle") ;
	if (!ss_resolve_write(&res,dst,SS_MASTER+1))
	{
		ss_resolve_free(&res) ;
		cleanup(cleantree) ;
		strerr_diefu1sys(111,"write resolve file of inner bundle") ;
	}
	ss_resolve_free(&res) ;
	
	char sym[newlen + 1 + SS_SYM_SVC_LEN + 1] ;
	char dstsym[newlen + SS_SVC_LEN + 1] ;

	auto_string(sym,dst,0) ;
	auto_string(sym,"/",newlen) ;
	auto_string(sym,SS_SYM_SVC,newlen + 1) ;

	auto_string(dstsym,dst,0) ;
	auto_string(dstsym,SS_SVC,newlen) ;
	VERBO3 strerr_warnt4x("point symlink: ",sym," to ",dstsym) ;
	if (symlink(dstsym,sym) < 0)
	{
		cleanup(cleantree) ;
		strerr_diefu2sys(111,"symlink: ", sym) ;
	}
	
	auto_string(sym,SS_SYM_DB,newlen + 1) ;
	auto_string(dstsym,SS_DB,newlen) ;
	VERBO3 strerr_warnt4x("point symlink: ",sym," to ",dstsym) ;
	if (symlink(dstsym,sym) < 0)
	{
		cleanup(cleantree) ;
		strerr_diefu2sys(111,"symlink: ", sym) ;
	}

	auto_string(dst,SS_DB,newlen) ;
	newlen = newlen + SS_DB_LEN ;
	auto_create(dst,SS_SRC,newlen,0755) ;
	auto_string(dst,SS_SRC,newlen) ;
	newlen = newlen + SS_SRC_LEN ;
	auto_create(dst,SS_MASTER,newlen,0755) ;
	auto_string(dst,SS_MASTER,newlen) ;
	newlen = newlen + SS_MASTER_LEN ;
	VERBO3 strerr_warnt3x("create file: ",dst,"/contents") ;
	if (!file_create_empty(dst,"contents",0644))
	{
		cleanup(cleantree) ;
		strerr_diefu3sys(111,"create ",dst,"/contents") ;
	}

	auto_string(dst,"/type",newlen) ;
	VERBO3 strerr_warnt2x("create file: ",dst) ;
	if(!openwritenclose_unsafe(dst,"bundle\n",7))
	{
		cleanup(cleantree) ;
		strerr_diefu2sys(111,"write ",dst) ;
	}
	
}

void create_backupdir(char const *base, char const *treename)
{
	int r ;
	
	size_t baselen = strlen(base) - 1 ;//remove the last slash
	size_t treenamelen = strlen(treename) ;

	char treetmp[baselen + 1 + SS_SYSTEM_LEN + SS_BACKUP_LEN + treenamelen + 1 + 1] ;
	
	auto_string(treetmp,base,0) ;
	auto_string(treetmp,"/",baselen) ;
	auto_string(treetmp,SS_SYSTEM,baselen + 1) ;
	auto_string(treetmp,SS_BACKUP,baselen + 1 + SS_SYSTEM_LEN) ;
	auto_string(treetmp,"/",baselen + 1 + SS_SYSTEM_LEN + SS_BACKUP_LEN) ;
	auto_string(treetmp,treename,baselen + 1 + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1) ;

	r = scan_mode(treetmp,S_IFDIR) ;
	if (r || (r == -1))
	{
		VERBO3 strerr_warnt2x("remove existing backup: ",treetmp) ;
		if (rm_rf(treetmp) < 0)
		{
			cleanup(cleantree) ;
			strerr_diefu2sys(111,"remove: ",treetmp) ;
		}
	}
	if (!r)	auto_dir(treetmp,0755) ;
}

int set_rules(char const *tree,uid_t *uids, size_t uidn,unsigned int what) 
{
	int r ;
	size_t treelen = strlen(tree) ;
	
	char pack[256] ;
	char tmp[treelen + SS_RULES_LEN + 1] ;
	
	auto_string(tmp,tree,0) ;
	auto_string(tmp,SS_RULES,treelen) ;

	if (!uidn && what) 
	{
		uids[0] = 1 ;
		uids[1] = MYUID ;
		uidn++ ;
	}
	if (what) //allow
	{
		for (size_t i = 0 ; i < uidn ; i++)
		{
			uint32_pack(pack,uids[i+1]) ;
			pack[uint_fmt(pack,uids[i+1])] = 0 ;
			VERBO3 strerr_warnt4x("create file : ",pack," at ",tmp) ;
			if(!file_create_empty(tmp,pack,0644) && errno != EEXIST)
			{
				VERBO3 strerr_warnwu4sys("create file : ",pack," at ",tmp) ;
				return 0 ;
			}
			VERBO3 strerr_warnt4x("user: ",pack," is allowed for tree: ",tree) ;
		}
		return 1 ;
	}
	//else deny
	for (size_t i = 0 ; i < uidn ; i++)
	{
		if (MYUID == uids[i+1]) continue ;
		uint32_pack(pack,uids[i+1]) ;
		pack[uint_fmt(pack,uids[i+1])] = 0 ;
		r = dir_search(tmp,pack,S_IFREG) ;
		if (r)
		{
			memcpy(tmp + treelen + SS_RULES_LEN,"/",1) ;
			memcpy(tmp + treelen + SS_RULES_LEN + 1,pack,uint_fmt(pack,uids[i+1])) ;
			tmp[treelen + SS_RULES_LEN + 1 + uint_fmt(pack,uids[i + 1])] = 0 ;
			VERBO3 strerr_warnt2x("unlink: ",tmp) ;
			r = unlink(tmp) ;
			if (r == -1) return 0 ;
		}
		VERBO3 strerr_warnt4x("user: ",pack," is denied for tree: ",tree) ;
	}
	return 1 ;
}

void tree_unsupervise(stralloc *live, char const *tree, char const *treename,uid_t owner,char const *const *envp)
{
	int r, wstat ;
	pid_t pid ;
	size_t treenamelen = strlen(treename) ;
	size_t newlen ;
	
	stralloc scandir = STRALLOC_ZERO ;
	stralloc livestate = STRALLOC_ZERO ;
	stralloc livetree = STRALLOC_ZERO ;
	stralloc list = STRALLOC_ZERO ;
	stralloc realsym = STRALLOC_ZERO ;
	
	/** set what we need */
	char prefix[treenamelen + 2] ;
	auto_string(prefix,treename,0) ;
	auto_string(prefix,"-",treenamelen) ;

	auto_stralloc(&scandir,live->s) ;
	r = set_livescan(&scandir,owner) ;
	if (!r) strerr_diefu1sys(111,"set livescan directory") ;
		
	auto_stralloc(&livestate,live->s) ;
	r = set_livestate(&livestate,owner) ;
	if (!r) strerr_diefu1sys(111,"set livestate directory") ;
	
	auto_stralloc(&livetree,live->s) ;
	r = set_livetree(&livetree,owner) ;
	if (!r) strerr_diefu1sys(111,"set livetree directory") ;
	auto_stralloc(&livetree,"/") ;
	newlen = livetree.len ;
	auto_stralloc0(&livetree) ;
	
	auto_stralloc(&livestate,"/") ;
	auto_stralloc(&livestate,treename) ;
	auto_stralloc0(&livestate) ;
	/** works begin */
	if (scan_mode(livestate.s,S_IFDIR)) 
		if (!sastr_dir_get(&list,livestate.s,SS_LIVETREE_INIT,S_IFREG)) strerr_diefu2x(111,"get service list at: ",livestate.s) ;
	
	if (!list.len) 
	{ 
		VERBO1 strerr_warni2x("Not supervised: ",livestate.s) ;
		return ;
	}
	
	{ 
		size_t i = 0, len = sastr_len(&list) ;
		char const *newargv[9 + len + 1] ;
		unsigned int m = 0 ;
		char fmt[UINT_FMT] ;
		fmt[uint_fmt(fmt, VERBOSITY)] = 0 ;
				
		newargv[m++] = SS_BINPREFIX "66-stop" ;
		newargv[m++] = "-v" ;
		newargv[m++] = fmt ;
		newargv[m++] = "-l" ;
		newargv[m++] = live->s ;
		newargv[m++] = "-t" ;
		newargv[m++] = treename ;
		newargv[m++] = "-u" ;
		
		len = list.len ;
		for (;i < len; i += strlen(list.s + i) + 1)
			newargv[m++] = list.s+i ;
		
		newargv[m++] = 0 ;
						
		pid = child_spawn0(newargv[0],newargv,envp) ;
		if (waitpid_nointr(pid,&wstat, 0) < 0)
			VERBO3 strerr_warnwu2sys("wait for ",newargv[0]) ;
		/** we don't want to die here, we try we can do to stop correctly
		 * service list, in any case we want to unsupervise */
		if (wstat) VERBO3 strerr_warnwu1sys("stop services") ;
	}	
	
	if (db_find_compiled_state(livetree.s,treename) >=0)
	{	
		list.len = 0 ;
		livetree.len = newlen ;
		auto_stralloc(&livetree,treename) ;
		auto_stralloc(&livetree,SS_SVDIRS) ;
		auto_stralloc0(&livetree) ;
		if (!sastr_dir_get(&list,livetree.s,"",S_IFDIR)) strerr_diefu2x(111,"get service list at: ",livetree.s) ;
		livetree.len = newlen ;
		auto_stralloc(&livetree,treename) ;
		auto_stralloc0(&livetree) ;
		
		size_t i = 0, len = list.len ;
		for (;i < len; i += strlen(list.s + i) + 1)
			s6rc_servicedir_unsupervise(livetree.s,prefix,list.s + i,0) ;
			
		r = sarealpath(&realsym,livetree.s) ;
		if (r < 0 ) strerr_diefu2sys(111,"find realpath of: ",livetree.s) ; 
		auto_stralloc0(&realsym) ;
		if (rm_rf(realsym.s) < 0) strerr_diefu2sys(111,"remove ", realsym.s) ;
		if (rm_rf(livetree.s) < 0) strerr_diefu2sys(111,"remove ", livetree.s) ;
		livetree.len = newlen ;
		auto_stralloc(&livetree,treename) ;
		auto_stralloc0(&livetree) ;
		/** remove the symlink itself */
		unlink_void(livetree.s) ;
	}
		
	if (scandir_send_signal(scandir.s,"an") <= 0) strerr_diefu2sys(111,"reload scandir: ",scandir.s) ;
	/** remove /run/66/state/uid/treename directory */
	VERBO2 strerr_warni3x("delete ",livestate.s," ..." ) ;
	if (rm_rf(livestate.s) < 0) strerr_diefu2sys(111,"delete ",livestate.s) ;
	
	VERBO1 strerr_warni2x("Unsupervised successfully tree: ",treename) ;
	
	stralloc_free(&scandir) ; 
	stralloc_free(&livestate) ; 
	stralloc_free(&livetree) ; 
	stralloc_free(&list) ;
	stralloc_free(&realsym) ;  
}

int main(int argc, char const *const *argv,char const *const *envp)
{
	int r, current, create, allow, deny, enable, disable, remove, snap, unsupervise ;
	
	uid_t owner ;
	
	size_t auidn = 0 ;
	uid_t auids[256] ;
	
	size_t duidn = 0 ;
	uid_t duids[256] ;
		
	char const *tree = NULL ;
	
	stralloc base = STRALLOC_ZERO ;
	stralloc dstree = STRALLOC_ZERO ;
	stralloc clone = STRALLOC_ZERO ;
	stralloc live = STRALLOC_ZERO ;
	
	current = create = allow = deny = enable = disable = remove = snap = unsupervise = 0 ;
	
	PROG = "66-tree" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			
			int opt = getopt_args(argc,argv, "hv:l:na:d:cEDRC:U", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'h' : info_help(); return 0 ;
				case 'v' : if (!uint0_scan(l.arg, &VERBOSITY)) exitusage(USAGE) ; break ;
				case 'l' : 	if (!stralloc_cats(&live,l.arg)) retstralloc(111,"main") ;
							if (!stralloc_0(&live)) retstralloc(111,"main") ;
							break ;
				case 'n' : create = 1 ; break ;
				case 'a' : if (!scan_uidlist_wdelim(l.arg,auids,',')) exitusage(USAGE) ; 
						   auidn = auids[0] ;
						   allow = 1 ;
						   break ;
				case 'd' : if (!scan_uidlist_wdelim(l.arg,duids,',')) exitusage(USAGE) ; 
						   duidn = duids[0] ;
						   deny = 1 ;
						   break ;
				case 'c' : current = 1 ; break ;
				case 'E' : enable = 1 ; if (disable) exitusage(USAGE) ; break ;
				case 'D' : disable = 1 ; if (enable) exitusage (USAGE) ; break ;
				case 'R' : remove = 1 ; if (create) exitusage(USAGE) ; break ;
				case 'C' : if (!stralloc_cats(&clone,l.arg)) retstralloc(111,"main") ;
						   if (!stralloc_0(&clone)) retstralloc(111,"main") ;
						   snap = 1 ;
						   break ;
				case 'U' : unsupervise = 1 ; if (create) exitusage(USAGE) ; break ;
				default : exitusage(USAGE) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc != 1) exitusage(USAGE) ;
	
	tree = argv[0] ;
	owner = MYUID ;
		
	if (!set_ownersysdir(&base, owner)) strerr_diefu1sys(111, "set owner directory") ;
	
	r = set_livedir(&live) ;
	if (!r) exitstralloc("main:livedir") ;
	if(r < 0) strerr_dief3x(111,"livedir: ",live.s," must be an absolute path") ;
	
	VERBO2 strerr_warni3x("sanitize ",tree," ..." ) ;
	r = sanitize_tree(&dstree,base.s,tree,owner) ;
	
	if(!r && create)
	{
		/** set cleanup */
		cleantree = dstree.s ;
		VERBO2 strerr_warni3x("creating ",dstree.s," ..." ) ;
		create_tree(dstree.s,tree) ;
		
		VERBO2 strerr_warni3x("creating backup directory for ", tree," ...") ;
		create_backupdir(base.s,tree) ;
		/** unset cleanup */
		cleantree = 0 ;
		
		VERBO2 strerr_warni3x("set permissions rules for ",dstree.s," ..." ) ;
		if (!set_rules(dstree.s,auids,auidn,1))//1 allow
		{
			cleanup(dstree.s) ;
			strerr_diefu1x(111,"set permissions access") ;
		}
		
		size_t dblen = dstree.len - 1 ;
		char newdb[dblen + SS_SVDIRS_LEN + 1] ;
		auto_string(newdb,dstree.s,0) ;
		auto_string(newdb,SS_SVDIRS,dblen) ;
	
		VERBO2 strerr_warni5x("compile ",newdb,"/db/",tree," ..." ) ;
		if (!db_compile(newdb,dstree.s,tree,envp))
		{
			cleanup(dstree.s) ;
			strerr_diefu4x(111,"compile ",newdb,"/db/",tree) ;
		}
		r = 1 ;
		create = 0 ;
		VERBO1 strerr_warni2x("Created successfully tree: ",tree) ;
		
	}
	
	if ((!r && !create) || (!r && enable)) strerr_diefu2x(111,"find tree: ",dstree.s) ;
	if (r && create) strerr_diefu3x(110,"create ",dstree.s,": already exist") ;

	if (enable)
	{
		VERBO2 strerr_warni3x("enable ",dstree.s," ..." ) ;
		r  = tree_cmd_state(VERBOSITY,"-a",tree) ;
		if (!r) strerr_diefu6x(111,"add: ",dstree.s," at: ",base.s,SS_SYSTEM,SS_STATE) ;
		else if (r == 1) 
		{
			VERBO1 strerr_warni2x("Enabled successfully tree: ",tree) ;
		}
		else VERBO1 strerr_warni2x("Already enabled tree: ",tree) ;
	}
	
	if (disable)
	{
		VERBO2 strerr_warni3x("disable ",dstree.s," ..." ) ;
		r  = tree_cmd_state(VERBOSITY,"-d",tree) ;
		if (!r) strerr_diefu6x(111,"remove: ",dstree.s," at: ",base.s,SS_SYSTEM,SS_STATE) ;
		else if (r == 1)
		{
			VERBO1 strerr_warni2x("Disabled successfully tree: ",tree) ;
		}
		else VERBO1 strerr_warni2x("Already disabled tree: ",tree) ;
	}
	
	if (auidn)
	{
		VERBO2 strerr_warni3x("set allowed user for tree: ",dstree.s," ..." ) ;
		if (!set_rules(dstree.s,auids,auidn,1))	strerr_diefu1x(111,"set permissions access") ;
	}	
	if (duidn)
	{
		VERBO2 strerr_warni3x("set denied user for tree: ",dstree.s," ..." ) ;
		if (!set_rules(dstree.s,duids,duidn,0))	strerr_diefu1x(111,"set permissions access") ;
	}
	if(current)
	{
		VERBO2 strerr_warni3x("make ",dstree.s," as default ..." ) ;
		if (!tree_switch_current(base.s,tree)) strerr_diefu3sys(111,"set ",dstree.s," as default") ;
		VERBO1 strerr_warni3x("Set successfully ",tree," as default") ;
	}
	if (unsupervise) tree_unsupervise(&live,dstree.s,tree,owner,envp) ;
	
	if (remove)
	{
		VERBO2 strerr_warni3x("delete ",dstree.s," ..." ) ;
		if (rm_rf(dstree.s) < 0) strerr_diefu2sys(111,"delete ", dstree.s) ;
				
		size_t treelen = strlen(tree) ;
		size_t baselen = base.len ;
		char treetmp[baselen + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1 + treelen  + 1] ;
		auto_string(treetmp,base.s,0) ;
		auto_string(treetmp,SS_SYSTEM,baselen) ;
		auto_string(treetmp,SS_BACKUP,baselen + SS_SYSTEM_LEN) ;
		auto_string(treetmp,"/",baselen + SS_SYSTEM_LEN + SS_BACKUP_LEN) ;
		auto_string(treetmp,tree,baselen + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1) ;
	
		r = scan_mode(treetmp,S_IFDIR) ;
		if (r || (r < 0))
		{
			VERBO2 strerr_warni3x("delete backup of tree ",treetmp," ...") ;
			if (rm_rf(treetmp) < 0)	strerr_diefu2sys(111,"delete: ",treetmp) ;
		}
		VERBO2 strerr_warni6x("disable: ",dstree.s," at: ",base.s,SS_SYSTEM,SS_STATE) ;
		if (!tree_cmd_state(VERBOSITY,"-d",tree))
			strerr_diefu6x(111,"disable: ",dstree.s," at: ",base.s,SS_SYSTEM,SS_STATE) ;
		
		VERBO1 strerr_warni2x("Deleted successfully: ",tree) ;
	}
	
	if (snap)
	{
		char tmp[dstree.len + 1] ;
		auto_string(tmp,dstree.s,0) ;
	
		r  = get_rlen_until(dstree.s,'/',dstree.len) ;
		dstree.len = r + 1;
		
		auto_stralloc_0(&dstree,clone.s) ;		
		r = scan_mode(dstree.s,S_IFDIR) ;
		if ((r < 0) || r) strerr_dief2x(111,dstree.s,": already exist") ; 
		VERBO2 strerr_warni5x("clone ",dstree.s," as ",tmp," ..." ) ;
		if (!hiercopy(tmp,dstree.s)) strerr_diefu4sys(111,"copy: ",dstree.s," at: ",clone.s) ;
		VERBO1 strerr_warni4x("Cloned successfully: ",tree," to ",clone.s) ;
	}
	stralloc_free(&reslive) ;
	stralloc_free(&base) ;
	stralloc_free(&dstree) ;
	stralloc_free(&clone) ;

	
	return 0 ;
}
