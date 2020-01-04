/* 
 * 66-update.c
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

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sastr.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>

#include <skalibs/stralloc.h>
#include <skalibs/buffer.h>
#include <skalibs/sgetopt.h>
#include <skalibs/djbunix.h>
#include <skalibs/unix-transactional.h>

#include <66/ssexec.h>
#include <66/constants.h>
#include <66/utils.h>
#include <66/db.h>
#include <66/svc.h>
#include <66/tree.h>
#include <66/backup.h>
#include <66/resolve.h>
#include <66/parser.h>

#define USAGE "66-update [ -h ] [ -c ] [ -v verbosity ] [ -l live ] [ -d ] tree(s)"

static stralloc WORKDIR = STRALLOC_ZERO ;
static uint8_t DRYRUN = 0 ;
static char *drun = "dry run do: " ;
static inline void info_help (void)
{
  static char const *help =
"66-update <options> tree(s)\n"
"\n"
"options :\n"
"	-h: print this help\n"
"	-c: use color\n"
"	-v: increase/decrease verbosity\n"
"	-l: live directory\n"
"	-d: dry run\n"
"\n"
"if no tree is given, all trees will be processed.\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
}

static void cleanup(void)
{
	int e = errno ;
	if (WORKDIR.len)
	{
		log_trace(DRYRUN ? drun : "","delete temporary directory: ",WORKDIR.s) ;
	//	rm_rf(WORKDIR.s) ;
	}
	errno = e ;
}

int tree_is_current(char const *base,char const *treename,uid_t owner)
{
	stralloc sacurr = STRALLOC_ZERO ;
	int current = 0 ;
	
	if (tree_find_current(&sacurr,base,owner))
	{
		char name[sacurr.len + 1] ;
		if (!basename(name,sacurr.s)) log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"basename of: ",sacurr.s) ;
		current = obstr_equal(treename,name) ;
	}
	stralloc_free(&sacurr) ;
	return current ;
}

int tree_is_enabled(char const *treename)
{
	return tree_cmd_state(VERBOSITY,"-s",treename) ;	
}

void tree_allowed(stralloc *list,char const *base, char const *treename)
{
	stralloc sa = STRALLOC_ZERO ;
	size_t treenamelen = strlen(treename), baselen = strlen(base), pos ;
	char tmp[baselen + SS_SYSTEM_LEN + 1 + treenamelen + SS_RULES_LEN + 1] ;
	
	auto_strings(tmp,base,SS_SYSTEM,"/",treename,SS_RULES) ;
	
	if (!sastr_dir_get(&sa,tmp,"",S_IFREG))
		log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"get permissions of tree at: ",tmp) ;

	for (pos = 0 ;pos < sa.len; pos += strlen(sa.s + pos) + 1)
	{
		char *suid = sa.s + pos ;
		uid_t uid = 0 ;
		if (!uid0_scan(suid, &uid))
			log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"get uid of: ",suid) ;
		if (pos) 
			if (!stralloc_cats(list,",")) log_die_nomem_nclean(&cleanup,"stralloc") ;
		if (!get_namebyuid(uid,list))
			log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"get name of uid: ",suid) ;
	}
	if (!stralloc_0(list)) log_die_nomem_nclean(&cleanup,"stralloc") ;
	log_trace(DRYRUN ? drun : "","allowed user(s) for tree: ",treename," are: ",list->s) ;
	stralloc_free(&sa) ;
}

void tree_contents(stralloc *list,char const *tree,ssexec_t *info)
{
	stralloc sa = STRALLOC_ZERO ;
	size_t treelen = strlen(tree), pos, newlen ;
	char solve[treelen + SS_SVDIRS_LEN + SS_DB_LEN + SS_SRC_LEN + 1] ;
	newlen = treelen + SS_SVDIRS_LEN ;
	auto_strings(solve,tree,SS_SVDIRS,SS_SVC) ;
	
	if (!sastr_dir_get(&sa,solve,"",S_IFDIR))
		log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"get source svc service file of: ",tree) ;

	auto_string_from(solve,newlen,SS_DB,SS_SRC) ;

	if (!sastr_dir_get(&sa,solve,SS_MASTER + 1,S_IFDIR))
		log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"get source of atomic service file of: ",tree) ;
	
	for (pos = 0 ;pos < sa.len; pos += strlen(sa.s + pos) + 1)
	{
		char *name = sa.s + pos ;
		int logname = get_rstrlen_until(name,SS_LOG_SUFFIX) ;
		if (logname > 0) continue ;
		log_trace(DRYRUN ? drun : "","tree: ",info->treename.s," contain service: ",name) ;
		if (ss_resolve_src_path(list,name,info) < 1) 
			log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"resolve source path of: ",name) ;
	
	}
	stralloc_free(&sa) ;
}

static int run_cmdline(char const *prog,char const **add,int len,char const *const *envp)
{
	pid_t pid ;
	int wstat ;
	
	char fmt[UINT_FMT] ;
	fmt[uint_fmt(fmt, VERBOSITY)] = 0 ;
	
	int m = 4 + len, i = 0, n = 0 ;
	char const *newargv[m] ;
	newargv[n++] = prog ;
	newargv[n++] = "-v" ; 
	newargv[n++] = fmt ;
	for (;i<len;i++)
		newargv[n++] = add[i] ;
	newargv[n] = 0 ;
	
	pid = child_spawn0(newargv[0],newargv,envp) ; 
	if (waitpid_nointr(pid,&wstat, 0) < 0)
		log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"wait for: ",newargv[0]) ;
	if (wstat) return 0 ;
	
	return 1 ;
}


int main(int argc, char const *const *argv,char const *const *envp)
{
	int r ;
	unsigned int nclassic = 0, nlongrun = 0, nbsv = 0 ;
	size_t systemlen, optslen = 5, pos, len ;
	char tree_opts_create[optslen] ;
	char *fdir = 0 ;
	
	log_color = &log_color_disable ;
	
	stralloc satree = STRALLOC_ZERO ;
	stralloc allow = STRALLOC_ZERO ;
	stralloc contents = STRALLOC_ZERO ;
	ssexec_t info = SSEXEC_ZERO ;
	
	PROG = "66-update" ;	
	{
		subgetopt_t l = SUBGETOPT_ZERO ;
		for (;;)
		{
			int opt = subgetopt_r(argc,argv, "hv:l:cd", &l) ;
			
			if (opt == -1) break ;
			switch (opt)
			{
				case 'h' : 	info_help(); return 0 ;
				case 'v' :  if (!uint0_scan(l.arg, &VERBOSITY)) log_usage(USAGE) ; break ;
				case 'l' : 	if (!stralloc_cats(&info.live,l.arg)) log_die_nomem("stralloc") ;
							if (!stralloc_0(&info.live)) log_die_nomem("stralloc") ;
							break ;
				case 'c' :	log_color = !isatty(1) ? &log_color_disable : &log_color_enable ; break ;
				case 'd' :  DRYRUN = 1 ; break ;
				default	:	log_usage(USAGE) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	info.owner = getuid() ;
	
	if (!set_ownersysdir(&info.base,info.owner)) log_dieusys(LOG_EXIT_SYS, "set owner directory") ;

	char system[info.base.len + SS_SYSTEM_LEN + 1] ;
	auto_strings(system,info.base.s,SS_SYSTEM,"/") ;
	systemlen = info.base.len + SS_SYSTEM_LEN + 1 ;
	
	if (!argc)
	{
		if (!sastr_dir_get(&satree,system,SS_BACKUP + 1,S_IFDIR))
			log_dieusys(LOG_EXIT_SYS,"get list of trees at: ",system) ;

		if (!satree.len)
		{
			log_info(DRYRUN ? drun : "","No trees exist yet -- Nothing to do") ;
			goto exit ;
		}
	}
	else
	{
		int i = 0 ;
		size_t arglen = 0 ;
		for (;i < argc ; i++)
		{
			arglen = strlen(argv[i]) ;
			char tree[systemlen + arglen + 1] ;
			auto_strings(tree,system,argv[i]) ;
			r = scan_mode(tree,S_IFDIR) ;
			if (r == -1) { errno = EEXIST ; log_diesys(LOG_EXIT_SYS,"conflicting format of: ",tree) ; }
			if (!r) log_die(LOG_EXIT_USER,"tree: ",tree," doesn't exist") ;
			if (!stralloc_catb(&satree,argv[i],strlen(argv[i]) + 1))
				log_die_nomem("stralloc") ;
		}
	}
	
	len = satree.len ;
	for (pos = 0 ; pos < len; pos += strlen(satree.s + pos) + 1)
	{
		int dbok = 0 ;
		
		nclassic = nlongrun = nbsv = 0 ;
		info.base.len = info.tree.len = info.treename.len = 0 ;
		allow.len = WORKDIR.len = contents.len = 0 ;
		auto_strings(tree_opts_create,"-n") ;
		optslen = 2 ;
		
		if (!auto_stra(&info.tree,satree.s + pos))
			log_die_nomem("stralloc") ;
 		
		set_ssinfo(&info) ;
		
		char tmp[systemlen + SS_BACKUP_LEN + info.treename.len + SS_SVDIRS_LEN + SS_DB_LEN + 1] ;
		char current[info.livetree.len + 1 + info.treename.len + 9 + 1] ;
		
		log_info(DRYRUN ? drun : "","save state of tree: ", info.treename.s) ;
		if (tree_is_current(info.base.s,info.treename.s,info.owner))
		{
			log_trace(DRYRUN ? drun : "","tree: ",info.treename.s," is marked current") ;
			auto_string_from(tree_opts_create,optslen,"c") ;
			optslen +=1 ;
		}
		if (tree_is_enabled(info.treename.s) == 1)
		{
			log_trace(DRYRUN ? drun : "","tree: ",info.treename.s," is marked enabled") ;
			auto_string_from(tree_opts_create,optslen,"E") ;
			optslen +=1 ;
		}
		
		tree_allowed(&allow,info.base.s,info.treename.s) ;
		
		log_info(DRYRUN ? drun : "","save service(s) list of tree: ", info.treename.s) ;
		tree_contents(&contents,info.tree.s,&info) ;
		
		dbok = db_ok(info.livetree.s, info.treename.s) ;
		
		if (dbok)
		{
			fdir = 0 ;
			
			log_trace(DRYRUN ? drun : "","find current source of live db: ",info.livetree.s,"/",info.treename.s) ;
			r = db_find_compiled_state(info.livetree.s,info.treename.s) ;
			if (r == -1) log_die(LOG_EXIT_SYS,"inconsistent state of: ",info.livetree.s) ;
			if (r == 1) {
				auto_strings(tmp,system,SS_BACKUP + 1,"/",info.treename.s,SS_DB) ;
			}
			else auto_strings(tmp,system,info.treename.s,SS_SVDIRS,SS_DB) ;

			log_trace(DRYRUN ? drun : "","created temporary directory at: /tmp") ;
			fdir = dir_create_tmp(&WORKDIR,"/tmp",info.treename.s) ;
			if (!fdir) log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"create temporary directory") ;
			
			log_trace(DRYRUN ? drun : "","copy contents of: ",tmp," to: ",WORKDIR.s) ;
			if (!hiercopy(tmp,fdir)) log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"copy: ",tmp," to: ",WORKDIR.s) ;
			
			char new[WORKDIR.len + 1 + info.treename.len + 1] ;
					
			auto_strings(current,info.livetree.s,"/",info.treename.s,"/compiled") ;
			auto_strings(new,WORKDIR.s,"/",info.treename.s) ;
			
			log_info(DRYRUN ? drun : "","update ",info.livetree.s,"/",info.treename.s," to: ",new) ;
			
			if (!DRYRUN)
				if (!atomic_symlink(new, current, PROG))
					log_dieusys(LOG_EXIT_SYS,"update: ",current," to: ", new) ;
		
			if (DRYRUN) cleanup() ;
		}
		
		/** finally we can destroy the tree and recreate it*/
		// destroy
		{
			char const *t[] = { "-l", info.live.s, "-R", info.treename.s } ;
			if (DRYRUN)
			{
				log_info(drun,"66-tree -l ",info.live.s," -R ",info.treename.s) ; 
			}
			else
			{
				if (!run_cmdline(SS_EXTBINPREFIX "66-tree",t,4,envp))
					log_dieu(LOG_EXIT_SYS,"delete tree: ", info.treename.s) ;
			}
		}
		//create
		{
			char const *t[] = { "-l", info.live.s, tree_opts_create,"-a",allow.s, info.treename.s } ;
			if (DRYRUN)
			{
				log_info(drun,"66-tree -l ",info.live.s," ",tree_opts_create," -a ",allow.s," ",info.treename.s) ; 
			}
			else
			{
				if (!run_cmdline(SS_EXTBINPREFIX "66-tree",t,6,envp))
					log_dieu(LOG_EXIT_SYS,"create tree: ", info.treename.s) ;
			}
		}
		/* we must reimplement the enable process instead of
		 * using directly 66-enable. The 66-enable program will use
		 * is own workdir with an empty tree. At call of db_update(),
		 * the service already running will be brought down. We don't 
		 * want this behavior. A nope update is necessary.
		 * So remake the things with the enable API */
		
		if (contents.len)
		{
			stralloc tostart = STRALLOC_ZERO ;
			log_info(DRYRUN ? drun : "","enable service(s) of tree: ",info.treename.s) ;
			if (!DRYRUN)
			{
				auto_strings(tmp,system,info.treename.s,SS_SVDIRS) ;
			}
			else
			{
				WORKDIR.len = 0 ;
				log_trace(drun,"copy: ", info.tree.s,SS_SVDIRS," to a temporary directory") ;
				if (!tree_copy(&WORKDIR,info.tree.s,info.treename.s)) 
					log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"create tmp working directory") ;
				/* we need to remove the contain of .resolve directory
				 * The write process will try to read it and obviously it
				 * get a wrong information*/
				auto_strings(tmp,WORKDIR.s,SS_RESOLVE) ;
				log_trace(drun,"remove directory: ", tmp) ;
				if (rm_rf(tmp) < 0) log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"remove: ",tmp) ;
				log_trace(drun,"create directory: ", tmp) ;
				if (!dir_create_parent(tmp,0755))
					log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"create directory: ",tmp) ;
				auto_string_from(tmp,0,WORKDIR.s) ;
				
			}
			start_parser(&contents,&info,&nbsv,DRYRUN ? 1 : 0) ;
			start_write(&tostart,&nclassic,&nlongrun,tmp,&gasv,&info,DRYRUN ? 1 : 0,0) ;
			/** we don't care about nclassic. Classic service are copies
			 * of original and we retrieve the original at the end of the
			 * process*/
			if(nlongrun)
			{
				ss_resolve_graph_t graph = RESOLVE_GRAPH_ZERO ;
				r = ss_resolve_graph_src(&graph,tmp,0,1) ;
				if (!r)
					log_dieu(LOG_EXIT_SYS,"resolve source of graph for tree: ",info.treename.s) ;
				
				r = ss_resolve_graph_publish(&graph,0) ;
				if (r <= 0) 
				{
					if (r < 0) log_die(LOG_EXIT_USER,"cyclic graph detected") ;
					log_dieusys(LOG_EXIT_SYS,"publish service graph") ;
				}
				
				if (!ss_resolve_write_master(&info,&graph,tmp,0))
					log_dieusys(LOG_EXIT_SYS,"update inner bundle") ;
				
				ss_resolve_graph_free(&graph) ;
				if (!db_compile(tmp,info.tree.s,info.treename.s,envp))
					log_dieu(LOG_EXIT_SYS,"compile ",tmp,"/",info.treename.s) ;
			}
			stralloc_free(&tostart) ;
			freed_parser() ;
			genalloc_free(sv_alltype,&gasv) ;
			
		}
		
		if (dbok)
		{
			auto_strings(tmp,system,info.treename.s,SS_SVDIRS,SS_DB) ;
			log_info(DRYRUN ? drun : "","update db: ",info.livetree.s,"/",info.treename.s, " to: ",tmp,"/",info.treename.s) ;
			/* Be paranoid here and use db_update instead of atomic_symlink.
			 * db_update() allow to make a running test and to see
			 * if the db match exactly the same state.
			 * We prefer to die in this case instead of leaving an
			 * inconsistent state. */
			if (!DRYRUN)
				if (!db_update(tmp,&info,envp))
					log_dieu(LOG_EXIT_SYS,"update: ",info.livetree.s,"/",info.treename.s," to: ", tmp,"/",info.treename.s) ;
		}

		cleanup() ;	
		
		log_info(DRYRUN ? drun : "","tree: ",info.treename.s," updated successfully") ;
	}
	exit:
	stralloc_free(&satree) ;
	stralloc_free(&allow) ;
	stralloc_free(&WORKDIR) ;
	stralloc_free(&contents) ;
	ssexec_free(&info) ;
	return 0 ;
	
}
	
