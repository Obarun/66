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
#include <sys/reboot.h>

#include <oblibs/log.h>
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/obgetopt.h>
#include <oblibs/environ.h>
#include <oblibs/sastr.h>

#include <skalibs/buffer.h>
#include <skalibs/djbunix.h>
#include <skalibs/stralloc.h>
#include <skalibs/types.h>

#include <66/config.h>
#include <66/constants.h>

static mode_t mask = SS_BOOT_UMASK ;
static unsigned int rescan = SS_BOOT_RESCAN ;
static char const *skel = SS_SKEL_DIR ;
static char *live = SS_LIVE ;
static char const *path = SS_BOOT_PATH ;
static char const *tree = SS_BOOT_TREE ;
static char const *rcinit = SS_SKEL_DIR SS_BOOT_RCINIT ;
static char const *banner = "\n[Starts stage1 process...]" ;
static char const *slashdev = 0 ;
static char const *envdir = 0 ;
static char const *fifo = 0 ;
static char const *log_user = SS_LOGGER_RUNNER ;
static char const *cver = 0 ;
static char tpath[MAXENV+1] ;
static char trcinit[MAXENV+1] ;
static char tlive[MAXENV+1] ;
static char ttree[MAXENV+1] ;
static char confile[MAXENV+1] ;
static char const *const *genv = 0 ;
static int fdin ;

#define USAGE "66-boot [ -h ] [ -m ] [ -s skel ] [ -l log_user ] [ -e environment ] [ -d dev ] [ -b banner ]"

static void sulogin(char const *msg,char const *arg)
{
	static char const *const newarg[2] = { SS_EXTBINPREFIX "sulogin" , 0 } ;
	pid_t pid ;
	int wstat ;
	fd_close(0) ;
	if (dup2(fdin,0) == -1) log_dieu(LOG_EXIT_SYS,"duplicate stdin -- you are on your own") ;
	fd_close(fdin) ;	
	if (*msg) log_warnu(msg,arg) ;
	pid = child_spawn0(newarg[0],newarg,genv) ;
	if (waitpid_nointr(pid,&wstat, 0) < 0)
			log_dieusys(LOG_EXIT_SYS,"wait for sulogin -- you are on your own") ;
	fdin=dup(0) ;
	if (fdin == -1) log_dieu(LOG_EXIT_SYS,"duplicate stdin -- you are on your own") ;
	fd_close(0) ;
	if (open("/dev/null",O_WRONLY)) log_dieu(LOG_EXIT_SYS,"open /dev/null -- you are on your own") ;
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
"	-s: skeleton directory\n"
"	-e: environment directory or file\n"
"	-d: dev directory\n"
"	-b: banner to display\n"
;

 if (buffer_putsflush(buffer_1, help) < 0) sulogin("","") ;
}

static void parse_conf(void)
{
	static char const *valid[] = 
	{ "VERBOSITY", "PATH", "LIVE", "TREE", "RCINIT", "UMASK", "RESCAN", 0 } ;
	int r ;
	unsigned int j = 0 ;
	stralloc src = STRALLOC_ZERO ;
	stralloc val = STRALLOC_ZERO ;
	if (skel[0] != '/') sulogin("skeleton directory must be an aboslute path: ",skel) ;
	size_t skelen = strlen(skel) ;
	memcpy(confile,skel,skelen) ;
	confile[skelen] = '/' ;
	memcpy(confile + skelen + 1, SS_BOOT_CONF, SS_BOOT_CONF_LEN) ;
	confile[skelen + 1 + SS_BOOT_CONF_LEN] = 0 ;
	size_t filesize=file_get_size(confile) ;
	r = openreadfileclose(confile,&src,filesize) ;
	if(!r) sulogin("open configuration file: ",confile) ; 
	if (!stralloc_0(&src)) sulogin("append stralloc of file: ",confile) ;
	
	for (char const *const *p = valid;*p;p++,j++)
	{
		if (!stralloc_copy(&val,&src)) sulogin("copy stralloc of file: ",confile) ;
		if (!environ_get_val_of_key(&val,*p)) continue ;
		/** value may be empty, in this case we use the default one */
		if (!sastr_clean_element(&val)) 
		{	
			log_warnu("get value of: ",*p," -- keeps the default") ;
			continue ;
		}
		if (!sastr_rebuild_in_oneline(&val)) sulogin("rebuild line of value: ",val.s) ;
		if (!stralloc_0(&val)) sulogin("append stralloc of value: ",val.s) ;
		
		switch (j)
		{
			case 0: if (!uint0_scan(val.s, &VERBOSITY)) sulogin("parse VERBOSITY value: ",val.s) ; 
					break ;
			case 1: memcpy(tpath,val.s,val.len) ;
					tpath[val.len] = 0 ;
					path = tpath ;
					break ;
			case 2: memcpy(tlive,val.s,val.len) ;
					tlive[val.len] = 0 ;
					live = tlive ;
					if (live[0] != '/') sulogin ("LIVE must be an absolute path: ",val.s) ; 
					break ;
			case 3: memcpy(ttree,val.s,val.len) ;
					ttree[val.len] = 0 ;
					tree = ttree ;
					break ;
			case 4: memcpy(trcinit,val.s,val.len) ;
					trcinit[val.len] = 0 ;
					rcinit = trcinit ;
					if (rcinit[0] != '/') sulogin ("RCINIT must be an absolute path: ",val.s) ; 
					break ;
			case 5: if (!uint0_oscan(val.s, &mask)) sulogin("invalid MASK value: ",val.s) ; break ;
			case 6: if (!uint0_scan(val.s, &rescan)) sulogin("invalid RESCAN value: ",val.s) ; break ;
			default: break ;
		}
		
	}
	stralloc_free(&val) ;
	stralloc_free(&src) ;
}

static int is_mnt(char const *str)
{
	struct stat st;
	size_t slen = strlen(str) ;
	int is_not_mnt = 0 ;
	if (lstat(str,&st) < 0) sulogin("lstat: ",str) ;
	if (S_ISDIR(st.st_mode))
	{
		dev_t st_dev = st.st_dev ; ino_t st_ino = st.st_ino ;
		char p[slen+4] ;
		memcpy(p,str,slen) ;
		memcpy(p + slen,"/..",3) ;
		p[slen+3] = 0 ;
		if (!stat(p,&st))
			is_not_mnt = (st_dev == st.st_dev) && (st_ino != st.st_ino) ;
	}else return 0 ;
	return is_not_mnt ? 0 : 1 ;
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
		sulogin("open for writing fifo: ",fifo) ;
	if (fd_copy(2, 1) == -1)
		sulogin("copy stderr to stdout","") ;
	fd_close(fdin) ;
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

static inline void make_cmdline(char const *prog,char const **add,int len,char const *msg,char const *arg,char const *const *envp)
{
	
	int m = 6 + len, i = 0, n = 0 ;
	char const *newargv[m] ;
	newargv[n++] = prog ;
	newargv[n++] = "-v" ; 
	newargv[n++] = cver ;
	newargv[n++] = "-l" ;
	newargv[n++] = live ; 
	for (;i<len;i++)
		newargv[n++] = add[i] ;
	newargv[n] = 0 ;
	run_cmdline(newargv,envp,msg,arg) ;
}

int main(int argc, char const *const *argv,char const *const *envp)
{
	unsigned int r , tmpfs = 0 ;
	size_t bannerlen, livelen ;
	pid_t pid ;
	int opened = 0 ;
	char verbo[UINT_FMT] ;
	cver = verbo ;
	stralloc envmodifs = STRALLOC_ZERO ;
	genv = envp ;
	PROG = "66-boot" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">hms:e:d:b:l:", &l) ;
			if (opt == -1) break ;
			if (opt == -2) sulogin("options must be set first","") ;
			switch (opt)
			{
				case 'h' : info_help(); return 0 ;
				case 'm' : tmpfs = 1 ; break ;
				case 's' : skel = l.arg ; break ;
				case 'e' : envdir = l.arg ; break ;
				case 'd' : slashdev = l.arg ; break ;
				case 'b' : banner = l.arg ; break ;
				case 'l' : log_user = l.arg ; break ;
				default :  log_usage(USAGE) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	if (geteuid())
	{
		errno = EPERM ;
		log_diesys(LOG_EXIT_USER, "nice try, peon") ;
	}
	fdin=dup(0) ;
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
		log_info("Mount: ",slashdev) ;
		fd_close(1) ;
		fd_close(2) ;
		if (mount("dev", slashdev, "devtmpfs", MS_NOSUID | MS_NOEXEC, "") == -1)
			{ opened++ ; sulogin ("mount: ", slashdev) ; }
		
		if (open("/dev/console", O_WRONLY) ||
		fd_copy(1, 0) == -1 ||
		fd_move(2, 0) == -1) return 111 ;
	}
	if (!opened)
		if (open("/dev/null", O_RDONLY)) sulogin("open: ", "/dev/null") ;
	
	char fs[livelen + 1] ;
	split_tmpfs(fs,live) ;
	r = is_mnt(fs) ;
	
	if (!r || tmpfs)
	{
		if (r && tmpfs)
		{
			log_info("Umount: ",fs) ;
			if (umount(fs) == -1) sulogin ("umount: ",fs ) ;
		}
		log_info("Mount: ",fs) ;
		if (mount("tmpfs", fs, "tmpfs", MS_NODEV | MS_NOSUID, "mode=0755") == -1) 
			sulogin("mount: ",fs) ;
	}
	/** respect the path before run 66-xxx API*/
	if (setenv("PATH", path, 1) == -1) sulogin("set initial PATH: ",path) ;
	/** create scandir */
	{
		char const *t[] = { "-b", "-c", "-s", skel, "-L", log_user } ;
		log_info("Create live scandir at: ",live) ;
		make_cmdline(SS_EXTBINPREFIX "66-scandir",t,6,"create live scandir at: ",live,envp) ;
	}
	/** initiate earlier service */
	{
		char const *t[] = { "-t",tree,"classic" } ;
		log_info("Initiate earlier service of tree: ",tree) ;
		make_cmdline(SS_EXTBINPREFIX "66-init",t,3,"initiate earlier service of tree: ",tree,envp) ;	
	}
	
	if (envdir) {
		int e = environ_get_envfile(&envmodifs,envdir) ;
		if (e <= 0){ environ_get_envfile_error(e,envdir) ; sulogin("","") ; }
	}
	
	{
		log_info("Starts boot logger at: ",live,"/log/0") ;
		int fdr = open_read(fifo) ;
		if (fdr == -1) sulogin("open fifo: ",fifo) ;
		fd_close(1) ;
		if (open(fifo, O_WRONLY) != 1) sulogin("open fifo: ",fifo) ;
		fd_close(fdr) ;
	}
	
	/** fork and starts scandir */
	{
		static char const *newargv[7] ; 
		newargv[0] = SS_EXTBINPREFIX "66-scandir" ;
		newargv[1] = "-v" ;
		newargv[2] = verbo ;
		newargv[3] = "-l" ;
		newargv[4] = live ;
		newargv[5] = "-u" ;
		newargv[6] = 0 ; 
		char const *newenvp[2] = { 0, 0 } ;
		size_t pathlen = strlen(path) ;
		char pathvar[6 + pathlen] ;
		memcpy(pathvar, "PATH=", 5) ;
		memcpy(pathvar + 5, path, pathlen + 1) ;
		newenvp[0] = pathvar ;
		pid = fork() ;
		if (pid == -1) sulogin("fork: ",rcinit) ;
		if (!pid) run_stage2(newenvp, 2, envmodifs.s,envmodifs.len) ;
		if (reboot(RB_DISABLE_CAD) == -1) log_warnusys("trap ctrl-alt-del","") ;
		if (fd_copy(2, 1) == -1) sulogin("copy stderr to stdout","") ;
		fd_close(fdin) ;
		xpathexec_r(newargv, newenvp, 2, envmodifs.s, envmodifs.len) ;
	}
}
