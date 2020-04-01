/* 
 * 66-shutdownd.c
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
 *
 * This file is a modified copy of s6-linux-init-shutdownd.c file
 * coming from skarnet software at https://skarnet.org/software/s6-linux-init.
 * All credits goes to Laurent Bercot <ska-remove-this-if-you-are-not-a-bot@skarnet.org>
 * */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>

#include <oblibs/environ.h>
#include <oblibs/files.h>
#include <oblibs/log.h>

#include <skalibs/posixplz.h>
#include <skalibs/uint32.h>
#include <skalibs/types.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/bytestr.h>
#include <skalibs/buffer.h>
#include <skalibs/sgetopt.h>
#include <skalibs/sig.h>
#include <skalibs/tai.h>
#include <skalibs/direntry.h>
#include <skalibs/djbunix.h>
#include <skalibs/iopause.h>
#include <skalibs/skamisc.h>

#include <execline/config.h>

#include <s6/s6-supervise.h>
#include <s6/config.h>

#include <66/config.h>
#include <66/constants.h>

#define STAGE4_FILE "stage4"
#define DOTPREFIX ".66-shutdownd:"
#define DOTPREFIXLEN (sizeof(DOTPREFIX) - 1)
#define DOTSUFFIX ":XXXXXX"
#define DOTSUFFIXLEN (sizeof(DOTSUFFIX) - 1)
#define SHUTDOWND_FIFO "fifo"
static char const *conf = SS_SKEL_DIR ;
static char const *live = 0 ;
static int inns = 0 ;
static int nologger = 0 ;

#define USAGE "66-shutdownd [ -h ] [ -l live ] [ -s skel ] [ -g gracetime ] [ -C ] [ -B ]"

static inline void info_help (void)
{
  static char const *help =
"66-shutdownd <options>\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-l: live directory\n"
"	-s: skeleton directory\n"
"	-g: grace time between the SIGTERM and the SIGKILL\n"
"	-C: the system is running inside a container\n"
"	-B: the catch-all logger do not exist\n"
;

	if (buffer_putsflush(buffer_1, help) < 0)
		log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
}

static void restore_console (void)
{
	if (!inns && !nologger)
	{
		fd_close(1) ;
		if (open("/dev/console", O_WRONLY) != 1)
			log_dieusys(LOG_EXIT_SYS,"open /dev/console for writing") ;
		if (fd_copy(2, 1) < 0) log_warnusys("fd_copy") ;
	}
}

struct at_s
{
	int fd ;
	char const *name ;
} ;

static int renametemp (char const *s, mode_t mode, void *data)
{
	struct at_s *at = data ;
	(void)mode ;
	return renameat(at->fd, at->name, at->fd, s) ;
}

static int mkrenametemp (int fd, char const *src, char *dst)
{
	struct at_s at = { .fd = fd, .name = src } ;
	return mkfiletemp(dst, &renametemp, 0700, &at) ;
}

ssize_t file_get_size(const char* filename)
{
	struct stat st;
	errno = 0 ;
	if (stat(filename, &st) == -1) return -1 ;
	return st.st_size;
}

static inline void auto_conf(char *confile,size_t conflen)
{
	memcpy(confile,conf,conflen) ;
	confile[conflen] = '/' ;
	memcpy(confile + conflen + 1, SS_BOOT_CONF, SS_BOOT_CONF_LEN) ;
	confile[conflen + 1 + SS_BOOT_CONF_LEN] = 0 ;
}

static void parse_conf(char const *confile,char *rcshut,char const *key)
{
	int r ;
	stralloc src = STRALLOC_ZERO ;
	size_t filesize = file_get_size(confile) ;
	r = openreadfileclose(confile,&src,filesize) ;
	if(!r) log_dieusys(LOG_EXIT_SYS,"open configuration file: ",confile) ; 
	if (!stralloc_0(&src)) log_dieusys(LOG_EXIT_SYS,"append stralloc configuration file") ;
	
	if (environ_get_val_of_key(&src,key))
	{
		memcpy(rcshut,src.s,src.len) ;
		rcshut[src.len] = 0 ;
	}
	stralloc_free(&src) ;
}

static inline void run_rcshut (char const *const *envp)
{
	pid_t pid ;
	size_t conflen = strlen(conf) ;
	char rcshut[4096] ;
	char confile[conflen + 1 + SS_BOOT_CONF_LEN] ;
	auto_conf(confile,conflen) ;
	parse_conf(confile,rcshut,"RCSHUTDOWN") ;
	char const *rcshut_argv[3] = { rcshut, confile, 0 } ;
	pid = child_spawn0(rcshut_argv[0], rcshut_argv, envp) ;
	if (pid)
	{
		int wstat ;
		char fmt[UINT_FMT] ;
		if (wait_pid(pid, &wstat) == -1) log_dieusys(LOG_EXIT_SYS, "waitpid") ;
		if (WIFSIGNALED(wstat))
		{
			fmt[uint_fmt(fmt, WTERMSIG(wstat))] = 0 ;
			log_warn(rcshut, " was killed by signal ", fmt) ;
		}
		else if (WEXITSTATUS(wstat))
		{
			fmt[uint_fmt(fmt, WEXITSTATUS(wstat))] = 0 ;
			log_warn(rcshut, " exited ", fmt) ;
		}
	}
	else log_warnusys("spawn ", rcshut) ;
}

static inline void prepare_shutdown (buffer *b, tain_t *deadline, unsigned int *grace_time)
{
	uint32_t u ;
	char pack[TAIN_PACK + 4] ;
	ssize_t r = sanitize_read(buffer_get(b, pack, TAIN_PACK + 4)) ;
	if (r == -1) log_dieusys(LOG_EXIT_SYS, "read from pipe") ;
	if (r < TAIN_PACK + 4) log_dieusys(101, "bad shutdown protocol") ;
	tain_unpack(pack, deadline) ;
	tain_add_g(deadline,deadline) ;
	uint32_unpack_big(pack + TAIN_PACK, &u) ;
	if (u && u <= 300000) *grace_time = u ;
}

static inline void handle_fifo (buffer *b, char *what, tain_t *deadline, unsigned int *grace_time)
{
	for (;;)
	{
		char c ;
		ssize_t r = sanitize_read(buffer_get(b, &c, 1)) ;
		if (r == -1) log_dieusys(LOG_EXIT_SYS, "read from pipe") ;
		else if (!r) break ;
		switch (c)
		{
			case 'S' :
			case 'h' :
			case 'p' :
			case 'r' :
				*what = c ;
				prepare_shutdown(b, deadline, grace_time) ;
				break ;
			case 'c' :
				*what = 'S' ;
				tain_add_g(deadline, &tain_infinite_relative) ;
				break ;
			default :
				{
					char s[2] = { c, 0 } ;
					log_warn("unknown command: ", s) ;
				}
				break ;
		}
	}
}

static inline void prepare_stage4 (char what)
{
	buffer b ;
	int fd ;
	char buf[512] ;
	char shutfinal[4096] ; //huge path allowed
	size_t conflen = strlen(conf) ;
	char confile[conflen + 1 + SS_BOOT_CONF_LEN] ;
	auto_conf(confile,conflen) ;
	parse_conf(confile,shutfinal,"RCSHUTDOWNFINAL") ;
	unlink_void(STAGE4_FILE ".new") ;
	fd = open_excl(STAGE4_FILE ".new") ;
	if (fd == -1) log_dieusys(LOG_EXIT_SYS, "open ", STAGE4_FILE ".new", " for writing") ;
	buffer_init(&b, &buffer_write, fd, buf, 512) ;
	
	if (inns)
	{
		if (buffer_puts(&b,
			"#!" SS_EXECLINE_SHEBANGPREFIX "execlineb -P\n\n"
			EXECLINE_EXTBINPREFIX "foreground { "
			S6_EXTBINPREFIX "s6-svc -Ox -- . }\n"
			EXECLINE_EXTBINPREFIX "background\n{\n  ") < 0
			
			|| (!nologger && buffer_puts(&b,
			EXECLINE_EXTBINPREFIX "foreground { "
			S6_EXTBINPREFIX "s6-svc -Xh -- ") < 0
			|| buffer_puts(&b,live) < 0
			|| buffer_puts(&b,SS_BOOT_LOG " }\n  ") < 0)
			
			|| buffer_puts(&b, S6_EXTBINPREFIX "66-scanctl ") < 0
			|| buffer_puts(&b, "-l ") < 0
			|| buffer_puts(&b, live) < 0
			|| buffer_put(&b, what == 'h' ? "s" : &what, 1) < 0
			|| buffer_putsflush(&b, "b\n}\n") < 0)
			log_dieusys(LOG_EXIT_SYS, "write to ", STAGE4_FILE ".new") ;
	}
	else
	{
		if (buffer_puts(&b,
			"#!" SS_EXECLINE_SHEBANGPREFIX "execlineb -P\n\n"
			EXECLINE_EXTBINPREFIX "foreground { "
			SS_BINPREFIX "66-umountall }\n"
			EXECLINE_EXTBINPREFIX "foreground { ") < 0
			|| buffer_put(&b,shutfinal,strlen(shutfinal)) < 0
			|| buffer_puts(&b," }\n" 
			SS_BINPREFIX "66-hpr -f -") < 0
			|| buffer_put(&b, &what, 1) < 0
			|| buffer_putsflush(&b, "\n") < 0) log_dieusys(LOG_EXIT_SYS, "write to ", STAGE4_FILE ".new") ;
	}
	if (fchmod(fd, S_IRWXU) == -1) log_dieusys(LOG_EXIT_SYS, "fchmod ", STAGE4_FILE ".new") ;
	fd_close(fd) ;
	if (rename(STAGE4_FILE ".new", STAGE4_FILE) == -1) 
		log_dieusys(LOG_EXIT_SYS, "rename ", STAGE4_FILE ".new", " to ", STAGE4_FILE) ;
}

static inline void unsupervise_tree (void)
{
	char const *except[3] =
	{
		"66-shutdownd",
		nologger ? 0 : SS_SCANDIR SS_LOG_SUFFIX,
		0
	} ;
	size_t livelen = strlen(live) ;
	size_t newlen ;
	char tmp[livelen + 1 + SS_SCANDIR_LEN + 3 + 1] ;
	memcpy(tmp,live,livelen) ;
	memcpy(tmp + livelen,"/" SS_SCANDIR "/0/",SS_SCANDIR_LEN + 4) ;
	tmp[livelen + SS_SCANDIR_LEN + 4] = 0 ;
	newlen = livelen + SS_SCANDIR_LEN + 4 ;
	DIR *dir = opendir(tmp) ;
	int fdd ;
	if (!dir) log_dieusys(LOG_EXIT_SYS, "opendir: ",tmp) ;
	fdd = dirfd(dir) ;
	if (fdd == -1) log_dieusys(LOG_EXIT_SYS, "dir_fd: ",tmp) ;
	for (;;)
	{
		char const *const *p = except ;
		direntry *d ;
		errno = 0 ;
		d = readdir(dir) ;
		if (!d) break ;
		if (d->d_name[0] == '.') continue ;
		for (; *p ; p++) if (!strcmp(*p, d->d_name)) break ;
		if (!*p)
		{
			size_t dlen = strlen(d->d_name) ;
			char fn[newlen + DOTPREFIXLEN + dlen + DOTSUFFIXLEN + 1] ;
			memcpy(fn, tmp,newlen) ;
			memcpy(fn + newlen,DOTPREFIX,DOTPREFIXLEN) ;
			memcpy(fn + newlen + DOTPREFIXLEN, d->d_name, dlen) ;
			memcpy(fn + newlen + DOTPREFIXLEN + dlen, DOTSUFFIX, DOTSUFFIXLEN + 1) ;
			if (mkrenametemp(fdd, d->d_name, fn + newlen) == -1)
			{
				log_warnusys("rename ",tmp, d->d_name, " to something based on ", fn) ;
				unlinkat(fdd, d->d_name, 0) ;
				/* if it still fails, too bad, it will restart in stage 4 and race */
			}
			else s6_svc_writectl(fn, S6_SUPERVISE_CTLDIR, "dx", 2) ;
		}
	}
	dir_close(dir) ;
	if (errno) log_dieusys(LOG_EXIT_SYS, "readdir: ",tmp) ;
}

int main (int argc, char const *const *argv, char const *const *envp)
{
	unsigned int grace_time = 3000 ;
	tain_t deadline ;
	int fdr, fdw ;
	buffer b ;
	char what = 'S' ;
	char buf[64] ;
	
	PROG = "66-shutdownd" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;
		for (;;)
		{
			int opt = subgetopt_r(argc, argv, "hl:s:g:CB", &l) ;
			if (opt == -1) break ;
			switch (opt)
			{
				case 'h' : info_help(); return 0 ;
				case 'l' : live = l.arg ; break ;
				case 's' : conf = l.arg ; break ;
				case 'g' : if (!uint0_scan(l.arg, &grace_time)) log_usage(USAGE) ; break ;
				case 'C' : inns = 1 ; break ;
				case 'B' : nologger = 1 ; break ;
				default : log_usage(USAGE) ;
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	if (conf[0] != '/') log_dieusys(LOG_EXIT_USER, "skeleton: ",conf," must be an absolute path") ;
	if (live && live[0] != '/') log_die(LOG_EXIT_USER,"live: ",live," must be an absolute path") ;
	else live = SS_LIVE ;
	if (grace_time > 300000) grace_time = 300000 ;

	/* if we're in stage 4, exec it immediately */
	{
		char const *stage4_argv[2] = { "./" STAGE4_FILE, 0 } ;
		restore_console() ;
		execve(stage4_argv[0], (char **)stage4_argv, (char *const *)envp) ;
		if (errno != ENOENT) log_warnusys("exec ", stage4_argv[0]) ;
	}

	fdr = open_read(SHUTDOWND_FIFO) ;
	if (fdr == -1 || coe(fdr) == -1)
		log_dieusys(LOG_EXIT_SYS, "open ", SHUTDOWND_FIFO, " for reading") ;
	fdw = open_write(SHUTDOWND_FIFO) ;
	if (fdw == -1 || coe(fdw) == -1)
		log_dieusys(LOG_EXIT_SYS, "open ", SHUTDOWND_FIFO, " for writing") ;
	if (sig_ignore(SIGPIPE) == -1)
		log_dieusys(LOG_EXIT_SYS, "sig_ignore SIGPIPE") ;
	buffer_init(&b, &buffer_read, fdr, buf, 64) ;
	tain_now_set_stopwatch_g() ;
	tain_add_g(&deadline, &tain_infinite_relative) ;

	for (;;)
	{
		iopause_fd x = { .fd = fdr, .events = IOPAUSE_READ } ;
		int r = iopause_g(&x, 1, &deadline) ;
		if (r == -1) log_dieusys(LOG_EXIT_SYS, "iopause") ;
		if (!r)
		{
			run_rcshut(envp) ;
			tain_now_g() ;
			if (what != 'S') break ;
			tain_add_g(&deadline, &tain_infinite_relative) ;
			continue ;
		}
		if (x.revents & IOPAUSE_READ)
			handle_fifo(&b, &what, &deadline, &grace_time) ;
	}

	fd_close(fdw) ;
	fd_close(fdr) ;
	fd_close(1) ;
	restore_console() ;

	/* The end is coming! */
	prepare_stage4(what) ;
	unsupervise_tree() ;
	
	if (sig_ignore(SIGTERM) == -1) log_warnusys("sig_ignore SIGTERM") ;
	
	if (!inns)
	{
		sync() ;
		log_info("sending all processes the TERM signal...") ;
	}
	
	kill(-1, SIGTERM) ;
	kill(-1, SIGCONT) ;
	tain_from_millisecs(&deadline, grace_time) ;
	tain_now_g() ;
	tain_add_g(&deadline, &deadline) ;
	deepsleepuntil_g(&deadline) ;
	
	if (!inns)
	{
		sync() ;
		log_info("sending all processes the KILL signal...") ;
	}
	
	kill(-1, SIGKILL) ;
	
	return 0 ;
}
	

