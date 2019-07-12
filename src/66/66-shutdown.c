/* 
 * 66-shutdown.c
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
 * This file is a modified copy of s6-linux-init-shutdown.c file
 * coming from skarnet software at https://skarnet.org/software/s6-linux-init.
 * All credits goes to Laurent Bercot <ska-remove-this-if-you-are-not-a-bot@skarnet.org>
 * */

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <utmpx.h>

#include <skalibs/uint32.h>
#include <skalibs/types.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/strerr2.h>
#include <skalibs/sgetopt.h>
#include <skalibs/sig.h>
#include <skalibs/tai.h>
#include <skalibs/djbunix.h>
#include <skalibs/djbtime.h>
#include <skalibs/buffer.h>

#include <66/config.h>
#include <66/hpr.h>

#ifndef UT_NAMESIZE
#define UT_NAMESIZE 32
#endif

#define USAGE "66-shutdown [ -H ] [ -l live ] [ -h | -p | -r | -k ] [ -f | -F ] [ -a ] [ -t sec ] time [ message ]  or  66-shutdown -c [ message ]"

#define AC_FILE SS_SKEL_DIR "shutdown.allow"
#define AC_BUFSIZE 4096
#define AC_MAX 64

static char const *live = 0 ;

static inline void info_help (void)
{
  static char const *help =
"66-shutdown <options> time [message] or 66-shutdown -c [ message ]\n"
"\n"
"options :\n"
"	-H: print this help\n" 
"	-l: live directory\n"
"	-h: halt the system\n"
"	-p: poweroff the system\n"
"	-r: reboot the system\n"
"	-f: ignored(compatibility option)\n"
"	-F: ignored(compatibility option)\n"
"	-k: only warn users\n"
"	-a: check users access control\n"
"	-t: grace time between the SIGTERM and the SIGKILL\n"
"	-c: cancel planned shutdown\n"
;
	if (buffer_putsflush(buffer_1, help) < 0)
		strerr_diefu1sys(111, "write to stdout") ;
}
/* shutdown 01:23: date/time format parsing */

static inline void add_one_day (struct tm *tm)
{
	tm->tm_isdst = -1 ;
	if (tm->tm_mday++ < 31) return ;
	tm->tm_mday = 1 ;
	if (tm->tm_mon++ < 11) return ;
	tm->tm_mon = 0 ;
	tm->tm_year++ ;
}

static inline void parse_hourmin (tain_t *when, char const *s)
{
	tai_t taithen ;
	struct tm tmthen ;
	unsigned int hour, minute ;
	size_t len = uint_scan(s, &hour) ;
	if (!len || len > 2 || s[len] != ':' || hour > 23)
		strerr_dief1x(110, "invalid time format") ;
	s += len+1 ;
	len = uint0_scan(s, &minute) ;
	if (!len || len != 2 || minute > 59)
		strerr_dief1x(110, "invalid time format") ;
	if (!localtm_from_tai(&tmthen, tain_secp(&STAMP), 1))
		strerr_diefu1sys(111, "break down current time into struct tm") ;
	tmthen.tm_hour = hour ;
	tmthen.tm_min = minute ;
	tmthen.tm_sec = 0 ;
	if (!tai_from_localtm(&taithen, &tmthen))
		strerr_diefu1sys(111, "assemble broken-down time into tain_t") ;
	if (tai_less(&taithen, tain_secp(&STAMP)))
	{
		add_one_day(&tmthen) ;
		if (!tai_from_localtm(&taithen, &tmthen))
			strerr_diefu1sys(111, "assemble broken-down time into tain_t") ;
	}
	when->sec = taithen ;
	when->nano = 0 ;
}

static void parse_mins (tain_t *when, char const *s)
{
	unsigned int mins ;
	if (!uint0_scan(s, &mins)) strerr_dieusage(100,USAGE) ;
	tain_addsec_g(when, mins * 60) ;
}

static inline void parse_time (tain_t *when, char const *s)
{
	if (!strcmp(s, "now")) tain_copynow(when) ;
	else if (s[0] == '+') parse_mins(when, s+1) ;
	else if (strchr(s, ':')) parse_hourmin(when, s) ;
	else parse_mins(when, s) ;
}


 /* shutdown -a: access control */

static inline unsigned char cclass (unsigned char c)
{
	switch (c)
	{
		case 0 : return 0 ;
		case '\n' : return 1 ;
		case '#' : return 2 ;
		default : return 3 ;
	}
}

static inline unsigned int parse_authorized_users (char *buf, char const **users, unsigned int max)
{
	static unsigned char const table[3][4] =
	{
		{ 0x03, 0x00, 0x01, 0x12 },
		{ 0x03, 0x00, 0x01, 0x01 },
		{ 0x23, 0x20, 0x02, 0x02 }
	} ;
	size_t pos = 0 ;
	size_t mark = 0 ;
	unsigned int n = 0 ;
	unsigned int state = 0 ;
	for (; state < 3 ; pos++)
	{
		unsigned char what = table[state][cclass(buf[pos])] ;
		state = what & 3 ;
		if (what & 0x10) mark = pos ;
		if (what & 0x20)
		{
			if (n >= max)
			{
				char fmt[UINT32_FMT] ;
				fmt[uint32_fmt(fmt, AC_MAX)] = 0 ;
				strerr_warnw4x(AC_FILE, " lists more than ", fmt, " authorized users - ignoring the extra ones") ;
				break ;
			}
			buf[pos] = 0 ;
			users[n++] = buf + mark ;
		}
	}
	return n ;
}

static inline int match_users_with_utmp (char const *const *users, unsigned int n)
{
	setutxent() ;
	for (;;)
	{
		struct utmpx *utx ;
		errno = 0 ;
		utx = getutxent() ;
		if (!utx) break ;
		if (utx->ut_type != USER_PROCESS) continue ;
		for (unsigned int i = 0 ; i < n ; i++)
			if (!strncmp(utx->ut_user, users[i], UT_NAMESIZE)) goto yes ;
	}
	endutxent() ;
	return 0 ;
	yes:
		endutxent() ;
		return 1 ;
}

static inline void access_control (void)
{
	char buf[AC_BUFSIZE] ;
	char const *users[AC_MAX] ;
	unsigned int n ;
	struct stat st ;
	int fd = open_readb(AC_FILE) ;
	if (fd == -1)
	{
		if (errno == ENOENT) return ;
			strerr_diefu2sys(111, "open ", AC_FILE) ;
	}
	if (fstat(fd, &st) == -1)
		strerr_diefu2sys(111, "stat ", AC_FILE) ;
	if (st.st_size >= AC_BUFSIZE)
	{
		char fmt[UINT32_FMT] ;
		fmt[uint32_fmt(fmt, AC_BUFSIZE - 1)] = 0 ;
		strerr_dief4x(1, AC_FILE, " is too big: it needs to be ", fmt, " bytes or less") ;
	}
	if (allread(fd, buf, st.st_size) < st.st_size)
		strerr_diefu2sys(111, "read ", AC_FILE) ;
	fd_close(fd) ;
	buf[st.st_size] = 0 ;
	n = parse_authorized_users(buf, users, AC_MAX) ;
	if (!n || !match_users_with_utmp(users, n))
		strerr_dief1x(1, "no authorized users logged in") ;
}


 /* main */

int main (int argc, char const *const *argv)
{
	unsigned int gracetime = 0 ;
	int what = 0 ;
	int doactl = 0 ;
	int docancel = 0 ;
	tain_t when ;
	
	PROG = "66-shutdown" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;
		for (;;)
		{
			int opt = subgetopt_r(argc, argv, "l:hprkafFct:", &l) ;
			if (opt == -1) break ;
			switch (opt)
			{
				case 'H' : info_help() ; return 0 ;
				case 'l' : live = l.arg ; break ;
				case 'h' : what = 1 ; break ;
				case 'p' : what = 2 ; break ;
				case 'r' : what = 3 ; break ;
				case 'k' : what = 4 ; break ;
				case 'a' : doactl = 1 ; break ;
				case 'f' : /* talk to the hand */ break ;
				case 'F' : /* no, the other hand */ break ;
				case 'c' : docancel = 1 ; break ;
				case 't' : if (!uint0_scan(l.arg, &gracetime)) strerr_dieusage(100,USAGE) ; break ;
				default : strerr_dieusage(100,USAGE) ;
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	if (live && live[0] != '/') strerr_dief3x(110,"live: ",live," must be an absolute path") ;
	else live = SS_LIVE ;
	size_t livelen = strlen(live) ;
	char tlive[livelen + INITCTL_LEN + 1] ;
	memcpy(tlive,live,livelen) ;
	memcpy(tlive + livelen,INITCTL,INITCTL_LEN) ;
	tlive[livelen + INITCTL_LEN] = 0 ;
	if (geteuid())
	{
		errno = EPERM ;
		strerr_diefu1sys(111, "shutdown") ;
	}
	if (doactl) access_control() ;
	if (!tain_now_g()) strerr_warnw1sys("get current time") ;
	if (docancel)
	{
		if (argv[0]) hpr_wall(argv[0]) ;
		if (!hpr_cancel(tlive)) goto err ;
		return 0 ;
	}
	if (!argc) strerr_dieusage(100,USAGE) ;
	parse_time(&when, argv[0]) ;
	if (argv[1]) hpr_wall(argv[1]) ;
	if (what < 4)
	{
		if (gracetime > 300)
		{
			gracetime = 300 ;
			strerr_warnw1x("delay between SIGTERM and SIGKILL is capped to 300 seconds") ;
		}
		if (!hpr_shutdown(tlive,what, &when, gracetime * 1000)) goto err ;
	}
	return 0 ;
	err:
		strerr_diefu2sys(111, "write to ", tlive) ;
}
