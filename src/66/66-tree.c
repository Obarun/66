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

#include <oblibs/obgetopt.h>
#include <oblibs/error2.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>
#include <oblibs/files.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/buffer.h>

#include <66/config.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/db.h>
#include <66/enum.h>

#include <s6/s6-supervise.h>
#include <s6-rc/s6rc-servicedir.h>
#include <s6-rc/s6rc-constants.h>

#include <stdio.h>

#define USAGE "66-tree [ -h ] [ -v verbosity ] [ -n|R ] [ -a|d ] [ -c ] [ -E|D ] [ -U ] [ -C clone ] tree" 

unsigned int VERBOSITY = 1 ;
static stralloc reslive = STRALLOC_ZERO ;

static inline void info_help (void)
{
  static char const *help =
"66-tree <options> tree\n"
"\n"
"options :\n"
"	-h: print this help\n"
"	-v: increase/decrease verbosity\n"
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

void cleanup(char const *tree){
	VERBO3 strerr_warnt3x("removing ",tree," ...") ;
	rm_rf(tree) ;
}

int sanitize_extra(char const *dst)
{
	int r ;
	size_t dstlen, slash ;
	dstlen = strlen(dst) - 1 ;//-1 remove last slash
	char parentdir[dstlen + 1] ;
	memcpy(parentdir,dst,dstlen) ;
	slash = get_rlen_until(dst,'/',dstlen) ;
	parentdir[slash] = '\0' ;
	r = dir_create_under(parentdir,dst+slash+1,0755) ;
	
	return r ;
}

int sanitize_tree(stralloc *dstree, char const *base, char const *tree,uid_t owner)
{
	
	ssize_t r ;
	size_t baselen = strlen(base) ;
	char dst[baselen + SS_SYSTEM_LEN + 1] ;
	
	memcpy(dst,base,baselen) ;
	dst[baselen] = 0 ;
		
	/** base is /var/lib/66 or $HOME/.66*/
	/** this verification is made in case of 
	 * first use of 66-*** tools */
	r = scan_mode(dst,S_IFDIR) ;
	if (r < 0)
	{
		VERBO3 strerr_warnwu2x("invalid directorey: ",dst) ;
		return -1 ;
	}
	if(!r)
	{
		VERBO3 strerr_warnt3x("create directory: ",dst,SS_SYSTEM) ;
		r = dir_create_under(dst,SS_SYSTEM,0755) ;
		if (!r){
			VERBO3 strerr_warnwu3sys("create ",dst,SS_SYSTEM) ;
			return -1 ;
		}
		/** create extra directory for service part */
		if (!owner)
		{
			if (sanitize_extra(SS_LOGGER_SYSDIR) < 0)
			{ VERBO3 strerr_warnwu2sys("create directory: ",SS_LOGGER_SYSDIR) ; return -1 ; }
			if (sanitize_extra(SS_SERVICE_PACKDIR) < 0)
			{ VERBO3 strerr_warnwu2sys("create directory: ",SS_LOGGER_SYSDIR) ; return -1 ; }
			if (sanitize_extra(SS_SERVICE_SYSDIR) < 0)
			{ VERBO3 strerr_warnwu2sys("create directory: ",SS_LOGGER_SYSDIR) ; return -1 ; }
			if (sanitize_extra(SS_SERVICE_SYSCONFDIR) < 0)
			{ VERBO3 strerr_warnwu2sys("create directory: ",SS_LOGGER_SYSDIR) ; return -1 ; }
		}
		else
		{
			size_t extralen ;
			stralloc extra = STRALLOC_ZERO ;
			if (!set_ownerhome(&extra,owner))
			{
				VERBO3 strerr_warnwu1sys("set home directory") ;
				return -1 ;
			}
			extralen = extra.len ;
			if (!stralloc_cats(&extra,SS_LOGGER_USERDIR)) retstralloc(0,"sanitize_tree") ;
			if (!stralloc_0(&extra)) retstralloc(0,"sanitize_tree") ;
			if (sanitize_extra(extra.s) < 0)
			{ VERBO3 strerr_warnwu2sys("create directory: ",extra.s) ; return -1 ; }
			extra.len = extralen ;
			if (!stralloc_cats(&extra,SS_SERVICE_USERDIR)) retstralloc(0,"sanitize_tree") ;
			if (!stralloc_0(&extra)) retstralloc(0,"sanitize_tree") ;
			if (sanitize_extra(extra.s) < 0)
			{ VERBO3 strerr_warnwu2sys("create directory: ",extra.s) ; return -1 ; }
			extra.len = extralen ;
			if (!stralloc_cats(&extra,SS_SERVICE_USERCONFDIR)) retstralloc(0,"sanitize_tree") ;
			if (!stralloc_0(&extra)) retstralloc(0,"sanitize_tree") ;
			if (sanitize_extra(extra.s) < 0)
			{ VERBO3 strerr_warnwu2sys("create directory: ",extra.s) ; return -1 ; }
			stralloc_free(&extra) ;
		}
	}
	if (!dir_search(dst,SS_TREE_CURRENT,S_IFDIR))
	{
		VERBO3 strerr_warnt3x("create directory: ",dst,SS_TREE_CURRENT) ;
		r = dir_create_under(dst,SS_TREE_CURRENT,0755) ;
		if (!r){
			VERBO3 strerr_warnwu3sys("create ",dst,SS_TREE_CURRENT) ;
			return -1 ;
		}
	}
	memcpy(dst + baselen, SS_SYSTEM, SS_SYSTEM_LEN) ;
	dst[baselen + SS_SYSTEM_LEN] = 0 ;
	if (!dir_search(dst,SS_BACKUP + 1,S_IFDIR))
	{	
		VERBO3 strerr_warnt3x("create directory: ",dst,SS_BACKUP) ;
		if(!dir_create_under(dst,SS_BACKUP + 1,0755))
		{
			VERBO3 strerr_warnwu3sys("create ",dst,SS_BACKUP) ;
			return -1 ;
		}
	}
	if (!dir_search(dst,SS_STATE + 1,S_IFREG))
	{
		VERBO3 strerr_warnt3x("create file: ",dst,SS_STATE) ;
		if(!file_create_empty(dst,SS_STATE + 1,0644))
		{
			VERBO3 strerr_warnwu3sys("create ",dst,SS_STATE) ;
			return -1 ;
		}
	}
	
	r = dir_search(dst,tree,S_IFDIR) ;
	if (r < 0){
		VERBO3 strerr_warnw3x(dst,tree," is not a directory") ;
		return -1 ;
	}
	
	/** we have one, keep it*/
	if (!stralloc_cats(dstree,dst)) retstralloc(0,"sanitize_tree") ;
	if (!stralloc_cats(dstree,"/")) retstralloc(0,"sanitize_tree") ;
	if (!stralloc_cats(dstree,tree)) retstralloc(0,"sanitize_tree") ;
	if (!stralloc_0(dstree)) retstralloc(0,"sanitize_tree") ;
	
	if (!r) return 0 ;
	
	return 1 ;
}

int create_tree(char const *tree,char const *treename)
{
	size_t newlen = 0 ;
	size_t treelen = strlen(tree) ;
	
	char dst[treelen + SS_DB_LEN + SS_SRC_LEN + 13 + 1] ;
	ss_resolve_t res = RESOLVE_ZERO ;
	ss_resolve_init(&res) ;
	
	memcpy(dst, tree, treelen) ;
	newlen = treelen ;
	dst[newlen] = 0 ;
	
	res.name = ss_resolve_add_string(&res,"Master") ;
	res.description = ss_resolve_add_string(&res,"inner bundle - do not use it") ;
	res.tree = ss_resolve_add_string(&res,dst) ;
	res.treename = ss_resolve_add_string(&res,treename) ;
	res.type = BUNDLE ;
	res.disen = 1 ;
	
	VERBO3 strerr_warnt3x("create directory: ",dst,SS_SVDIRS) ;
	if(!dir_create_under(dst,SS_SVDIRS + 1,0755))
	{
		VERBO3 strerr_warnwu3sys("create ",dst,SS_SVDIRS) ;
		return 0 ;
	}
	VERBO3 strerr_warnt3x("create directory: ",dst,SS_RULES) ;
	if (!dir_create_under(dst,SS_RULES + 1,0755)){
		VERBO3 strerr_warnwu3sys("create ",dst,SS_RULES) ;
		return 0 ;
	}
	
	memcpy(dst + newlen,SS_SVDIRS,SS_SVDIRS_LEN) ;
	newlen = newlen + SS_SVDIRS_LEN ;
	dst[newlen] = 0 ;

	VERBO3 strerr_warnt3x("create directory: ",dst,SS_DB) ;
	if (!dir_create_under(dst,SS_DB + 1,0755))
	{
		VERBO3 strerr_warnwu3sys("create ",dst,SS_DB) ;
		return 0 ;
	}
	VERBO3 strerr_warnt3x("create directory: ",dst,SS_SVC) ;
	if (!dir_create_under(dst,SS_SVC + 1,0755))
	{
		VERBO3 strerr_warnwu3sys("create ",dst,SS_DB) ;
		return 0 ;
	}
		
	VERBO3 strerr_warnt3x("create directory: ",dst,SS_RESOLVE) ;
	if (!dir_create_under(dst,SS_RESOLVE + 1,0755))
	{
		VERBO3 strerr_warnwu3sys("create ",dst,SS_RESOLVE) ;
		return 0 ;
	}
	
	VERBO3 strerr_warnt1x("write resolve file of: Master") ;
	if (!ss_resolve_write(&res,dst,"Master",SS_SIMPLE))
	{
		VERBO3 strerr_warnwu1sys("write resolve file of: Master") ;
		ss_resolve_free(&res) ;
		return 0 ;
	}
	ss_resolve_free(&res) ;
	
	char sym[newlen + 1 + SS_SYM_SVC_LEN + 1] ;
	char dstsym[newlen + SS_SVC_LEN + 1] ;
	
	memcpy(sym,dst,newlen) ;
	sym[newlen] = '/' ;
	
	memcpy(sym + newlen + 1, SS_SYM_SVC, SS_SYM_SVC_LEN) ;
	sym[newlen + 1 + SS_SYM_SVC_LEN] = 0 ;
	
	
	memcpy(dstsym,dst,newlen) ;
	memcpy(dstsym + newlen, SS_SVC, SS_SVC_LEN) ;
	dstsym[newlen + SS_SVC_LEN] = 0 ;

	VERBO3 strerr_warnt4x("point symlink: ",sym," to ",dstsym) ;
	if (symlink(dstsym,sym) < 0)
	{
		VERBO3 strerr_warnwu2sys("symlink: ", sym) ;
		return 0 ;
	}
	
	memcpy(sym + newlen + 1, SS_SYM_DB, SS_SYM_DB_LEN) ;
	sym[newlen + 1 + SS_SYM_DB_LEN] = 0 ;
	
	
	memcpy(dstsym,dst,newlen) ;
	memcpy(dstsym + newlen, SS_DB, SS_DB_LEN) ;
	dstsym[newlen + SS_DB_LEN] = 0 ;
	
	VERBO3 strerr_warnt4x("point symlink: ",sym," to ",dstsym) ;
	if (symlink(dstsym,sym) < 0)
	{
		VERBO3 strerr_warnwu2sys("symlink: ", sym) ;
		return 0 ;
	}
		
	memcpy(dst + newlen,SS_DB,SS_DB_LEN) ;
	newlen = newlen + SS_DB_LEN ;
	dst[newlen] = 0 ;
	
	VERBO3 strerr_warnt3x("create directory: ",dst,SS_SRC) ;
	if (!dir_create_under(dst,SS_SRC,0755))
	{
		VERBO3 strerr_warnwu3sys("create ",dst,SS_SRC) ;
		return 0 ;
	}
		
	memcpy(dst + newlen,SS_SRC,SS_SRC_LEN) ;
	newlen = newlen + SS_SRC_LEN ;
	dst[newlen] = 0 ;
	
	
	VERBO3 strerr_warnt3x("create directory: ",dst,SS_MASTER) ;
	if(!dir_create_under(dst,SS_MASTER + 1,0755))
	{
		VERBO3 strerr_warnwu3sys("create ",dst,SS_MASTER) ;
		return 0 ;
	}
	
	memcpy(dst + newlen,SS_MASTER,SS_MASTER_LEN) ;
	newlen = newlen + SS_MASTER_LEN ;
	dst[newlen] = 0 ;
	
	VERBO3 strerr_warnt3x("create file: ",dst,"/contents") ;
	if (!file_create_empty(dst,"contents",0644))
	{
		VERBO3 strerr_warnwu3sys("create ",dst,"/contents") ;
		return 0 ;
	}

	memcpy(dst + newlen,"/type",5) ;
	newlen = newlen + 5 ;
	dst[newlen] = 0 ;
	
	VERBO3 strerr_warnt2x("create file: ",dst) ;
	if(!openwritenclose_unsafe(dst,"bundle\n",7))
	{
		VERBO3 strerr_warnwu2sys("write ",dst) ;
		return 0 ;
	}
		
	return 1 ;
}

int create_backupdir(char const *base, char const *treename)
{
	int r ;
	
	size_t baselen = strlen(base) - 1 ;//remove the last slash
	size_t treenamelen = strlen(treename) ;
	
	char treetmp[baselen + 1 + SS_SYSTEM_LEN + SS_BACKUP_LEN + treenamelen + 1 + 1] ;
	memcpy(treetmp, base, baselen) ;
	treetmp[baselen] = '/' ;
	memcpy(treetmp + baselen + 1, SS_SYSTEM, SS_SYSTEM_LEN) ;
	memcpy(treetmp + baselen + 1 + SS_SYSTEM_LEN, SS_BACKUP, SS_BACKUP_LEN) ;
	treetmp[baselen + 1 + SS_SYSTEM_LEN + SS_BACKUP_LEN] = '/' ;
	memcpy(treetmp + baselen + 1 + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1, treename, treenamelen) ;
	treetmp[baselen + 1 + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1 + treenamelen ] = 0 ;
	
	r = scan_mode(treetmp,S_IFDIR) ;
	if (r || (r < 0))
	{
		VERBO3 strerr_warnt2x("remove existing backup: ",treetmp) ;
		if (rm_rf(treetmp) < 0)
		{
			VERBO3 strerr_warnwu2sys("remove: ",treetmp) ;
			return 0 ;
		}
	}
	if (!r)
	{
		VERBO3 strerr_warnt2x("create directory: ",treetmp) ;
		if (!dir_create(treetmp,0755))
		{
			VERBO3 strerr_warnwu2sys("create directory: ",treetmp) ;
			return 0 ;
		}
	}
	
	return 1 ;
}

int set_rules(char const *tree,uid_t *uids, size_t uidn,unsigned int what) 
{
	int r ;
	size_t treelen = strlen(tree) ;
	
	char pack[256] ;
	char tmp[treelen + SS_RULES_LEN + 1] ;
	
	memcpy(tmp,tree,treelen) ;
	memcpy(tmp + treelen,SS_RULES,SS_RULES_LEN) ;
	tmp[treelen + SS_RULES_LEN] = 0 ; 
	
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
			if (r<0) return 0 ;
		}
		VERBO3 strerr_warnt4x("user: ",pack," is denied for tree: ",tree) ;
	}
	return 1 ;
}

int tree_unsupervise(char const *tree, char const *treename,uid_t owner,char const *const *envp)
{
	int r, wstat ;
	pid_t pid ;
	size_t treenamelen = strlen(treename) ;
	
	genalloc nclassic = GENALLOC_ZERO ;
	genalloc nrc = GENALLOC_ZERO ;
	genalloc gasv = GENALLOC_ZERO ; //stralist
	stralloc dtree = STRALLOC_ZERO ;
	stralloc livetree = STRALLOC_ZERO ;
	stralloc live = STRALLOC_ZERO ;
	stralloc realsym = STRALLOC_ZERO ;
	stralloc scandir = STRALLOC_ZERO ;
	ss_resolve_t res = RESOLVE_ZERO ;
	
	char prefix[treenamelen + 2] ;
	memcpy(prefix,treename,treenamelen) ;
	memcpy(prefix + treenamelen,"-",1) ;
	prefix[treenamelen + 1] = 0 ;
	
	if (!stralloc_cats(&dtree,tree)) retstralloc(111,"tree_unsupervise") ;
	if (!stralloc_cats(&dtree,SS_SVDIRS)) retstralloc(111,"tree_unsupervise") ;
	if (!stralloc_0(&dtree)) retstralloc(111,"tree_unsupervise") ;
	
	/** we don't now the live directory because a tree do not depends of the live
	 * we need to check every resolve file to find the resolve live flag*/
	if (!graph_type_src(&gasv,dtree.s,2)) strerr_diefu1sys(111,"find services") ;
	/** should be always true because of Master existence at least*/
	
	for (unsigned int i = 0 ; i < genalloc_len(stralist,&gasv) ; i++) 
	{
		if (!ss_resolve_read(&res,dtree.s,gaistr(&gasv,i))) strerr_diefu2sys(111,"read resolve file of: ",gaistr(&gasv,i)) ;
		if (res.type == CLASSIC && res.run) 
		{
			if (!stra_add(&nclassic,gaistr(&gasv,i))) strerr_diefu3sys(111,"add: ",gaistr(&gasv,i)," as service to handle") ;
		}
		else if (res.type >= BUNDLE && res.run)
		{ 
			if (!stra_add(&nrc,gaistr(&gasv,i))) strerr_diefu3sys(111,"add: ",gaistr(&gasv,i)," as service to handle") ;
		}
		if (res.live && !reslive.len)
		{
			if (!stralloc_cats(&live,res.sa.s + res.live)) retstralloc(111,"tree_unsupervise") ;
			if (!stralloc_0(&live)) retstralloc(111,"tree_unsupervise") ;
			if (!stralloc_cats(&scandir,res.sa.s + res.live)) retstralloc(111,"tree_unsupervise") ;
			if (!stralloc_0(&scandir)) retstralloc(111,"tree_unsupervise") ;
			if (!stralloc_cats(&livetree,res.sa.s + res.live)) retstralloc(111,"tree_unsupervise") ;
			if (!stralloc_0(&livetree)) retstralloc(111,"tree_unsupervise") ;
			if (!stralloc_cats(&reslive,res.sa.s + res.resolve)) retstralloc(111,"tree_unsupervise") ;
			if (!stralloc_0(&reslive)) retstralloc(111,"tree_unsupervise") ;
		}
	}
	genalloc_deepfree(stralist,&gasv,stra_free) ;
	/** db can be empty but Master is should be found,
	 * res.live can be empty if any rc service was enabled.
	 * in this case we have nothing to do  because an empty
	 * database cannot be initialized */
	
	if (reslive.len && scan_mode(reslive.s,S_IFDIR) && (genalloc_len(stralist,&nclassic) || genalloc_len(stralist,&nrc)))
	{
		r = set_livetree(&livetree,owner) ;
		if (!r) retstralloc(111,"tree_unsupervise") ;
		if (!set_livescan(&scandir,owner)) strerr_diefu1sys(111,"set scandir") ;
		/** do not trust Master because some service can be disabled but the user
		 * did not stop yet , so Master comport less service than the resolve directory*/
		
		char const *newargv[9 + genalloc_len(stralist,&nrc) + genalloc_len(stralist,&nclassic) ] ;
		unsigned int m = 0 ;
		char fmt[UINT_FMT] ;
		fmt[uint_fmt(fmt, VERBOSITY)] = 0 ;
				
		newargv[m++] = SS_BINPREFIX "66-stop" ;
		newargv[m++] = "-v" ;
		newargv[m++] = fmt ;
		newargv[m++] = "-l" ;
		newargv[m++] = live.s ;
		newargv[m++] = "-t" ;
		newargv[m++] = treename ;
		newargv[m++] = "-X" ;
				
		for (unsigned int i=0 ; i < genalloc_len(stralist,&nrc) ; i++)
			newargv[m++] = gaistr(&nrc,i) ;

		for (unsigned int i=0 ; i < genalloc_len(stralist,&nclassic) ; i++)
			newargv[m++] = gaistr(&nclassic,i) ;

		newargv[m++] = 0 ;
					
		pid = child_spawn0(newargv[0],newargv,envp) ;
		if (waitpid_nointr(pid,&wstat, 0) < 0)
		{
			VERBO3 strerr_warnwu2sys("wait for ",newargv[0]) ;
			return 0 ;
		}
		if (wstat) strerr_diefu1sys(111,"stop services") ;
		/** in use? bring down all db service */
		
		if (db_find_compiled_state(livetree.s,treename) >=0)
		{	
			for (unsigned int i = 0 ; i < genalloc_len(stralist,&nrc) ; i++)
			{
				if (obstr_equal(gaistr(&nrc,i),SS_MASTER+1)) continue ;
				ss_resolve_t dres = RESOLVE_ZERO ;
				ss_resolve_init(&dres) ;
				if (!ss_resolve_read(&dres,res.sa.s + res.resolve,gaistr(&nrc,i))) strerr_diefu2sys(111,"read resolve file of: ",gaistr(&nrc,i)) ;
				ss_resolve_setflag(&dres,SS_FLAGS_INIT,SS_FLAGS_TRUE) ;
				ss_resolve_setflag(&dres,SS_FLAGS_RELOAD,SS_FLAGS_FALSE) ;
				ss_resolve_setflag(&dres,SS_FLAGS_UNSUPERVISE,SS_FLAGS_FALSE) ;
				ss_resolve_setflag(&dres,SS_FLAGS_RUN,SS_FLAGS_FALSE) ;
				if (!ss_resolve_write(&dres,res.sa.s + res.resolve,gaistr(&nrc,i),SS_DOUBLE)) strerr_diefu2sys(111,"write resolve file of: ", gaistr(&nrc,i)) ;
				ss_resolve_free(&dres) ;
			}
			if (!stra_add(&nrc,S6RC_ONESHOT_RUNNER)) strerr_diefu3sys(111,"add ",S6RC_ONESHOT_RUNNER," as service to unsupervise") ;
			if (!stra_add(&nrc,S6RC_FDHOLDER)) strerr_diefu3sys(111,"add ",S6RC_FDHOLDER," as service to unsupervise") ;
			
			if (!stralloc_cats(&livetree,"/")) retstralloc(111,"tree_unsupervise") ;
			if (!stralloc_cats(&livetree,treename)) retstralloc(111,"tree_unsupervise") ;
			if (!stralloc_0(&livetree)) retstralloc(111,"tree_unsupervise") ;
			/** unsupervise service into the scandir */
			for (unsigned int i=0 ; i < genalloc_len(stralist,&nrc) ;i++)
				s6rc_servicedir_unsupervise(livetree.s,prefix,gaistr(&nrc,i),0) ;
				
			r = sarealpath(&realsym,livetree.s) ;
			if (r < 0 ) strerr_diefu2sys(111,"find realpath of: ",livetree.s) ; 
			if (!stralloc_0(&realsym)) retstralloc(111,"tree_unsupervise") ;
			if (rm_rf(realsym.s) < 0) strerr_diefu2sys(111,"remove ", realsym.s) ;
			if (rm_rf(livetree.s) < 0) strerr_diefu2sys(111,"remove ", livetree.s) ;		
		}
		
		if (genalloc_len(stralist,&nclassic))
		{
			for (unsigned int i = 0 ; i < genalloc_len(stralist,&nclassic) ; i++) 
			{
				ss_resolve_init(&res) ;
				if (!ss_resolve_read(&res,reslive.s,gaistr(&nclassic,i))) strerr_diefu2sys(111,"read resolve file of: ",gaistr(&nclassic,i)) ;
				ss_resolve_setflag(&res,SS_FLAGS_INIT,SS_FLAGS_TRUE) ;
				ss_resolve_setflag(&res,SS_FLAGS_RELOAD,SS_FLAGS_FALSE) ;
				ss_resolve_setflag(&res,SS_FLAGS_UNSUPERVISE,SS_FLAGS_FALSE) ;
				ss_resolve_setflag(&res,SS_FLAGS_RUN,SS_FLAGS_FALSE) ;
				if (!ss_resolve_write(&res,res.sa.s + res.resolve,gaistr(&nclassic,i),SS_DOUBLE)) strerr_diefu2sys(111,"set write resolve file of: ", gaistr(&nclassic,i)) ;
				if (rm_rf(res.sa.s + res.runat) < 0) strerr_diefu2sys(111,"remove: ", res.sa.s + res.runat) ;
			}
		}
		if (scandir_send_signal(scandir.s,"an") <= 0) strerr_diefu2sys(111,"reload scandir: ",scandir.s) ;
		
		VERBO1 strerr_warni2x("Unsupervised successfully: ",treename) ;
	}
	else VERBO1 strerr_warni2x("Not supervised: ",treename) ;
	
	genalloc_deepfree(stralist,&nrc,stra_free) ;
	genalloc_deepfree(stralist,&nclassic,stra_free) ;
	stralloc_free(&livetree) ; 
	stralloc_free(&dtree) ; 
	stralloc_free(&live) ;
	stralloc_free(&realsym) ; 
	stralloc_free(&scandir) ; 
	ss_resolve_free(&res) ;
	
	return 1 ;
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
	
	current = create = allow = deny = enable = disable = remove = snap = unsupervise = 0 ;
	
	PROG = "66-tree" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			
			int opt = getopt_args(argc,argv, "hv:na:d:cEDRC:U", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'h' : info_help(); return 0 ;
				case 'v' : if (!uint0_scan(l.arg, &VERBOSITY)) exitusage(USAGE) ; break ;
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
				case 'R' : remove = 1 ; unsupervise = 1 ; if (create) exitusage(USAGE) ; break ;
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
	
	VERBO2 strerr_warni3x("sanitize ",tree," ..." ) ;
	r = sanitize_tree(&dstree,base.s,tree,owner) ;
	if (r < 0){
		strerr_diefu2x(111,"sanitize ",tree) ;
	}
	
	if(!r && create)
	{
		VERBO2 strerr_warni3x("creating ",dstree.s," ..." ) ;
		if (!create_tree(dstree.s,tree))
		{
			cleanup(dstree.s) ;
			strerr_diefu2x(111,"create tree: ",dstree.s) ;
		}
		VERBO2 strerr_warni3x("creating backup directory for ", tree," ...") ;
		if (!create_backupdir(base.s,tree))
		{
			cleanup(dstree.s) ;
			strerr_diefu2x(111,"create backup directory for: ",tree) ;
		}
		VERBO2 strerr_warni3x("set permissions rules for ",dstree.s," ..." ) ;
		if (!set_rules(dstree.s,auids,auidn,1))//1 allow
		{
			cleanup(dstree.s) ;
			strerr_diefu1x(111,"set permissions access") ;
		}
		
		size_t dblen = dstree.len - 1 ;
		char newdb[dblen + SS_SVDIRS_LEN + 1] ;
		memcpy(newdb,dstree.s,dblen) ;
		memcpy(newdb + dblen, SS_SVDIRS, SS_SVDIRS_LEN) ;
		newdb[dblen + SS_SVDIRS_LEN] = 0 ;
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
		
		if (r != 1) strerr_diefu6x(111,"add: ",dstree.s," at: ",base.s,SS_SYSTEM,SS_STATE) ;
		VERBO1 strerr_warni2x("Enabled successfully tree: ",tree) ;
	}
	
	if (disable)
	{
		VERBO2 strerr_warni3x("disable ",dstree.s," ..." ) ;
		r  = tree_cmd_state(VERBOSITY,"-d",tree) ;
		if (r != 1) strerr_diefu6x(111,"remove: ",dstree.s," at: ",base.s,SS_SYSTEM,SS_STATE) ;
		VERBO1 strerr_warni2x("Disabled successfully tree: ",tree) ;
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
	if (unsupervise) tree_unsupervise(dstree.s,tree,owner,envp) ;
	
	if (remove)
	{
		VERBO2 strerr_warni3x("delete ",dstree.s," ..." ) ;
		if (rm_rf(dstree.s) < 0) strerr_diefu2sys(111,"delete ", dstree.s) ;
				
		size_t treelen = strlen(tree) ;
		size_t baselen = base.len ;
		char treetmp[baselen + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1 + treelen  + 1] ;
		memcpy(treetmp, base.s, baselen) ;
		memcpy(treetmp + baselen, SS_SYSTEM, SS_SYSTEM_LEN) ;
		memcpy(treetmp + baselen + SS_SYSTEM_LEN, SS_BACKUP, SS_BACKUP_LEN) ;
		memcpy(treetmp + baselen + SS_SYSTEM_LEN + SS_BACKUP_LEN, "/", 1) ;
		memcpy(treetmp + baselen + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1, tree, treelen) ;
		treetmp[baselen + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1 + treelen ] = 0 ;
		
		r = scan_mode(treetmp,S_IFDIR) ;
		if (r || (r < 0))
		{
			VERBO2 strerr_warni3x("delete backup of tree ",treetmp," ...") ;
			if (rm_rf(treetmp) < 0)
			{
				VERBO3 strerr_warnwu2sys("delete: ",treetmp) ;
				return 0 ;
			}
		}
		VERBO2 strerr_warni6x("disable: ",dstree.s," at: ",base.s,SS_SYSTEM,SS_STATE) ;
		r  = tree_cmd_state(VERBOSITY,"-d",tree) ;
		if (r != 1) strerr_diefu6x(111,"delete: ",dstree.s," at: ",base.s,SS_SYSTEM,SS_STATE) ;
		/** remove /run/66/state/treename directory */
		if (reslive.len && (scan_mode(reslive.s,S_IFDIR)))
		{
			VERBO2 strerr_warni3x("delete ",reslive.s," ..." ) ;
			if (rm_rf(reslive.s) < 0) strerr_diefu2sys(111,"delete ",reslive.s) ;
		}
		VERBO1 strerr_warni2x("Deleted successfully: ",tree) ;
	}
	
	if (snap)
	{
		char tmp[dstree.len + 1] ;
		memcpy(tmp,dstree.s,dstree.len) ;
		tmp[dstree.len] = 0 ;
		r  = get_rlen_until(dstree.s,'/',dstree.len) ;
		dstree.len = r + 1;
					
		if (!stralloc_cats(&dstree,clone.s)) retstralloc(111,"main") ;
		if (!stralloc_0(&dstree)) retstralloc(111,"main") ;
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
