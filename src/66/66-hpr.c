/* 
 * 66-hpr.c
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
 * This file is a modified copy of s6-linux-init-hpr.c file
 * coming from skarnet software at https://skarnet.org/software/s6-linux-init.
 * All credits goes to Laurent Bercot <ska-remove-this-if-you-are-not-a-bot@skarnet.org>
 * */

#include <skalibs/nonposix.h>

#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <utmpx.h>
#include <sys/reboot.h>

#include <oblibs/log.h>

#include <skalibs/sgetopt.h>
#include <skalibs/sig.h>
#include <skalibs/tai.h>
#include <skalibs/djbunix.h>
#include <skalibs/buffer.h>

#include <66/hpr.h>
#include <66/config.h>

#ifndef UT_NAMESIZE
#define UT_NAMESIZE 32
#endif

#ifndef UT_HOSTSIZE
#define UT_HOSTSIZE 256
#endif

#ifndef _PATH_WTMP
#define _PATH_WTMP "/dev/null/wtmp"
#endif

#define USAGE "66-hpr [ -H ] [ -l live ] [ -b banner ] [ -f ] [ -h | -p | -r ] [ -d | -w ] [ -W ]"

char const *banner = 0 ;
char const *live = 0 ;

static inline void info_help (void)
{
  static char const *help =
"66-hpr <options>\n"
"\n"
"options :\n"
"	-H: print this help\n" 
"	-l: live directory\n"
"	-b: end banner to display\n"
"	-f: force\n"
"	-h: halt the system\n"
"	-p: poweroff the system\n"
"	-r: reboot the system\n"
"	-d: do not write wtmp shutdown entry\n"
"	-w: only write wtmp shutdown entry\n"
"	-W: do not send a wall message\n"
;
	if (buffer_putsflush(buffer_1, help) < 0)
		log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
}
int main (int argc, char const *const *argv)
{
	int what = 0 ;
	int force = 0 ;
	int dowtmp = 1 ;
	int dowall = 1 ;
	
	PROG = "66-hpr" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;
		for (;;)
		{
			int opt = subgetopt_r(argc, argv, "Hl:hprfdwWb:", &l) ;
			if (opt == -1) break ;
			switch (opt)
			{
				case 'H' : info_help() ; return 0 ;
				case 'l' : live = l.arg ; break ;
				case 'h' : what = 1 ; break ;
				case 'p' : what = 2 ; break ;
				case 'r' : what = 3 ; break ;
				case 'f' : force = 1 ; break ;
				case 'd' : dowtmp = 0 ; break ;
				case 'w' : dowtmp = 2 ; break ;
				case 'W' : dowall = 0 ; break ;
				case 'b' : banner = l.arg ; break ;
				default :  log_usage(USAGE) ;
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	if (!banner) banner = HPR_WALL_BANNER ;
	if (live && live[0] != '/') log_die(LOG_EXIT_USER,"live: ",live," must be an absolute path") ;
	else live = SS_LIVE ;
	if (!what)
		log_die(LOG_EXIT_USER, "one of the -h, -p or -r options must be given") ;

	if (geteuid())
	{
		errno = EPERM ;
		log_diesys(LOG_EXIT_USER, "nice try, peon") ;
	}

	if (force)
	{
		sync() ;
		reboot(what == 3 ? RB_AUTOBOOT : what == 2 ? RB_POWER_OFF : RB_HALT_SYSTEM) ;
			log_dieusys(LOG_EXIT_SYS, "reboot()") ;
	}
	
	if (!tain_now_g()) log_warnsys("get current time") ;
	if (dowtmp)
	{
		struct utmpx utx =
		{
			.ut_type = RUN_LVL,
			.ut_pid = getpid(),
			.ut_line = "~",
			.ut_id = "",
			.ut_session = getsid(0)
		} ;
		strncpy(utx.ut_user, what == 3 ? "reboot" : "shutdown", UT_NAMESIZE) ;
		if (gethostname(utx.ut_host, UT_HOSTSIZE) < 0)
		{
			utx.ut_host[0] = 0 ;
			log_warnusys("gethostname") ;
		}
		else utx.ut_host[UT_HOSTSIZE - 1] = 0 ;

/* glibc multilib can go fuck itself */
#ifdef  __WORDSIZE_TIME64_COMPAT32
	{
		struct timeval tv ;
		if (!timeval_from_tain(&tv, &STAMP))
			log_warnusys("timeval_from_tain") ;
		utx.ut_tv.tv_sec = tv.tv_sec ;
		utx.ut_tv.tv_usec = tv.tv_usec ;
	}
#else
	if (!timeval_from_tain(&utx.ut_tv, &STAMP))
		log_warnusys("timeval_from_tain") ;
#endif

		updwtmpx(_PATH_WTMP, &utx) ;
	}
	if (dowall) hpr_wall(banner) ;
	if (dowtmp < 2)
	{
		size_t livelen = strlen(live) ;
		char tlive[livelen + INITCTL_LEN + 1] ;
		memcpy(tlive,live,livelen) ;
		memcpy(tlive + livelen,INITCTL,INITCTL_LEN) ;
		tlive[livelen + INITCTL_LEN] = 0 ;
		if (!hpr_shutdown(tlive,what, &tain_zero, 0))
			log_dieusys(LOG_EXIT_SYS, "notify 66-shutdownd") ;
	}
	return 0 ;
}
