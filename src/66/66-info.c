/* 
 * 66-info.c
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

#include <string.h>
#include <sys/stat.h>
#include <locale.h>
#include <langinfo.h>

#include <oblibs/obgetopt.h>
#include <oblibs/error2.h>
#include <oblibs/stralist.h>
#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/directory.h>

#include <skalibs/buffer.h>
#include <skalibs/stralloc.h>
#include <skalibs/unix-transactional.h>
#include <skalibs/direntry.h>
#include <skalibs/types.h>
#include <skalibs/bytestr.h>
#include <skalibs/lolstdio.h>
#include <skalibs/djbunix.h>

#include <66/utils.h>
#include <66/tree.h>
#include <66/constants.h>
#include <66/enum.h>

#include <s6/s6-supervise.h>//s6_svc_ok

#include <stdio.h>
unsigned int VERBOSITY = 1 ;
static stralloc base = STRALLOC_ZERO ;
static stralloc live = STRALLOC_ZERO ;
static stralloc SCANDIR = STRALLOC_ZERO ; // upper case to avoid conflicts with dirent.h
static uid_t owner ;

#define MAXSIZE 4096
#define DBG(...) bprintf(buffer_1, __VA_ARGS__) ;\
				buffer_putflush(buffer_1,"\n",1) ;


#define USAGE "66-info [ -h help ] [ -T tree ] [ -S service ] sub-options (use -h as sub-options for futher informations)"

#define TREE_USAGE "66-info -T [ -help ] tree "
#define exit_tree_usage() strerr_dieusage(100, TREE_USAGE)
#define SV_USAGE "66-info -S [ -help ] [ -l live ] [ -p n lines ] service"
#define exit_sv_usage() strerr_dieusage(100, SV_USAGE)



static inline void info_help (void)
{
  static char const *help =
"66-info <options> sub-options \n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-T: get informations about tree(s)\n"
"	-S: get informations about service(s)\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
}

static inline void tree_help (void)
{
  static char const *help =
"66-info -T <options> tree\n"
"\n"
"options :\n"
"	-h: print this help\n" 
;

 if (buffer_putsflush(buffer_1, help) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
}

static inline void sv_help (void)
{
  static char const *help =
"66-info -S <options> service\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-l: live directory\n"
"	-p: print n last lines of the associated log file\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
}

char *print_nlog(char *str, int n) 
{ 
	int r = 0 ;
	int DELIM ='\n' ;
	size_t slen = strlen(str) ;
	
	if (n <= 0) return NULL; 

	size_t ndelim = 0;  
	char *target_pos = NULL;   
	
	r = get_rlen_until(str,DELIM,slen) ;
	
	//target_pos = strrchr(str, DELIM); 
	target_pos = str + r ; 

	if (target_pos == NULL) return NULL; 

	while (ndelim < n) 
	{ 
		while (str < target_pos && *target_pos != DELIM) 
			--target_pos; 

		if (*target_pos ==  DELIM) 
			--target_pos, ++ndelim; 
		else break; 
	} 

	if (str < target_pos) 
		target_pos += 2; 

	return target_pos ;
} 

int get_fromdir(genalloc *ga, char const *srcdir,mode_t mode)
{
	int fdsrc ;
	
	DIR *dir = opendir(srcdir) ;
	if (!dir)
		return 0 ;
	
	fdsrc = dir_fd(dir) ;
	
	for (;;)
    {
		struct stat st ;
		direntry *d ;
		d = readdir(dir) ;
		if (!d) break ;
		if (d->d_name[0] == '.')
		if (((d->d_name[1] == '.') && !d->d_name[2]) || !d->d_name[1])
			continue ;
		if (stat_at(fdsrc, d->d_name, &st) < 0)
			return 0 ;
		if((st.st_mode & S_IFMT) == mode)
		{
			if (!str_diff(d->d_name,SS_BACKUP + 1)) continue ; 
			if (!stra_add(ga,d->d_name)) return 0 ;		 
		}
	}

	return 1 ;
}

int print_status(char const *svname,char const *type,char const *treename, char const *const *envp)
{
	int r ;
	int wstat ;
	pid_t pid ;
	
	stralloc livetree = STRALLOC_ZERO ;
	stralloc svok = STRALLOC_ZERO ;
	
	if (!stralloc_copy(&SCANDIR,&live)) retstralloc(111,"main") ;
	if (!stralloc_copy(&livetree,&live)) retstralloc(111,"main") ;
	
	r = set_livescan(&SCANDIR,owner) ;
	if (!r) retstralloc(111,"main") ;
	if(r < 0) strerr_dief3x(111,"live: ",SCANDIR.s," must be an absolute path") ;
	
	if ((scandir_ok(SCANDIR.s)) !=1 )
	{
		if (buffer_putsflush(buffer_1,"scandir is not running\n") < 0) return 0 ;
		goto out ;
	}
	
	if (!stralloc_copy(&livetree,&live)) retstralloc(111,"main") ;
	
	r = set_livetree(&livetree,owner) ;
	if (!r) retstralloc(111,"main") ;
	if (r < 0 ) strerr_dief3x(111,"live: ",livetree.s," must be an absolute path") ;
	
	if (get_enumbyid(type,key_enum_el) == CLASSIC)
	{
		size_t scanlen = SCANDIR.len - 1 ;
		if (!stralloc_catb(&svok,SCANDIR.s,scanlen)) retstralloc(0,"print_status") ;
		if (!stralloc_cats(&svok,"/")) retstralloc(0,"print_status") ;
		if (!stralloc_cats(&svok,svname)) retstralloc(0,"print_status") ;
	}
	else if (get_enumbyid(type,key_enum_el) == LONGRUN)
	{
		size_t livelen = livetree.len -1 ;
		if (!stralloc_catb(&svok,livetree.s,livelen)) retstralloc(0,"print_status") ;
		if (!stralloc_cats(&svok,"/")) retstralloc(0,"print_status") ;
		if (!stralloc_cats(&svok,treename)) retstralloc(0,"print_status") ;
		if (!stralloc_cats(&svok,SS_SVDIRS)) retstralloc(0,"print_status") ;
		if (!stralloc_cats(&svok,"/")) retstralloc(0,"print_status") ;
		if (!stralloc_cats(&svok,svname)) retstralloc(0,"print_status") ;
	}
	else
	{
		if (buffer_putsflush(buffer_1,"nothing to display\n") < 0) return 0 ;
		goto out ;
	}
	if (!stralloc_0(&svok)) retstralloc(0,"print_status") ;
	
	r = s6_svc_ok(svok.s) ;
	if (r != 1)
	{
		if (buffer_putsflush(buffer_1,"not running\n") < 0) return 0 ;
		goto out ;
	}
	char const *newargv[3] ;
	unsigned int m = 0 ;
	
	newargv[m++] = SS_BINPREFIX "s6-svstat" ;
	newargv[m++] = svok.s ;
	newargv[m++] = 0 ;
				
	pid = child_spawn0(newargv[0],newargv,envp) ;
	if (waitpid_nointr(pid,&wstat, 0) < 0)
		strerr_diefu2sys(111,"wait for ",newargv[0]) ;
	
	if (wstat)
		strerr_diefu2x(111,"status for service: ",svname) ;
	
	out:
		stralloc_free(&livetree) ;
		stralloc_free(&svok) ;
		
	return 1 ;
}

int tree_args(int argc, char const *const *argv)
{
	int r, comma, master ;
	
	genalloc gatree = GENALLOC_ZERO ;// stralist, all tree
	stralloc tree = STRALLOC_ZERO ;
	stralloc sacurrent = STRALLOC_ZERO ;
	char current[MAXSIZE] ;
	
	r = tree_find_current(&sacurrent,base.s) ;
	if (r)
	{
		size_t currlen = get_rlen_until(sacurrent.s,'/',sacurrent.len - 1) ;
		size_t currnamelen = (sacurrent.len - 1) - currlen ;
		memcpy(current, sacurrent.s + currlen + 1,currnamelen) ;
		currnamelen-- ;
		current[currnamelen] = 0 ;
	}

	size_t baselen = base.len - 1 ;
	char src[baselen + SS_SYSTEM_LEN + 1] ;
	memcpy(src,base.s,baselen) ;
	memcpy(src + baselen,SS_SYSTEM, SS_SYSTEM_LEN) ;
	src[baselen + SS_SYSTEM_LEN] = 0 ;
	
	comma = master = 0 ;
	//without args see all tree available, if arg = tree, -s to specify see service
	{
		subgetopt_t l = SUBGETOPT_ZERO ;
			
		for (;;)
		{
			int opt = getopt_args(argc,argv, ">h", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			
			switch (opt)
			{
				case 'h' : 	tree_help(); return 0 ;
				default : exit_tree_usage() ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	if (argc > 1) exit_tree_usage();
	
	if (!argv[0])
	{
		if (!get_fromdir(&gatree, src, S_IFDIR)) return 0 ;
		if (!bprintf(buffer_1,"%s\n","[Available tree]")) return 0 ;
		if (genalloc_len(stralist,&gatree))
		{
			for (unsigned int i = 0 ; i < genalloc_len(stralist,&gatree) ; i++)
			{
				int enabled = tree_cmd_state(VERBOSITY,"-s",gaistr(&gatree,i)) ; 
				if (!bprintf(buffer_1," %s : ",gaistr(&gatree,i))) return 0 ;	
				if (obstr_equal(gaistr(&gatree,i),current))
				{ if (!bprintf(buffer_1, "%s","current")) return 0 ; comma = 1 ; }
				if (enabled)
				{ 
					if (comma) if(!bprintf(buffer_1, "%s",",")) return 0 ;
					if (!bprintf(buffer_1, "%s","enabled")) return 0;
				}
				if (buffer_putflush(buffer_1,"\n",1) < 0) return 0 ;
				comma = 0 ;
			}
			gatree = genalloc_zero ;
		}
		else 
		{
			if (!bprintf(buffer_1," %s ","nothing to display")) return 0 ;
			if (buffer_putflush(buffer_1,"\n",1) < 0) return 0 ;
		}
		return 1 ;
	}
	
	if(!stralloc_cats(&tree,argv[0])) retstralloc(111,"main") ;
	if(!stralloc_0(&tree)) retstralloc(111,"main") ;
	
	r = tree_sethome(&tree,base.s) ;
	if (r < 0) strerr_diefu1x(110,"find the current tree. You must use -t options") ;
	if (!r) strerr_diefu2sys(111,"find tree: ", tree.s) ;
		
	size_t treelen = get_rlen_until(tree.s,'/',tree.len - 1) ;
	size_t treenamelen = (tree.len - 1) - treelen ;
	char treename[treenamelen + 1] ;
	memcpy(treename, tree.s + treelen + 1,treenamelen) ;
	treenamelen-- ;
	treename[treenamelen] = 0 ;
	
	size_t satreelen = tree.len - 1 ;
	size_t newlen ;
	char what[satreelen + SS_SVDIRS_LEN + SS_DB_LEN + SS_SRC_LEN + 1] ;
	memcpy(what,tree.s,satreelen) ;
	memcpy(what + satreelen, SS_SVDIRS,SS_SVDIRS_LEN) ;
	newlen = satreelen + SS_SVDIRS_LEN ;
	
	if (!bprintf(buffer_1,"%s%s%s\n","[Service on tree: ",treename,"]")) return 0 ;
	
	/** classic */
	memcpy(what + newlen, SS_SVC, SS_SVC_LEN) ;
	what[newlen + SS_SVC_LEN] = 0 ;
			
	if (!bprintf(buffer_1," %s","classic :")) ;
	if (!get_fromdir(&gatree,what,S_IFDIR)) return 0 ;
	if (genalloc_len(stralist,&gatree))
	{
		for (unsigned int i = 0 ; i < genalloc_len(stralist,&gatree) ; i++)
			if (!bprintf(buffer_1," %s ",gaistr(&gatree,i))) return 0 ;
				
		gatree = genalloc_zero ;
	}
	else if (!bprintf(buffer_1," %s ","nothing to display")) return 1 ;
	if (buffer_putflush(buffer_1,"\n",1) < 0) return 1 ;
	
	/** rc */
	memcpy(what + newlen, SS_DB, SS_DB_LEN) ;
	memcpy(what + newlen + SS_DB_LEN, SS_SRC, SS_SRC_LEN) ;
	what[newlen + SS_DB_LEN + SS_SRC_LEN] = 0 ;
	
	if (!bprintf(buffer_1," %s","rc :")) ;
	if (!get_fromdir(&gatree,what,S_IFDIR)) return 0 ;
	if (genalloc_len(stralist,&gatree) > 1) //only pass if Master is not alone
	{
		for (unsigned int i = 0 ; i < genalloc_len(stralist,&gatree) ; i++)
		{
			if (!str_diff(gaistr(&gatree,i),SS_MASTER+1))
				continue ; 
			if (!bprintf(buffer_1," %s ",gaistr(&gatree,i))) return 0 ;
		}
		gatree = genalloc_zero ;
	}
	else if (!bprintf(buffer_1," %s ","nothing to display")) return 0 ;
	if (buffer_putflush(buffer_1,"\n",1) < 0) return 0 ;
	
	
	stralloc_free(&tree) ;
	stralloc_free(&sacurrent) ;
	genalloc_deepfree(stralist,&gatree,stra_free) ;
	
	return 1 ;
}

int sv_args(int argc, char const *const *argv,char const *const *envp)
{
	int r ; 
	unsigned int nlog = 0 ;
	
	stralloc tree = STRALLOC_ZERO ;
	stralloc what = STRALLOC_ZERO ;
	stralloc type = STRALLOC_ZERO ;
	genalloc gawhat = GENALLOC_ZERO ;//stralist
	genalloc gatree = GENALLOC_ZERO ;
	
	char const *svname = NULL ;
	char const *treename = NULL ;
	
	r = 0 ;
	
	{
		subgetopt_t l = SUBGETOPT_ZERO ;
		
		for (;;)
		{
			int opt = getopt_args(argc,argv, ">hl:p:", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'h' : 	sv_help(); return 0 ;
				case 'l' : 	if (!stralloc_cats(&live,l.arg)) retstralloc(111,"main") ;
							if (!stralloc_0(&live)) retstralloc(111,"main") ;
							break ;
				case 'p' :	if (!uint0_scan(l.arg, &nlog)) exitusage() ; break ;
				default : exit_sv_usage() ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	if (argc > 1 || !argc) exit_sv_usage() ;

	svname = *argv ;
	
	r = set_livedir(&live) ;
	if (!r) retstralloc(111,"main") ;
	if (r < 0 ) strerr_dief3x(111,"live: ",live.s," must be an absolute path") ;
	
	size_t baselen = base.len - 1 ;
	size_t newlen ;
	char src[MAXSIZE] ;
	memcpy(src,base.s,baselen) ;
	memcpy(src + baselen,SS_SYSTEM, SS_SYSTEM_LEN) ;
	baselen = baselen + SS_SYSTEM_LEN ;
	src[baselen] = 0 ;
	
	{
		if (!get_fromdir(&gatree, src, S_IFDIR)) return 0 ;
		
		if (genalloc_len(stralist,&gatree))
		{
			for (unsigned int i = 0 ; i < genalloc_len(stralist,&gatree) ; i++)
			{
					treename = gaistr(&gatree,i) ;
					size_t treelen = gaistrlen(&gatree,i) ;
					src[baselen] = '/' ;
					memcpy(src + baselen + 1,treename,treelen) ;
					memcpy(src + baselen + 1 + treelen,SS_SVDIRS,SS_SVDIRS_LEN) ;
					newlen = baselen + 1 + treelen + SS_SVDIRS_LEN ;
					memcpy(src + baselen + 1 + treelen + SS_SVDIRS_LEN,SS_RESOLVE,SS_RESOLVE_LEN) ;
					src[baselen + 1 + treelen + SS_SVDIRS_LEN + SS_RESOLVE_LEN] = 0 ;
					if (dir_search(src,svname,S_IFDIR))
					{
						if(!stralloc_cats(&tree,treename)) retstralloc(111,"main") ;
						if(!stralloc_0(&tree)) retstralloc(111,"main") ;
						src[newlen] = 0 ;
						break ;
					}
			}
			
		}
		else 
		{
			if (!bprintf(buffer_1," %s ","nothing to display")) return 0 ;
			if (buffer_putflush(buffer_1,"\n",1) < 0) return 0 ;
			return 1 ;
		}
	}
	
	r = tree_sethome(&tree,base.s) ;
	if (r < 0) strerr_diefu1x(110,"find the current tree. You must use -t options") ;
	if (!r) strerr_diefu2sys(111,"find tree: ", tree.s) ;
	
	if (!bprintf(buffer_1,"%s%s%s\n","[",svname,"]")) return 0 ;
	if (!bprintf(buffer_1,"%s%s\n","tree : ",treename)) return 0 ;
	
	/** retrieve type but do not print it*/	
	r = resolve_read(&type,src,svname,"type") ;
	if (r < -1) strerr_dief2sys(111,"invalid .resolve directory: ",src) ;
	if (r <= 0) strerr_diefu2x(111,"read type of: ",svname) ;
	
	/** status */
	if (buffer_putsflush(buffer_1,"status : ") < 0) return 0 ;
	print_status(svname,type.s,treename,envp) ;

	/** print the type */	
	if (!bprintf(buffer_1,"%s %s\n","type :",type.s)) return 0 ;
	
	/** description */
	r = resolve_read(&what,src,svname,"description") ;
	if (r < -1) strerr_dief2sys(111,"invalid .resolve directory: ",src) ;
	if (r <= 0) strerr_diefu2x(111,"read description of: ",svname) ;
	if (!bprintf(buffer_1,"%s %s\n","description :",what.s)) return 0 ;
	
	/** dependencies */
	if (get_enumbyid(type.s,key_enum_el) > CLASSIC) 
	{	
		if (get_enumbyid(type.s,key_enum_el) == BUNDLE) 
		{
			if (!bprintf(buffer_1,"%s\n","contents :")) return 0 ;
		}
		else if (!bprintf(buffer_1,"%s\n","depends on :")) return 0 ;
		r = resolve_read(&what,src,svname,"deps") ;
		if (what.len)
		{
			if (!clean_val(&gawhat,what.s)) return 0 ;
			
			for (unsigned int i = 0 ; i < genalloc_len(stralist,&gawhat) ; i++)
			{
				if (!bprintf(buffer_1," %s ",gaistr(&gawhat,i))) return 0 ;
			}
		}
		else if (!bprintf(buffer_1," %s ","nothing to display")) return 0 ;
		if (buffer_putflush(buffer_1,"\n",1) < 0) return 0 ;
	}
	
	/** logger */
	if (get_enumbyid(type.s,key_enum_el) == CLASSIC || get_enumbyid(type.s,key_enum_el) == LONGRUN) 
	{
		if (!bprintf(buffer_1,"%s ","logger at :")) return 0 ;
		r = resolve_read(&what,src,svname,"logger") ;
		if (r < -1) strerr_dief2sys(111,"invalid .resolve directory: ",src) ;
		if (r <= 0)
		{	
			if (!bprintf(buffer_1,"%s \n","apparently not")) return 0 ;
		}
		else
		{
			r = resolve_read(&what,src,svname,"dstlog") ;
			if (r < -1) strerr_dief2sys(111,"invalid .resolve directory: ",src) ;
			if (r <= 0)
			{
				if (!bprintf(buffer_1,"%s \n","unset")) return 0 ;
			}
			else
			{
				if (!bprintf(buffer_1,"%s \n",what.s)) return 0 ;
				if (nlog)
				{
					stralloc log = STRALLOC_ZERO ;
					if (!file_readputsa(&log,what.s,"current")) retstralloc(0,"sv_args") ;
					if (!bprintf(buffer_1,"%s \n",print_nlog(log.s,nlog))) return 0 ;
					stralloc_free(&log) ;
				}
			}
		}
	}
	if (buffer_putsflush(buffer_1,"") < 0) return 0 ;
	
	stralloc_free(&tree) ;
	stralloc_free(&what) ;
	stralloc_free(&type) ;
	genalloc_deepfree(stralist,&gawhat,stra_free) ;
	genalloc_deepfree(stralist,&gatree,stra_free) ;
	
	return 0 ;
}
int main(int argc, char const *const *argv, char const *const *envp)
{
	int what ;
	
	what = -1 ;
		
	PROG = "66-info" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(2,argv, ">hTS", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'h' : 	info_help(); return 0 ;
				case 'T' : 	what = 0 ; break ;
				case 'S' :	what = 1 ; break ;
				default : exitusage() ; 
			}
		}
	}
	if (what<0) exitusage() ;
	
	owner = MYUID ;
	
	/*
	setlocale(LC_ALL, "");
		
	if(!str_diff(nl_langinfo(CODESET), "UTF-8")) {
		style = &graph_utf8;
	}
	*/
	
	if (!set_ownersysdir(&base,owner)) strerr_diefu1sys(111, "set owner directory") ;
	
	if (!what) tree_args(--argc,++argv) ;
	if (what) sv_args(--argc,++argv,envp) ;
	
	stralloc_free(&base) ;
	stralloc_free(&live) ;
	stralloc_free(&SCANDIR) ;
	
	return 0 ;
}
