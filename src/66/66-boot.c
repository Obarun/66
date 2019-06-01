/* 
 * 66-boot.c
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
 
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mount.h>

#include <oblibs/error2.h>
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/obgetopt.h>

#include <skalibs/buffer.h>
#include <skalibs/djbunix.h>
#include <skalibs/diuint32.h>
#include <skalibs/genalloc.h>
#include <skalibs/stralloc.h>
#include <skalibs/types.h>

#include <66/config.h>
#include <66/environ.h>
#include <66/constants.h>

unsigned int VERBOSITY = 1 ;
static mode_t mask = SS_BOOT_UMASK ;
static unsigned int rescan = SS_BOOT_RESCAN ;
static char const *confile = SS_BOOT_CONF ;
static char *live = SS_LIVE ;
static char const *path = SS_BOOT_PATH ;
static char const *tree = SS_BOOT_TREE ;
static char const *rcinit = SS_DATA_SYSDIR SS_BOOT_RCINIT ;
static char const *rcshut = SS_DATA_SYSDIR SS_BOOT_RCSHUTDOWN ;
static char const *banner = "\n[Starts boot process ...]" ;
static char const *slashdev = 0 ;
static char const *envdir = 0 ;
static char const *fifo = 0 ;
static char const *log = 0 ;
static char tpath[MAXENV+1] ;
static char trcinit[MAXENV+1] ;
static char trcshut[MAXENV+1] ;
static char tlive[MAXENV+1] ;
static char ttree[MAXENV+1] ;
static int fdin ;

#define USAGE "66-boot [ -h ] [ -m ] [ -f confile ] [ -l log_user ] [ -e environment ] [ -d dev ] [ -b banner ]"

static void sulogin(char const *msg,char const *arg)
{
	fd_close(0) ;
	fd_close(1) ;
	fd_close(2) ;
	dup2(fdin,0) ;
	fd_close(fdin) ;
	open("/dev/console",O_WDONLY) ;
	fd_copy(2,1) ;
	if (*msg) strerr_warnwu2sys(msg,arg) ;
	char const *newarg[] = { SS_EXTBINPREFIX "sulogin" , 0 } ;
	xpathexec (newarg) ; 
}

static inline void info_help (void)
{
  static char const *help =
"66-boot <options>\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-m: mount parent live directory\n"
"	-l: run catch-all logger as log_user user\n"
"	-f: configuration file\n"
"	-e: environment directory or file\n"
"	-d: dev directory\n"
"	-b: banner to display\n"
;

 if (buffer_putsflush(buffer_1, help) < 0) sulogin("","") ;
}

static void parse_conf(void)
{
	static char const *valid[] = 
	{ "VERBOSITY", "PATH", "LIVE", "TREE", "RCINIT", "UMASK", "RESCAN" } ;
	int r ;
	unsigned int i = 0, j = 0 ;
	stralloc src = STRALLOC_ZERO ;
	stralloc saconf = STRALLOC_ZERO ;
	genalloc gaconf = GENALLOC_ZERO ;
	size_t filesize=file_get_size(confile) ;
	r = openreadfileclose(confile,&src,filesize) ;
	if(!r) sulogin("open configuration file: ",confile) ; 
	if (!stralloc_0(&src)) sulogin("append stralloc configuration file","") ;
	
	r = env_split(&gaconf,&saconf,&src) ;
	if (!r) sulogin("parse configuration file: ",confile) ;
	
	for (;i < genalloc_len(diuint32,&gaconf) ; i++)
	{
		char *key = saconf.s + genalloc_s(diuint32,&gaconf)[i].left ;
		char *val = saconf.s + genalloc_s(diuint32,&gaconf)[i].right ;
		j = 0 ;
		for (char const *const *p = valid;*p;p++,j++)
		{
			if (!strcmp(*p,key))
			{
				switch (j)
				{
					case 0: if (!uint0_scan(val, &VERBOSITY)) sulogin("invalid VERBOSITY value: ","") ; 
							break ;
					case 1: memcpy(tpath,val,strlen(val)) ;
							tpath[strlen(val)] = 0 ;
							path = tpath ;
							break ;
					case 2: memcpy(tlive,val,strlen(val)) ;
							tlive[strlen(val)] = 0 ;
							live = tlive ;
							if (live[0] != '/') sulogin ("LIVE must be an absolute path","") ; 
							break ;
					case 3: memcpy(ttree,val,strlen(val)) ;
							ttree[strlen(val)] = 0 ;
							tree = ttree ;
							break ;
					case 4: memcpy(trcinit,val,strlen(val)) ;
							trcinit[strlen(val)] = 0 ;
							rcinit = trcinit ;
							if (rcinit[0] != '/') sulogin ("RCINIT must be an absolute path","") ; 
							break ;
					case 5: if (!uint0_oscan(val, &mask)) sulogin("invalid MASK value","") ; break ;
					case 6: if (!uint0_scan(val, &rescan)) sulogin("invalid RESCAN value","") ; break ;
					default: break ;
				}
			}
		}
	}
	genalloc_free(diuint32,&gaconf) ;
	stralloc_free(&saconf) ;
	stralloc_free(&src) ;
}

static void split_tmpfs(char *dst,char *str)
{
	size_t len = get_len_until(str+1,'/') ;
	len++ ;
	memcpy(dst,str,len) ;
	dst[len] = 0 ;
}

static inline void run_stage2 (char const *const *envp, size_t envlen, char const *modifs, size_t modiflen)
{
	
	char const *newargv[3] = { rcinit, confile, 0 } ;
	setsid() ;
	fd_close(1) ;
	if (open(fifo, O_WRONLY) != 1)  /* blocks until catch-all logger is up */
		strerr_diefu2sys(111,"open for writing fifo: ",fifo) ;
	if (fd_copy(2, 1) == -1)
		strerr_diefu1sys(111,"fd_copy stdout to stderr") ;
	xpathexec_r(newargv, envp, envlen, modifs, modiflen) ;
}
static inline void run_cmdline(char const *const *newargv, char const *const *envp, char const *msg,char const *arg)
{
	pid_t pid ;
	int wstat ;
	pid = child_spawn0(newargv[0],newargv,envp) ; ;
	if (waitpid_nointr(pid,&wstat, 0) < 0)
		sulogin("wait for: ",newargv[0]) ;
	if (wstat) sulogin(msg,arg) ;
}

int main(int argc, char const *const *argv,char const *const *envp)
{
	unsigned int tmpfs = 0 ;
	size_t bannerlen, livelen ;
	pid_t pid ;
	char verbo[UINT_FMT] ;
	stralloc envmodifs = STRALLOC_ZERO ;
	fdin = dup(0) ;
	PROG = "66-boot" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">hmf:e:d:b:", &l) ;
			if (opt == -1) break ;
			if (opt == -2) sulogin("options must be set first","") ;
			switch (opt)
			{
				case 'h' : info_help(); return 0 ;
				case 'm' : tmpfs = 1 ; break ;
				case 'f' : confile = l.arg ; break ;
				case 'e' : envdir = l.arg ; break ;
				case 'd' : slashdev = l.arg ; break ;
				case 'b' : banner = l.arg ; break ;
				case 'l' : log = l.arg ; break ;
				default : exitusage(USAGE) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	parse_conf() ;
	verbo[uint_fmt(verbo, VERBOSITY)] = 0 ;
	bannerlen = strlen(banner) ;
	livelen = strlen(live) ;
	char tfifo[livelen + 1 + SS_BOOT_LOGFIFO_LEN + 1] ;
	memcpy(tfifo,live,livelen) ;
	tfifo[livelen] = '/' ;
	memcpy(tfifo + livelen + 1,SS_BOOT_LOGFIFO,SS_BOOT_LOGFIFO_LEN) ;
	tfifo[livelen + 1 + SS_BOOT_LOGFIFO_LEN] = 0 ;
	fifo = tfifo ;
	
	allwrite(1, banner, bannerlen) ;
	allwrite(1, "\n", 2) ;
	if (chdir("/") == -1) sulogin("chdir to ","/") ;
	umask(mask) ;
	setpgid(0, 0) ;
	fd_close(0) ;
	if (slashdev)
	{
		strerr_warni2x("Mount: ",slashdev) ;
		fd_close(1) ;
		fd_close(2) ;
		if (mount("dev", slashdev, "devtmpfs", MS_NOSUID | MS_NOEXEC, "") == -1)
		{
			int e = errno ;
			open("/dev/null", O_RDONLY) ;
			open("/dev/console", O_WRONLY) ;
			fd_copy(2, 1) ;
			errno = e ;
			sulogin ("mount: ", slashdev) ;
		}
		if (open("/dev/console", O_WRONLY) ||
		fd_copy(1, 0) == -1 ||
		fd_move(2, 0) == -1) return 111 ;
	}
	if (open("/dev/null", O_RDONLY)) sulogin("open: ", "/dev/null") ;
	if (tmpfs)
	{
		char fs[livelen + 1] ;
		split_tmpfs(fs,live) ;
		strerr_warni2x("Mount: ",fs) ;
		if (umount(fs) == -1) sulogin ("umount tmpfs directory: ",fs ) ;
		if (mount("tmpfs", fs, "tmpfs", MS_NODEV | MS_NOSUID, "mode=0755") == -1) 
			sulogin("mount: ",fs) ;
	}
	
	{
		strerr_warni2x("Create live scandir at: ",live) ;
		int m = log ? 10 : 8 ;
		char const *newargv[m] ;
		newargv[0] = SS_EXTBINPREFIX "66-scandir" ;
		newargv[1] = "-v" ; 
		newargv[2] = verbo ;
		newargv[3] = "-l" ;
		newargv[4] = live ; 
		newargv[5] = "-b" ;
		newargv[6] = "-c" ;
		if (log)
		{
			newargv[7] = "-L" ;
			newargv[8] = log ;
			newargv[9] =  0 ;
		}
		else newargv[7] = 0 ;
		run_cmdline(newargv,envp,"create live scandir at: ",live) ;
	}
	
	{
		strerr_warni2x("Initiate earlier service of tree: ",tree) ;
		char const *newargv[9] ;
		newargv[0] = SS_EXTBINPREFIX "66-init" ;
		newargv[1] = "-v" ; 
		newargv[2] = verbo ;
		newargv[3] = "-l" ;
		newargv[4] = live ; 
		newargv[5] = "-t" ;
		newargv[6] = tree ;
		newargv[7] = "classic" ;
		newargv[8] = 0 ;
		run_cmdline(newargv,envp,"initiate earlier service of tree: ",tree) ;
	}
	
	if (envdir)
		if (!env_get_from_src(&envmodifs,envdir)) sulogin("get environment from: ",envdir) ;
	
	{
		strerr_warni3x("Starts boot logger at: ",live,"/log/0") ;
		int fdr = open_read(fifo) ;
		if (fdr == -1) sulogin("open fifo: ",fifo) ;
		fd_close(1) ;
		if (open(fifo, O_WRONLY) != 1) sulogin("open fifo: ",fifo) ;
		fd_close(fdr) ;
	}
		
	{
		char const *newenvp[2] = { 0, 0 } ;
		size_t pathlen = strlen(path) ;
		char pathvar[6 + pathlen] ;
		if (setenv("PATH", path, 1) == -1) sulogin("set initial PATH: ",path) ;
		memcpy(pathvar, "PATH=", 5) ;
		memcpy(pathvar + 5, path, pathlen + 1) ;
		newenvp[0] = pathvar ;
		char const *newargv[7] ;
		newargv[0] = SS_EXTBINPREFIX "66-scandir" ;
		newargv[1] = "-v" ; 
		newargv[2] = verbo ;
		newargv[3] = "-l" ;
		newargv[4] = live ; 
		newargv[5] = "-u" ;
		newargv[6] =  0 ;
		pid = fork() ;
		if (pid == -1) sulogin("fork: ",rcinit) ;
		if (!pid) run_stage2(newenvp, 2, envmodifs.s,envmodifs.len) ;
		if (fd_copy(2, 1) == -1) strerr_diefu1sys(111,"redirect output file descriptor","") ;
		strerr_warni1x("Boot completed successfully") ;
		strerr_warni1x("Supervision starts...") ;
		xpathexec_r(newargv, newenvp, 2, envmodifs.s, envmodifs.len) ;
	}
}
