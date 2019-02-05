/* 
 * 66-tree.c
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

//#include <stdio.h>

#define USAGE "66-tree [ -h ] [ -v verbosity ] [ -n | R ] [ -a ] [ -d ] [ -c ] [ -E | D ] [ -C ] tree" 

unsigned int VERBOSITY = 1 ;

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
"	-E: enable tree\n"
"	-D: disable tree\n"
"	-R: remove tree\n"
"	-C: clone tree\n"
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
	if(!r){
		
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
			if (sanitize_extra(SS_LOGGER_USERDIR) < 0)
			{ VERBO3 strerr_warnwu2sys("create directory: ",SS_LOGGER_SYSDIR) ; return -1 ; }
			if (sanitize_extra(SS_SERVICE_USERDIR) < 0)
			{ VERBO3 strerr_warnwu2sys("create directory: ",SS_LOGGER_SYSDIR) ; return -1 ; }
			if (sanitize_extra(SS_SERVICE_USERCONFDIR) < 0)
			{ VERBO3 strerr_warnwu2sys("create directory: ",SS_LOGGER_SYSDIR) ; return -1 ; }
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



int create_tree(char const *tree,char const *const *envp)
{
	size_t newlen = 0 ;
	size_t treelen = strlen(tree) ;
	
	char dst[treelen + SS_DB_LEN + SS_SRC_LEN + 13 + 1] ;

	
	memcpy(dst, tree, treelen) ;
	newlen = treelen ;
	dst[newlen] = 0 ;
	
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
	memcpy(dst + newlen, SS_RESOLVE, SS_RESOLVE_LEN) ;
	dst[newlen + SS_RESOLVE_LEN] = 0 ;
	VERBO3 strerr_warnt3x("create directory: ",dst,SS_MASTER) ;
	if (!dir_create_under(dst,SS_MASTER + 1,0755))
	{
		VERBO3 strerr_warnwu3sys("create ",dst,SS_MASTER) ;
		return 0 ;
	}
	memcpy(dst + newlen + SS_RESOLVE_LEN, SS_MASTER, SS_MASTER_LEN) ;
	dst[newlen + SS_RESOLVE_LEN + SS_MASTER_LEN] = 0 ;
	
	VERBO3 strerr_warnt3x("create file: ",dst,"/type") ;
	if(!file_write_unsafe(dst,"type","bundle\n",7))
	{
		VERBO3 strerr_warnwu3sys("write ",dst,"/type") ;
		return 0 ;
	}
	
	dst[newlen] = 0 ;
	
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
			else
			{
				VERBO3 strerr_warnt4x("user: ",pack," is allowed for tree: ",tree) ;
			}
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

 
int main(int argc, char const *const *argv,char const *const *envp)
{
	int r, current, create, allow, deny, enable, disable, remove, snap ;
	
	uid_t owner ;
	
	size_t auidn = 0 ;
	uid_t auids[256] ;
	
	size_t duidn = 0 ;
	uid_t duids[256] ;
	
	char const *tree = NULL ;
	
	stralloc base = STRALLOC_ZERO ;
	stralloc dstree = STRALLOC_ZERO ;
	stralloc clone = STRALLOC_ZERO ;
	
	current = create = allow = deny = enable = disable = remove = snap = 0 ;
	
	PROG = "66-tree" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			
			int opt = getopt_args(argc,argv, "hv:na:d:cEDRC:", &l) ;
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
				case 'R' : remove = 1 ; if (create) exitusage(USAGE) ; break ;
				case 'C' : if (!stralloc_cats(&clone,l.arg)) retstralloc(111,"main") ;
						   if (!stralloc_0(&clone)) retstralloc(111,"main") ;
						   snap = 1 ;
						   break ;
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
		if (!create_tree(dstree.s,envp))
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
	}
	
	if ((!r && !create) || (!r && enable)) strerr_diefu2x(111,"find tree: ",dstree.s) ;
	if (r && create) strerr_diefu3x(110,"create ",dstree.s,": already exist") ;

	if (enable)
	{
		VERBO2 strerr_warni3x("enable ",dstree.s," ..." ) ;
		r  = tree_cmd_state(VERBOSITY,"-a",tree) ;
		
		if (r != 1) strerr_diefu6x(111,"add: ",dstree.s," at: ",base.s,SS_SYSTEM,SS_STATE) ;
	}
	
	if (disable)
	{
		VERBO2 strerr_warni3x("disable ",dstree.s," ..." ) ;
		r  = tree_cmd_state(VERBOSITY,"-d",tree) ;
		if (r != 1) strerr_diefu6x(111,"remove: ",dstree.s," at: ",base.s,SS_SYSTEM,SS_STATE) ;
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
	}
	
	if (remove)
	{
		VERBO2 strerr_warni3x("remove ",dstree.s," ..." ) ;
		if (rm_rf(dstree.s) < 0) strerr_diefu2sys(111,"remove ", dstree.s) ;
				
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
			VERBO2 strerr_warni3x("remove backup of tree ",treetmp," ...") ;
			if (rm_rf(treetmp) < 0)
			{
				VERBO3 strerr_warnwu2sys("remove: ",treetmp) ;
				return 0 ;
			}
		}
		VERBO2 strerr_warni6x("disable: ",dstree.s," at: ",base.s,SS_SYSTEM,SS_STATE) ;
		r  = tree_cmd_state(VERBOSITY,"-d",tree) ;
		if (r != 1) strerr_diefu6x(111,"remove: ",dstree.s," at: ",base.s,SS_SYSTEM,SS_STATE) ;
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
	}
	
	stralloc_free(&base) ;
	stralloc_free(&dstree) ;
	stralloc_free(&clone) ;
	
	return 0 ;
}
