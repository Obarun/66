/* 
 * 66-shutdownd.c
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

#include <skalibs/posixplz.h>
#include <skalibs/uint32.h>
#include <skalibs/types.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/bytestr.h>
#include <skalibs/buffer.h>
#include <skalibs/strerr2.h>
#include <skalibs/sgetopt.h>
#include <skalibs/sig.h>
#include <skalibs/tai.h>
#include <skalibs/direntry.h>
#include <skalibs/djbunix.h>
#include <skalibs/iopause.h>
#include <skalibs/skamisc.h>
#include <skalibs/diuint32.h>

#include <execline/config.h>

#include <s6/s6-supervise.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/environ.h>

#define STAGE4_FILE "stage4"
#define DOTPREFIX ".66-shutdownd:"
#define DOTPREFIXLEN (sizeof(DOTPREFIX) - 1)
#define DOTSUFFIX ":XXXXXX"
#define DOTSUFFIXLEN (sizeof(DOTSUFFIX) - 1)
#define SHUTDOWND_FIFO "fifo"
static char const *conf = SS_SKEL_DIR ;
static char const *live = 0 ;

#define USAGE "66-shutdownd [ -h ] [ -l live ] [ -s skel ] [ -g gracetime ]"

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
;

	if (buffer_putsflush(buffer_1, help) < 0)
		strerr_diefu1sys(111, "write to stdout") ;
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

static void parse_conf(char const *confile,char *rcshut,size_t filesize)
{
	int r ;
	unsigned int i = 0 ;
	stralloc src = STRALLOC_ZERO ;
	stralloc saconf = STRALLOC_ZERO ;
	genalloc gaconf = GENALLOC_ZERO ;
	r = openreadfileclose(confile,&src,filesize) ;
	if(!r) strerr_diefu2sys(111,"open configuration file: ",confile) ; 
	if (!stralloc_0(&src)) strerr_diefu1sys(111,"append stralloc configuration file") ;
	
	r = env_split(&gaconf,&saconf,&src) ;
	if (!r) strerr_diefu2sys(111,"parse configuration file: ",confile) ;
	
	for (;i < genalloc_len(diuint32,&gaconf) ; i++)
	{
		char *key = saconf.s + genalloc_s(diuint32,&gaconf)[i].left ;
		char *val = saconf.s + genalloc_s(diuint32,&gaconf)[i].right ;
		if (!strcmp(key,"RCSHUTDOWN"))
		{
			memcpy(rcshut,val,strlen(val)) ;
			rcshut[strlen(val)] = 0 ;
		}
	}
	genalloc_free(diuint32,&gaconf) ;
	stralloc_free(&saconf) ;
	stralloc_free(&src) ;
}

static inline void run_rcshut (char const *const *envp)
{
	pid_t pid ;
	size_t filesize, conflen = strlen(conf) ;
	char confile[conflen + 1 + SS_BOOT_CONF_LEN] ;
	memcpy(confile,conf,conflen) ;
	confile[conflen] = '/' ;
	memcpy(confile + conflen + 1, SS_BOOT_CONF, SS_BOOT_CONF_LEN) ;
	confile[conflen + 1 + SS_BOOT_CONF_LEN] = 0 ;
	filesize=file_get_size(confile) ;
	char rcshut[filesize+1] ;
	parse_conf(confile,rcshut,filesize) ;
	char const *rcshut_argv[3] = { rcshut, confile, 0 } ;
	pid = child_spawn0(rcshut_argv[0], rcshut_argv, envp) ;
	if (pid)
	{
		int wstat ;
		char fmt[UINT_FMT] ;
		if (wait_pid(pid, &wstat) == -1) strerr_diefu1sys(111, "waitpid") ;
		if (WIFSIGNALED(wstat))
		{
			fmt[uint_fmt(fmt, WTERMSIG(wstat))] = 0 ;
			strerr_warnw3x(rcshut, " was killed by signal ", fmt) ;
		}
		else if (WEXITSTATUS(wstat))
		{
			fmt[uint_fmt(fmt, WEXITSTATUS(wstat))] = 0 ;
			strerr_warnw3x(rcshut, " exited ", fmt) ;
		}
	}
	else strerr_warnwu2sys("spawn ", rcshut) ;
}

static inline void prepare_shutdown (buffer *b, tain_t *deadline, unsigned int *grace_time)
{
	uint32_t u ;
	char pack[TAIN_PACK + 4] ;
	ssize_t r = sanitize_read(buffer_get(b, pack, TAIN_PACK + 4)) ;
	if (r == -1) strerr_diefu1sys(111, "read from pipe") ;
	if (r < TAIN_PACK + 4) strerr_dief1x(101, "bad shutdown protocol") ;
	tain_unpack(pack, deadline) ;
	uint32_unpack_big(pack + TAIN_PACK, &u) ;
	if (u && u <= 300000) *grace_time = u ;
}

static inline void handle_fifo (buffer *b, char *what, tain_t *deadline, unsigned int *grace_time)
{
	for (;;)
	{
		char c ;
		ssize_t r = sanitize_read(buffer_get(b, &c, 1)) ;
		if (r == -1) strerr_diefu1sys(111, "read from pipe") ;
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
					strerr_warnw2x("unknown command: ", s) ;
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
	unlink_void(STAGE4_FILE ".new") ;
	fd = open_excl(STAGE4_FILE ".new") ;
	if (fd == -1) strerr_diefu3sys(111, "open ", STAGE4_FILE ".new", " for writing") ;
	buffer_init(&b, &buffer_write, fd, buf, 512) ;

	if (buffer_puts(&b,
		"#!" SS_EXECLINE_SHEBANGPREFIX "execlineb -P\n\n"
		EXECLINE_EXTBINPREFIX "foreground { "
		SS_BINPREFIX "66-umountall }\n"
		SS_BINPREFIX "66-hpr -f -") < 0
		|| buffer_put(&b, &what, 1) < 0
		|| buffer_putsflush(&b, "\n") < 0) strerr_diefu2sys(111, "write to ", STAGE4_FILE ".new") ;
	if (fchmod(fd, S_IRWXU) == -1) strerr_diefu2sys(111, "fchmod ", STAGE4_FILE ".new") ;
	fd_close(fd) ;
	if (rename(STAGE4_FILE ".new", STAGE4_FILE) == -1) 
		strerr_diefu4sys(111, "rename ", STAGE4_FILE ".new", " to ", STAGE4_FILE) ;
}

static inline void unsupervise_tree (void)
{
	static char const *except[] =
	{
		SS_SCANDIR SS_LOG_SUFFIX,
		"66-shutdownd",
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
	if (!dir) strerr_diefu2sys(111, "opendir: ",tmp) ;
	fdd = dirfd(dir) ;
	if (fdd == -1) strerr_diefu2sys(111, "dir_fd: ",tmp) ;
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
				strerr_warnwu5sys("rename ",tmp, d->d_name, " to something based on ", fn) ;
				unlinkat(fdd, d->d_name, 0) ;
				/* if it still fails, too bad, it will restart in stage 4 and race */
			}
			else s6_svc_writectl(fn, S6_SUPERVISE_CTLDIR, "dx", 2) ;
		}
	}
	dir_close(dir) ;
	if (errno) strerr_diefu2sys(111, "readdir: ",tmp) ;
}

int main (int argc, char const *const *argv, char const *const *envp)
{
	char what = 'S' ;
	unsigned int grace_time = 3000 ;
	tain_t deadline ;
	int fdr, fdw ;
	buffer b ;
	char buf[64] ;
	
	PROG = "66-shutdownd" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;
		for (;;)
		{
			int opt = subgetopt_r(argc, argv, "hl:s:g:", &l) ;
			if (opt == -1) break ;
			switch (opt)
			{
				case 'h' : info_help(); return 0 ;
				case 'l' : live = l.arg ; break ;
				case 's' : conf = l.arg ; break ;
				case 'g' : if (!uint0_scan(l.arg, &grace_time)) strerr_dieusage(100,USAGE) ; break ;
				default : strerr_dieusage(100,USAGE) ;
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	if (conf[0] != '/') strerr_dief3x(110, "skeleton: ",conf," must be an absolute path") ;
	if (live && live[0] != '/') strerr_dief3x(110,"live: ",live," must be an absolute path") ;
	else live = SS_LIVE ;
	if (grace_time > 300000) grace_time = 300000 ;

	/* if we're in stage 4, exec it immediately */
	{
		char const *stage4_argv[2] = { "./" STAGE4_FILE, 0 } ;
		execve(stage4_argv[0], (char **)stage4_argv, (char *const *)envp) ;
		if (errno != ENOENT) strerr_warnwu2sys("exec ", stage4_argv[0]) ;
	}

	fdr = open_read(SHUTDOWND_FIFO) ;
	if (fdr == -1 || coe(fdr) == -1)
		strerr_diefu3sys(111, "open ", SHUTDOWND_FIFO, " for reading") ;
	fdw = open_write(SHUTDOWND_FIFO) ;
	if (fdw == -1 || coe(fdw) == -1)
		strerr_diefu3sys(111, "open ", SHUTDOWND_FIFO, " for writing") ;
	if (sig_ignore(SIGPIPE) == -1)
		strerr_diefu1sys(111, "sig_ignore SIGPIPE") ;
	buffer_init(&b, &buffer_read, fdr, buf, 64) ;
	tain_now_g() ;
	tain_add_g(&deadline, &tain_infinite_relative) ;

	for (;;)
	{
		iopause_fd x = { .fd = fdr, .events = IOPAUSE_READ } ;
		int r = iopause_g(&x, 1, &deadline) ;
		if (r == -1) strerr_diefu1sys(111, "iopause") ;
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
	if (open("/dev/console", O_WRONLY) != 1)
		strerr_diefu1sys(111, "open /dev/console for writing") ;
	if (fd_copy(2, 1) == -1) strerr_warnwu1sys("fd_copy") ;


	/* The end is coming! */

	prepare_stage4(what) ;
	unsupervise_tree() ;
	sync() ;
	if (sig_ignore(SIGTERM) == -1) strerr_warnwu1sys("sig_ignore SIGTERM") ;
		strerr_warni1x("sending all processes the TERM signal...") ;
	kill(-1, SIGTERM) ;
	kill(-1, SIGCONT) ;
	tain_from_millisecs(&deadline, grace_time) ;
	tain_add_g(&deadline, &deadline) ;
	deepsleepuntil_g(&deadline) ;
	sync() ;
	strerr_warni1x("sending all processes the KILL signal...") ;
	kill(-1, SIGKILL) ;
	return 0 ;
}
	

