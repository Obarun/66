/* 
 * 66-scandir.c
 * 
 * Copyright (c) 2018-2019 Eric Vidal <eric@obarun.org>
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
#include <errno.h>
#include <sys/stat.h>

#include <oblibs/obgetopt.h>
#include <oblibs/error2.h>
#include <oblibs/directory.h>
#include <oblibs/types.h>
#include <oblibs/files.h>
#include <oblibs/string.h>

#include <skalibs/buffer.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/types.h>
#include <skalibs/env.h>
#include <skalibs/bytestr.h>//byte_count

#include <s6/config.h>
#include <execline/config.h>

#include <66/config.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/environ.h>

//#include <stdio.h>

#define CRASH 0
#define FINISH 1
#define HUP 2
#define INT 3
#define QUIT 4
#define TERM 5
#define USR1 6
#define USR2 7

#define SIGSIZE 64
#define AUTO_CRTE_CHW 1
#define AUTO_CRTE_CHW_CHM 2
#define PERM1777 S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO

static uid_t OWNER ;
static char OWNERSTR[UID_FMT] ;
static gid_t GIDOWNER ;
static char GIDSTR[GID_FMT] ;
static char TMPENV[MAXENV+1] ;
static char const *skel = SS_DATA_SYSDIR ;
static char const *log_user = "root" ;
static unsigned int BOOT = 0 ;
unsigned int VERBOSITY = 1 ;

#define USAGE "66-scandir [ -h ] [ -v verbosity ] [ -b ] [ -l live ] [ -t rescan ] [ -L log_user ] [ -s skel ] [ -e environment ] [ -c | u | r ] owner"

static inline void info_help (void)
{
  static char const *help =
"66-scandir <options> owner\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-v: increase/decrease verbosity\n"
"	-b: create scandir for a boot process\n"
"	-l: live directory\n"
"	-t: rescan scandir every milliseconds\n"
"	-L: run catch-all logger as log_user user\n"
"	-s: skeleton directory\n"
"	-e: directory environment\n"
"	-c: create scandir\n"
"	-r: remove scandir\n"
"	-u: bring up scandir\n"
;

	if (buffer_putsflush(buffer_1, help) < 0)
		strerr_diefu1sys(111, "write to stdout") ;
}

void scandir_up(char const *scandir, unsigned int timeout, char const *const *envp)
{
	int r ;
	r = scandir_ok(scandir) ;
	if (r < 0) strerr_diefu2sys(111, "check: ", scandir) ;
	if (r)
	{
		VERBO2 strerr_warni3x("scandir: ",scandir," already running") ;
		return ;
	}
	
	char const *newup[6] ;
	unsigned int m = 0 ;
	char fmt[UINT_FMT] ;
	fmt[uint_fmt(fmt, timeout)] = 0 ;
	
	newup[m++] = S6_BINPREFIX "s6-svscan" ;
	newup[m++] = "-st" ;
	newup[m++] = fmt ;
	newup[m++] = "--" ;
	newup[m++] = scandir ;
	newup[m++] = 0 ;
	VERBO2 strerr_warni3x("Starts scandir ",scandir," ...") ;
	xpathexec_run (newup[0], newup, envp) ;
}

static void inline auto_chown(char const *str)
{
	VERBO3 strerr_warnt6x("chown directory: ",str," to: ",OWNERSTR,":",GIDSTR) ;
	if (chown(str,OWNER,GIDOWNER) < 0)
		strerr_diefu2sys(111,"chown: ",str) ;
}

static void inline auto_dir(char const *str,mode_t mode)
{
	VERBO3 strerr_warnt2x("create directory: ",str) ;
	if (!dir_create(str,mode)) 
		strerr_diefu2sys(111,"create directory: ",str) ;
}

static void inline auto_chmod(char const *str,mode_t mode)
{
	//VERBO3 strerr_warnt2x("chmod: ",str) ;
	if (chmod(str,mode) < 0)
		strerr_diefu2sys(111,"chmod: ",str) ;
}

static void inline auto_file(char const *dst,char const *file,char const *contents,size_t conlen)
{
	VERBO3 strerr_warnt4x("write file: ",dst,"/",file) ;
	if (!file_write_unsafe(dst,file,contents,conlen))
		strerr_diefu4sys(111,"write file: ",dst,"/",file) ;
}

static void inline auto_stralloc(stralloc *sa,char const *str)
{
	if (!stralloc_cats(sa,str)) strerr_diefu1sys(111,"append stralloc") ;
}

static void inline auto_check(char const *str,mode_t check,mode_t type,mode_t perm,int what)
{
	int r ;
	r = scan_mode(str,check) ;
	if (r < 0) { errno = EEXIST ; strerr_dief2sys(111,"conflicting format of: ",str) ; }
	if (!r)
	{
		auto_dir(str,type) ;
		if (what > 0) auto_chown(str) ;
		if (what > 1) auto_chmod(str,perm) ;
	}
}

static void inline auto_fifo(char const *str)
{
	int r ;
	r = scan_mode(str,S_IFIFO) ;
	if (r < 0) { errno = EEXIST ; strerr_dief2sys(111,"conflicting format of: ",str) ; }
	if (!r)
	{
		VERBO3 strerr_warnt2x("create fifo: ",str) ;
		if (mkfifo(str, 0600) < 0) 
			strerr_diefu2sys(111,"create fifo: ",str) ;
	}
}
static void inline auto_rm(char const *str)
{
	int r ;
	r = scan_mode(str,S_IFDIR) ;
	if (r > 0)
	{
		VERBO2 strerr_warni3x("removing ",str," ...") ;
		if (rm_rf(str) < 0) strerr_diefu2sys(111,"remove: ",str) ;
	}
}
static void inline log_perm(char const *str,uid_t *uid,gid_t *gid)
{
	if (!youruid(uid,str)) strerr_diefu2sys(111,"set uid of: ",str) ;
	if (!yourgid(gid,*uid)) strerr_diefu2sys(111,"set gid of: ",str) ;
}

static void inline auto_addlive(stralloc *sa,char const *live, char const *str)
{
	auto_stralloc(sa,live) ;
	auto_stralloc(sa,str) ;
}

void write_shutdownd(char const *live, char const *scandir)
{
	stralloc run = STRALLOC_ZERO ;
	size_t scandirlen = strlen(scandir) ;
	char shut[scandirlen + 1 + SS_BOOT_SHUTDOWND_LEN + 5 + 1] ;
	memcpy(shut,scandir,scandirlen) ;
	shut[scandirlen] = '/' ;
	memcpy(shut + scandirlen + 1,SS_BOOT_SHUTDOWND,SS_BOOT_SHUTDOWND_LEN) ;	
	shut[scandirlen + 1 + SS_BOOT_SHUTDOWND_LEN] = 0 ;	
	auto_check(shut,S_IFDIR,0755,0755,AUTO_CRTE_CHW_CHM) ;
	memcpy(shut + scandirlen + 1 + SS_BOOT_SHUTDOWND_LEN,"/fifo",5) ;
	shut[scandirlen + 1 + SS_BOOT_SHUTDOWND_LEN + 5] = 0 ;
	auto_fifo(shut) ;
	auto_stralloc(&run, 
		"#!" EXECLINE_SHEBANGPREFIX "execlineb -P\n" \
		SS_BINPREFIX "66-shutdownd -l ") ;
	auto_stralloc(&run,live) ;
	auto_stralloc(&run," -s ") ;
	auto_stralloc(&run,skel) ;
	auto_stralloc(&run," -g 3000\n") ;
	shut[scandirlen + 1 + SS_BOOT_SHUTDOWND_LEN] = 0 ;
	auto_file(shut,"run",run.s,run.len) ;
	memcpy(shut + scandirlen + 1 + SS_BOOT_SHUTDOWND_LEN,"/run",4) ;
	shut[scandirlen + 1 + SS_BOOT_SHUTDOWND_LEN + 4] = 0 ;
	auto_chmod(shut,0755) ;
	stralloc_free(&run) ;
}

void write_bootlog(char const *live, char const *scandir) 
{
	int r ;
	uid_t uid ;
	gid_t gid ;
	size_t livelen = strlen(live) ;
	size_t scandirlen = strlen(scandir) ;
	size_t ownerlen = uid_fmt(OWNERSTR,OWNER) ;
 	size_t loglen ;
	char path[livelen + 4 + ownerlen + 1] ;
	char logdir[scandirlen + SS_SCANDIR_LEN + SS_LOG_SUFFIX_LEN + 1 + 5 + 1] ;
	
	stralloc run = STRALLOC_ZERO ;
	/** run/66/scandir/uid_name/scandir-log */
	memcpy(logdir,scandir,scandirlen) ;
	memcpy(logdir + scandirlen,"/" SS_SCANDIR SS_LOG_SUFFIX,SS_SCANDIR_LEN + SS_LOG_SUFFIX_LEN + 1) ;
	loglen = scandirlen + SS_SCANDIR_LEN + SS_LOG_SUFFIX_LEN + 1 ; 
	logdir[loglen] = 0 ;
	auto_check(logdir,S_IFDIR,0755,0,AUTO_CRTE_CHW) ;
	
	/** make the fifo*/
	memcpy(logdir + loglen, "/fifo", 5) ;
	logdir[loglen + 5] = 0 ;
	auto_fifo(logdir) ;
		
	/** set the log path for the run file
	 * /run/66/log*/
	memcpy(path,live,livelen) ;
	memcpy(path+livelen,"log",3) ;
	path[livelen + 3] = 0 ;
	log_perm(log_user,&uid,&gid) ;
	VERBO3 strerr_warnt4x("create directory: ",path,"/",OWNERSTR) ;
	r = dir_create_under(path,OWNERSTR,02750) ;
	if (r < 0) strerr_diefu3sys(111,"create: ",path,OWNERSTR) ;
	/** chown /run/66/log/<uid>*/
	path[livelen + 3] = '/' ;
	memcpy(path + livelen + 4,OWNERSTR,ownerlen) ;
	path[livelen + 4 + ownerlen] = 0 ;
	if (chown(path,uid,gid) < 0)
		strerr_diefu2sys(111,"chown: ",path) ;
	auto_chmod(path,02755) ;

	/** make run file */
	auto_stralloc(&run, 
		"#!" EXECLINE_SHEBANGPREFIX "execlineb -P\n" \
		EXECLINE_BINPREFIX "redirfd -w 2 /dev/console\n" \
		EXECLINE_BINPREFIX "redirfd -w 1 /dev/null\n" \
		EXECLINE_BINPREFIX "redirfd -rnb 0 fifo\n" \
		S6_BINPREFIX "s6-setuidgid ") ;
	auto_stralloc(&run,log_user) ;
	auto_stralloc(&run,"\n" S6_BINPREFIX "s6-log -bpd3 -- 1 t ") ;
	auto_stralloc(&run,path) ;
	auto_stralloc(&run,"\n") ;
	
	logdir[loglen] = 0 ;
	
	auto_file(logdir,"run",run.s,run.len) ;
	auto_file(logdir,"notification-fd","3\n",2) ;
	
	memcpy(logdir + loglen,"/run",4) ;
	logdir[loglen + 4] = 0 ;	
	auto_chmod(logdir,0755) ;
	
	stralloc_free(&run) ;
}

void write_control(char const *scandir,char const *live, char const *filename, int file)
{
	size_t scandirlen = strlen(scandir) ;
	size_t filen = strlen(filename) ;
	char mode[scandirlen + SS_SVSCAN_LOG_LEN + filen + 1] ;
	stralloc sa = STRALLOC_ZERO ;

	/** shebang */
	auto_stralloc(&sa, "#!" EXECLINE_SHEBANGPREFIX "execlineb -P\n") ;
	
	if (file == FINISH)
	{
		if (BOOT)
		{
			auto_stralloc(&sa,
				EXECLINE_BINPREFIX "redirfd -w 1 /dev/console\n" \
				EXECLINE_BINPREFIX "fdmove -c 2 1\n" \
				EXECLINE_BINPREFIX "foreground { " SS_BINPREFIX "66-echo -- \"scandir ") ;
			auto_stralloc(&sa,scandir) ;
			auto_stralloc(&sa,
				" exited. Rebooting.\" }\n" \
				SS_BINPREFIX "66-hpr -r -f -l ") ;
			auto_addlive(&sa,live,"\n") ;
		}
		else
		{
			auto_stralloc(&sa,SS_BINPREFIX "66-echo -- \"scandir ") ;
			auto_stralloc(&sa, scandir) ;
			auto_stralloc(&sa, " shutted down...\"\n") ;
		}
		goto write ;
	}
	
	if (file == CRASH)
	{
		auto_stralloc(&sa,
			EXECLINE_BINPREFIX "redirfd -w 1 /dev/console\n" \
			EXECLINE_BINPREFIX "fdmove -c 2 1\n" \
			EXECLINE_BINPREFIX "foreground { " SS_BINPREFIX "66-echo -- \"scandir ") ;
		auto_stralloc(&sa,scandir) ;
		auto_stralloc(&sa, " crashed.") ;
		if (BOOT)
		{
			auto_stralloc(&sa,
				" Rebooting.\" }\n" \
				SS_BINPREFIX "66-hpr -r -f -l ") ;
			auto_addlive(&sa,live,"\n") ;
		}
		else auto_stralloc(&sa,"\" }\n") ;
		goto write ;
	}
	if (!BOOT)
	{
		auto_stralloc(&sa, EXECLINE_BINPREFIX "foreground { " SS_BINPREFIX "66-all -v3 -l ") ;
		auto_addlive(&sa,live," down }\n") ;
	}
	switch(file)
	{
		case USR1:
			if (BOOT)
			{
				auto_stralloc(&sa,SS_BINPREFIX "66-shutdown -a -p -l ") ;
				auto_addlive(&sa,live," -- now\n") ;
			}
			break ;
		case USR2: 
			if (BOOT)
			{
				auto_stralloc(&sa,SS_BINPREFIX "66-shutdown -a -h -l ") ;
				auto_addlive(&sa,live," -- now\n") ;
			}
			break ;
		case TERM:
			if (!BOOT)
			{
				auto_stralloc(&sa, S6_BINPREFIX "66-scanctl -l ") ;
				auto_addlive(&sa,live," t\n") ;
			}
			break ;
		case QUIT:
			if (!BOOT) auto_stralloc(&sa, SS_BINPREFIX "66-scanctl quit\n") ;
			break ;
		case INT:
			if (BOOT)
			{
				auto_stralloc(&sa,SS_BINPREFIX "66-shutdown -a -r -l ") ;
				auto_addlive(&sa,live," -- now\n") ;
			}
			break ;
		case HUP:
			break ;
		default:
			break ;
	}
	
	write:
		memcpy(mode,scandir,scandirlen) ;
		memcpy(mode + scandirlen, SS_SVSCAN_LOG, SS_SVSCAN_LOG_LEN) ;
		mode[scandirlen + SS_SVSCAN_LOG_LEN ] = 0 ;
		auto_file(mode,filename+1,sa.s,sa.len) ;	
		memcpy(mode + scandirlen + SS_SVSCAN_LOG_LEN,filename,filen) ;
		mode[scandirlen + SS_SVSCAN_LOG_LEN + filen] = 0 ;
		auto_chmod(mode,0755) ;
			
	stralloc_free(&sa) ;
}

void create_scandir(char const *live, char const *scandir)
{
	size_t scanlen = strlen(scandir) ;
	char tmp[scanlen + 11 + 1] ;
	
	/** run/66/scandir/<uid> */	
	memcpy(tmp,scandir,scanlen) ;
	tmp[scanlen] = 0 ;
	auto_check(tmp,S_IFDIR,0755,0,AUTO_CRTE_CHW) ;
	
	/** run/66/scandir/name/.svscan */
	memcpy(tmp + scanlen, SS_SVSCAN_LOG, SS_SVSCAN_LOG_LEN) ;
	tmp[scanlen + SS_SVSCAN_LOG_LEN] = 0 ;
	auto_check(tmp,S_IFDIR,0755,0,AUTO_CRTE_CHW) ;
	
	char const *const file[] = 
	{ 
		"/crash", "/finish", "/SIGHUP", "/SIGINT",
		"/SIGQUIT", "/SIGTERM", "/SIGUSR1", "/SIGUSR2"
	 } ;
	VERBO2 strerr_warni1x("write control file... ") ;
	for (int i = 0 ; i < 8; i++)
		write_control(scandir,live,file[i],i) ;

	if (BOOT)
	{
		write_bootlog(live, scandir) ;
		write_shutdownd(live, scandir) ;
	}
}

void sanitize_live(char const *live)
{
	size_t livelen = strlen(live) ;	
	char tmp[livelen + SS_SCANDIR_LEN + 1] ;
	
	/** run/66 */
	auto_check(live,S_IFDIR,0755,0,AUTO_CRTE_CHW) ;
	
	/** run/66/scandir */
	memcpy(tmp,live,livelen) ;
	memcpy(tmp + livelen,SS_SCANDIR,SS_SCANDIR_LEN) ;
	tmp[livelen + SS_SCANDIR_LEN] = 0 ;
	auto_check(tmp,S_IFDIR,0755,PERM1777,AUTO_CRTE_CHW_CHM) ;
		
	/** run/66/tree */
	memcpy(tmp + livelen,SS_TREE,SS_TREE_LEN) ;
	tmp[livelen + SS_TREE_LEN] = 0  ;
	auto_check(tmp,S_IFDIR,0755,PERM1777,AUTO_CRTE_CHW_CHM) ;

	/** run/66/log */
	memcpy(tmp + livelen,SS_LOG,SS_LOG_LEN) ;
	tmp[livelen + SS_LOG_LEN] = 0  ;
	auto_check(tmp,S_IFDIR,0755,PERM1777,AUTO_CRTE_CHW_CHM) ;
	
	/** /run/66/state*/
	memcpy(tmp + livelen,SS_STATE+1,SS_STATE_LEN-1) ;
	tmp[livelen + SS_STATE_LEN-1] = 0  ;
	auto_check(tmp,S_IFDIR,0755,PERM1777,AUTO_CRTE_CHW_CHM) ;
}

int main(int argc, char const *const *argv, char const *const *envp)
{
	int r ;
	unsigned int up, rescan, create, remove ;
	
	stralloc live = STRALLOC_ZERO ;
	stralloc scandir = STRALLOC_ZERO ;
	stralloc envdir = STRALLOC_ZERO ;
	
	char const *newenv[MAXENV+1] ;
	char const *const *genv = 0 ;
	
	up = rescan = create = remove = 0 ;
	
	PROG = "66-scandir" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">hv:bl:t:s:e:cruL:", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'h' : info_help(); return 0 ;
				case 'v' : if (!uint0_scan(l.arg, &VERBOSITY)) exitusage(USAGE) ; break ;
				case 'b' : BOOT = 1 ; break ;
				case 'l' : if(!stralloc_cats(&live,l.arg)) retstralloc(111,"main") ;
						   if(!stralloc_0(&live)) retstralloc(111,"main") ;
						   break ;
				case 't' : if (!uint0_scan(l.arg, &rescan)) break ;
				case 's' : skel = l.arg ; break ;
				case 'e' : if(!stralloc_cats(&envdir,l.arg)) retstralloc(111,"main") ;
						   if(!stralloc_0(&envdir)) retstralloc(111,"main") ;
						   break ;
				case 'c' : create = 1 ; if (remove) exitusage(USAGE) ; break ;
				case 'r' : remove = 1 ; if (create) exitusage(USAGE) ; break ;
				case 'u' : up = 1 ; if (remove) exitusage(USAGE) ; break ;
				case 'L' : log_user = l.arg ; break ;
				default : exitusage(USAGE) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (!argc) OWNER = MYUID ;
	else if (!youruid(&OWNER,argv[0])) strerr_diefu2sys(111,"set uid of: ",argv[0]) ;
	
	if (BOOT && OWNER) strerr_dief1x(110,"-b options can be set only with root") ; 
	OWNERSTR[uid_fmt(OWNERSTR,OWNER)] = 0 ;
	
	if (!yourgid(&GIDOWNER,OWNER)) strerr_diefu2sys(111,"set gid of: ",OWNERSTR) ;
	GIDSTR[gid_fmt(GIDSTR,GIDOWNER)] = 0 ; 
		
	/** live -> /run/66/ */
	r = set_livedir(&live) ;
	if (r < 0) strerr_dief3x(110,"live: ",live.s," must be an absolute path") ;
	if (!r) strerr_diefu1sys(111,"set live directory") ;
	
	if (!stralloc_copy(&scandir,&live)) retstralloc(111,"main") ;
		
	/** scandir -> /run/66/scandir/ */
	r = set_livescan(&scandir,OWNER) ;
	if (r < 0) strerr_dief3x(110,"scandir: ", scandir.s, " must be an absolute path") ;
	if (!r) strerr_diefu1sys(111,"set scandir directory") ;
		
	if (BOOT && skel[0] != '/')
		strerr_dief3x(110, "rc.shutdown: ",skel," must be an absolute path") ;
	
	if (envdir.len)
	{
		if (envdir.s[0] != '/')
			strerr_dief3x(110,"environment: ",envdir.s," must be an absolute path") ;
		
		if (!build_env(envdir.s,envp,newenv,TMPENV)) strerr_diefu2x(111,"build environment with: ",envdir.s) ;
		genv = newenv ;
	}
	else genv = envp ;
	
	r = scan_mode(scandir.s, S_IFDIR) ;
	if (r < 0) strerr_dief3x(111,"scandir: ",scandir.s," exist with unkown mode") ;
	if (!r && !create && !remove) strerr_dief3x(110,"scandir: ",scandir.s," doesn't exist") ;
	if (!r && create)
	{
		VERBO2 strerr_warni3x("sanitize ",live.s," ...") ;
		sanitize_live(live.s) ;
		VERBO2 strerr_warni3x("create scandir ",scandir.s," ...") ;
		create_scandir(live.s, scandir.s) ;
	}
	/**swap to char [] to be able to freed stralloc*/
	char ownerscan[scandir.len + 1] ;
	memcpy(ownerscan,scandir.s,scandir.len) ;
	ownerscan[scandir.len] = 0 ;
	if (r && create)
	{
		VERBO1 strerr_warni3x("scandir: ",scandir.s," already exist, keep it") ;
		goto end ;
	}
 	
 	r = scandir_ok(scandir.s) ;
	if (r < 0) strerr_diefu2sys(111, "check: ", scandir.s) ;
	if (r && remove) strerr_diefu3x(110,"remove: ",scandir.s,": is running")  ;
	if (remove)
	{
		auto_rm(scandir.s) ;
		/** /run/66/tree/uid */
		if (!stralloc_copy(&scandir,&live)) retstralloc(111,"main") ;
		r = set_livetree(&scandir,OWNER) ;
		if (!r) strerr_diefu1sys(111,"set livetree directory") ;
		auto_rm(scandir.s) ;
		if (!stralloc_copy(&scandir,&live)) retstralloc(111,"main") ;
		/** run/66/state/uid */
		r = set_livestate(&scandir,OWNER) ;
		if (!r) strerr_diefu1sys(111,"set livestate directory") ;
		auto_rm(scandir.s) ;
	}
	end:
	stralloc_free(&scandir) ;
	stralloc_free(&live) ;
	stralloc_free(&envdir) ;
	
	if (up) scandir_up(ownerscan,rescan,genv) ;
		
	return 0 ;
}
