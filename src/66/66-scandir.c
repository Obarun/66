/* 
 * 66-scandir.c
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

#include <s6/config.h>
#include <s6-portable-utils/config.h>
#include <execline/config.h>

#include <66/config.h>
#include <66/utils.h>
#include <66/constants.h>

#include <stdio.h>

#define CRASH 0
#define FINISH 1
#define HUP 2
#define INT 3
#define QUIT 4
#define TERM 5
#define USR1 6
#define USR2 7

#define MAXENV 4096 
#define SIGSIZE 64

static uid_t OWNER ;
size_t OWNERLEN ; 
char OWNERSTR[256] ;

static gid_t GIDOWNER ;
size_t GIDOWNERLEN ;
char GIDSTR[256] ;

static char const *stage2tini = "/etc/66/stage2" ;
static char const *stage3 = "/etc/66/stage3 $@" ;


#define USAGE "66-scandir [ -h help ] [ -v verbosity ] [ -b boot ] [ -l live ] [ -t rescan ] [ -2 stage2.tini ] [ -3 stage3 ] [ -e environment ] [ -c create ] [ -r remove ] [ -u up ] [ -s signal ] owner"

unsigned int VERBOSITY = 1 ;
static unsigned int BOOT = 0 ;

static inline void info_help (void)
{
  static char const *help =
"66-scandir <options> tree\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-v: increase/decrease verbosity\n"
"	-b: boot scandir\n"
"	-l: live directory\n"
"	-t: rescan scandir every milliseconds\n"
"	-2: stage2.tini scripts\n"
"	-3: stage3 scripts\n"
"	-e: directory environment\n"
"	-c: create scandir\n"
"	-r: remove scandir\n"
"	-u: bring up scandir\n"
"	-s: send a signal to the scandir\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
}
static inline unsigned int lookup (char const *const *table, char const *signal)
{
  unsigned int i = 0 ;
  for (; table[i] ; i++) if (!strcmp(signal, table[i])) break ;
  return i ;
}
static inline unsigned int parse_signal (char const *signal)
{
  static char const *const signal_table[] =
  {
    "reload",
    "interrupt",
    "quit",
    "halt",
    "reboot",
    "poweroff",
    0
  } ;
  unsigned int i = lookup(signal_table, signal) ;
  if (!signal_table[i]) strerr_dief2x(111,"unknow signal: ",signal) ;
  return i ;
}

int scandir_down(char const *scandir, char const *signal, char const *const *envp)
{
	int r ;
	unsigned int sig = 5 ;
	size_t siglen = strlen(signal) ;
	char csig[SIGSIZE + 1] ;
	
	if (siglen > SIGSIZE) strerr_diefu2x(111,"too many command to send to: ",scandir) ;
	r = scandir_ok(scandir) ;
	if (r < 0) strerr_diefu2sys(111, "check ", scandir) ;
	if (!r)
	{
		VERBO2 strerr_warni3x("scandir ",scandir," not running") ;
		return 0 ;
	}
	sig = parse_signal(signal) ;
	
	if (sig < 6)
	{
		csig[0] = '-' ;
		switch(sig)
		{
			case 0: csig[1] = 'a' ; 
					csig[2] = 'n' ;
					csig[3] = 0 ;
					break ;
			case 1: csig[1] = 'r' ;
					csig[2] = 't' ;
					csig[3] = 0 ;
					break ;
			case 2: csig[1] = 'q' ; 
					csig[2] = 0 ;
					break ;
			case 3: csig[1] = 's' ;
					csig[2] = 't' ;
					csig[3] = 0 ;
					break ;
			case 4: csig[1] = '6' ;
					csig[2] = 0 ;
					break ;
			case 5: csig[1] = 'p' ;
					csig[2] = 't' ;
					csig[3] = 0 ;
					break ;
			default: break ;
		}
	}
	else
	{
		csig[0] = '-' ;
		memcpy(csig + 1,signal,siglen) ;
		csig[siglen + 1] = 0 ;
	}

	char const *newdown[5] ;
	unsigned int m = 0 ;
	newdown[m++] = S6_BINPREFIX "s6-svscanctl" ;
	newdown[m++] = csig ;
	newdown[m++] = "--" ;
	newdown[m++] = scandir ;
	newdown[m++] = 0 ;
	VERBO2 strerr_warni5x("Sending ",csig," signal to scandir: ",scandir," ...") ;	
	xpathexec_run (newdown[0], newdown, envp) ;

}

int scandir_up(char const *scandir, unsigned int timeout, char const *const *envp)
{
	int r ;
	r = scandir_ok(scandir) ;
	if (r < 0) strerr_diefu2sys(111, "check ", scandir) ;
	if (r)
	{
		VERBO2 strerr_warni3x("scandir ",scandir," already running") ;
		return 0 ;
	}
	
	/** setsid */
	if (BOOT)
	{
		if (setpgid(0, 0) < 0)
			VERBO2 strerr_warnwu1sys("setpgid") ;
	}
	
	char const *newup[6] ;
	unsigned int m = 0 ;
	char fmt[UINT_FMT] ;
	fmt[uint_fmt(fmt, timeout)] = 0 ;
	
	newup[m++] = S6_BINPREFIX "s6-svscan" ;
	newup[m++] = "-st" ;
	newup[m++] = fmt ;
	//newup[m++] = "-d" ;
	//newup[m++] = "3" ;
	newup[m++] = "--" ;
	newup[m++] = scandir ;
	newup[m++] = 0 ;
	VERBO2 strerr_warni3x("Starting scandir ",scandir," ...") ;
	xpathexec_run (newup[0], newup, envp) ;
}

int write_log(char const *scandir, char const *scanname)
{
	int r ;
	
	size_t scandirlen = strlen(scandir) ;
	size_t blogless, pathless ;
	
	stralloc run = STRALLOC_ZERO ;
	stralloc path = STRALLOC_ZERO ;
	stralloc saname = STRALLOC_ZERO ;
	stralloc tmp = STRALLOC_ZERO ;
	
	VERBO2 strerr_warni3x("create scandir logger ",scandir,"/scandir-log ...") ;
	
	if (!OWNER)
	{
		if (!stralloc_cats(&path,SS_LOGGER_SYS_DIRECTORY)) retstralloc(0,"write_log") ;
	}
	else
	{
		if (!set_ownerhome(&path,OWNER))
		{
			VERBO3 strerr_warnwu1x("set owner path") ;
			return 0 ;
		}
		path.len-- ;
		if (!stralloc_cats(&path,SS_LOGGER_USER_DIRECTORY)) retstralloc(0,"write_log") ;
	}
	pathless = path.len ;
	if (!stralloc_0(&path)) retstralloc(0,"write_log") ;
	
	/** create log direcory onto the scandir
	 * usually scandir-log */
	if (!stralloc_cats(&saname,"scandir-")) retstralloc(0,"write_log") ;
	blogless = saname.len ;
	if (!stralloc_cats(&saname,"log")) retstralloc(0,"write_log") ;
	
	size_t bnew ;	
	char b[scandirlen +  saname.len + 1 + 4 + 1] ;// 12 -> scandir-log + '/' + '/run'
	memcpy(b, scandir, scandirlen) ;
	memcpy(b + scandirlen, "/", 1) ;
	memcpy(b + scandirlen + 1, saname.s, saname.len) ;//-1 remove 0
	bnew = scandirlen + saname.len + 1 ;
	b[bnew] = 0 ;
		
	r = scan_mode(b,S_IFDIR) ;
	if (r < 0) { errno = EEXIST ; return 0 ; }
	if (!r)
	{
		VERBO3 strerr_warnt2x("create log directory: ",b) ;
		r = dir_create(b,0755) ;
		if (r != 1)
		{
			VERBO3 strerr_warnwu2sys("create log directory ",b) ;
			return 0 ;
		}
		VERBO3 strerr_warnt6x("chown directory: ",b," to: ",OWNERSTR,":",GIDSTR) ;
		if (chown(b,OWNER,GIDOWNER) < 0)
		{
			VERBO3 strerr_warnwu2sys("chown: ",b) ;
			return 0 ;
		}
	}
	/** make run file */
	path.len = pathless ;
	if (!stralloc_cats(&path,"scandir-")) retstralloc(0,"write_log") ;
	if (!stralloc_cats(&path,scanname)) retstralloc(0,"write_log") ;
	if (!stralloc_0(&path)) retstralloc(0,"write_log") ;
	
	VERBO3 strerr_warnt3x("write: ",b,"/run file") ;
	if (!stralloc_cats(&run, "#!" EXECLINE_SHEBANGPREFIX "execlineb -P\n" \
							EXECLINE_BINPREFIX "fdmove -c 2 1\n" \
							S6_BINPREFIX "s6-log n3 t s1000000 ")) retstralloc(0, "write_log") ;
	if (!stralloc_cats(&run,path.s)) retstralloc(0, "write_log") ;
	if (!stralloc_cats(&run,"\n")) retstralloc(0, "write_log") ;						
	if (!file_write_unsafe(b,"run",run.s,run.len))
	{
		VERBO3 strerr_warnwu3sys("write: ",b,"/run") ;
		return 0 ;
	}

	memcpy(b + bnew,"/run",4) ;
	b[bnew + 4] = 0 ;
	VERBO3 strerr_warnt2x("chmod 0755: ",b) ;
	if (chmod(b,0755) < 0)
	{
		VERBO3 strerr_warnwu2sys("chmod: ",b) ;
		return 0 ;
	}
	VERBO3 strerr_warnt6x("chown directory: ",b," to: ",OWNERSTR,":",GIDSTR) ;
	if (chown(b,OWNER,GIDOWNER) < 0)
	{
		VERBO3 strerr_warnwu2sys("chown: ",b) ;
		return 0 ;
	}
	
	/** create SS_LOGGER_{USER,SYS}_DIRECTORY
	 * usually log/scandir-owner_uid */
	saname.len = blogless ;
	if (!stralloc_cats(&saname,scanname)) retstralloc(0,"write_log") ;
	if (!stralloc_0(&saname)) retstralloc(0,"write_log") ;
	
	if (!stralloc_catb(&tmp,path.s,pathless)) retstralloc(0,"write_log") ;
	if (!stralloc_0(&tmp)) retstralloc(0,"write_log") ;

	/** $HOME/ */
	r = scan_mode(path.s,S_IFDIR) ;
	if (r < 0) { errno = EEXIST ; return 0 ; }
	if (!r)
	{
		VERBO3 strerr_warnt3x("create: ",tmp.s,saname.s) ;
		r = dir_create_under(tmp.s,saname.s,0755) ;
		if (r != 1)
		{
			VERBO3 strerr_warnwu3sys("create directory: ",tmp.s,saname.s) ;
			return 0 ;
		}
	}
	if (!stralloc_catb(&tmp,path.s,saname.len)) retstralloc(0,"write_log") ;
	if (!stralloc_0(&tmp)) retstralloc(0,"write_log") ;
	VERBO3 strerr_warnt6x("chown directory: ",tmp.s," to: ",OWNERSTR,":",GIDSTR) ;
	if (chown(tmp.s,OWNER,GIDOWNER) < 0)
	{
		VERBO3 strerr_warnwu2sys("chown: ",tmp.s) ;
		return 0 ;
	}
	stralloc_free(&run) ;
	stralloc_free(&path) ;
	stralloc_free(&saname) ;
	stralloc_free(&tmp) ;
	
	return 1 ;
}

int write_bootlog(char const *live, char const *scandir, char const *scanname) 
{
	int r ;
	size_t livelen = strlen(live) ;
	size_t scandirlen = strlen(scandir) ;
	size_t scannamelen = strlen(scanname) ;
	
	size_t logdirless ;
	char path[livelen + 4 + scannamelen + 1] ;
	char logdir[scandirlen + 12 + 5 + 1] ;
	
	stralloc run = STRALLOC_ZERO ;
	
	VERBO2 strerr_warni3x("creating scandir boot logger ",scandir,"/scandir-log ...") ;
	
	/** run/66/scandir/uid_name/scandir-log */
	memcpy(logdir,scandir,scandirlen) ;
	memcpy(logdir + scandirlen,"/scandir-log",12) ;
	logdirless = scandirlen + 12 ; 
	logdir[logdirless] = 0 ;
	
	r = scan_mode(logdir,S_IFDIR) ;
	if (r < 0) { errno = EEXIST ; return 0 ; }
	if (!r)
	{
		VERBO3 strerr_warnt2x("create directory: ",logdir) ;
		r = dir_create(logdir,0755) ;
		if (!r)
		{
			VERBO3 strerr_warnwu2sys("create directory: ",logdir) ;
			return 0 ;
		}
		VERBO3 strerr_warnt6x("chown directory: ",logdir," to: ",OWNERSTR,":",GIDSTR) ;
		if (chown(logdir,OWNER,GIDOWNER) < 0)
		{
			VERBO3 strerr_warnwu2sys("chown: ",logdir) ;
			return 0 ;
		}
		
	}
	/** make the fifo*/
	memcpy(logdir + logdirless, "/fifo", 5) ;
	logdir[logdirless + 5] = 0 ;
	
	
	r = scan_mode(logdir,S_IFIFO) ;
	if (r < 0) { errno = EEXIST ; return 0 ; }
	if (!r)
	{
		VERBO3 strerr_warnt2x("create fifo: ",logdir) ;
		if (mkfifo(logdir, 0644) < 0)
		{
			VERBO3 strerr_warnwu2sys("create fifo: ",logdir) ;
			return 0 ;
		}
		VERBO3 strerr_warnt6x("chown directory: ",logdir," to: ",OWNERSTR,":",GIDSTR) ;
		if (chown(logdir,OWNER,GIDOWNER) < 0)
		{
			VERBO3 strerr_warnwu2sys("chown: ",logdir) ;
			return 0 ;
		}
		
	}
	
	/** set the log path for the run file*/
	memcpy(path,live,livelen) ;
	memcpy(path+livelen,"log",3) ;
	path[livelen + 3] = 0 ;
	VERBO3 strerr_warnt4x("create directory: ",path,"/",scanname) ;
	r = dir_create_under(path,scanname,0755) ;
	if (r < 0) 
	{
		VERBO3 strerr_warnwu3sys("create: ",path,scanname) ;
		return 0 ;
	}
	path[livelen + 3] = '/' ;
	memcpy(path + livelen + 4,scanname,scannamelen) ;
	path[livelen + 4 + scannamelen] = 0 ;
	VERBO3 strerr_warnt6x("chown directory: ",path," to: ",OWNERSTR,":",GIDSTR) ;
	if (chown(path,OWNER,GIDOWNER) < 0)
	{
		VERBO3 strerr_warnwu2sys("chown: ",path) ;
		return 0 ;
	}

	/** make run file */
	if (!stralloc_cats(&run, "#!" EXECLINE_SHEBANGPREFIX "execlineb -P\n" \
							EXECLINE_BINPREFIX "redirfd -w 2 /dev/console\n" \
							EXECLINE_BINPREFIX "redirfd -w 1 /dev/null\n" \
							EXECLINE_BINPREFIX "redirfd -rnb 0 fifo\n" \
							S6_BINPREFIX "s6-log -bp -- t ")) retstralloc(0, "write_bootlog") ;
	if (!stralloc_cats(&run,path)) retstralloc(0, "write_bootlog") ;
	if (!stralloc_cats(&run,"\n")) retstralloc(0, "write_bootlog") ;
	
	logdir[logdirless] = 0 ;

	VERBO3 strerr_warnt3x("write file: ",logdir,"/run") ;
	if (!file_write_unsafe(logdir,"run",run.s,run.len))
	{
		VERBO3 strerr_warnwu3sys("write file: ",logdir,"/run") ;
		return 0 ;
	}
	
	memcpy(logdir + logdirless,"/run",4) ;
	logdir[logdirless + 4] = 0 ;	

	VERBO3 strerr_warnt2x("chmod 0755: ",logdir) ;
	if (chmod(logdir,0755) < 0)
	{
		VERBO3 strerr_warnwu2sys("chmod: ",logdir) ;
		return 0 ;
	}
	VERBO3 strerr_warnt6x("chown directory: ",logdir," to: ",OWNERSTR,":",GIDSTR) ;
	if (chown(logdir,OWNER,GIDOWNER) < 0)
	{
		VERBO3 strerr_warnwu2sys("chown: ",logdir) ;
		return 0 ;
	}
	stralloc_free(&run) ;
	
	return 1 ;
}


int write_control(char const *scandir,char const *tree, char const *filename, int file)
{
	int r ;
	
	size_t scandirlen = strlen(scandir) ;

	char bscan[scandirlen + 1] ;
	
	stralloc sa = STRALLOC_ZERO ;
	
	VERBO2 strerr_warni3x("write control file ",filename + 1," ...") ;
	
	memcpy(bscan, scandir,scandirlen) ;
	dir_unslash(bscan) ;
	r = get_rlen_until(bscan,'/',scandirlen) ;
	bscan[r] = 0 ;

	/** shebang */
	if (!stralloc_cats(&sa, "#!" EXECLINE_SHEBANGPREFIX "execlineb -P\n")) ;
	
	if (file == FINISH)
	{
		if (BOOT)
		{
			if (!stralloc_cats(&sa, stage3)) retstralloc(0, "write_controlboot") ;
			if (!stralloc_cats(&sa, "\n" )) retstralloc(0, "write_controlboot") ;
		}
		else
		{
			if (!stralloc_cats(&sa,S6_PORTABLE_UTILS_BINPREFIX "s6-echo -- \"scandir ")) retstralloc(0, "write_control") ;
			if (!stralloc_cats(&sa, bscan)) retstralloc(0, "write_control") ;
			if (!stralloc_cats(&sa, " shutted down...\"\n")) retstralloc(0, "write_control") ;
		}
		goto write ;
	}
	
	if (file == CRASH)
	{
		if (!stralloc_cats(&sa,EXECLINE_BINPREFIX "redirfd -r 0 /dev/console\n" \
								EXECLINE_BINPREFIX "redirfd -w 1 /dev/console\n" \
								EXECLINE_BINPREFIX "fdmove -c 2 1\n" \
								S6_PORTABLE_UTILS_BINPREFIX "s6-echo -- \"scandir ")) retstralloc(0, "write_controlboot") ;
		if (!stralloc_cats(&sa, bscan)) retstralloc(0, "write_controlboot") ;
		if (!stralloc_cats(&sa, " crashed ... \"\n")) retstralloc(0, "write_controlboot") ;
		if (!stralloc_cats(&sa, "/bin/sh -i\n")) retstralloc(0, "write_controlboot") ;
		goto write ;
	}
	
	if (file == USR1)
	{
			if (!stralloc_cats(&sa, S6_BINPREFIX "s6-svscanctl -an -- ")) retstralloc(0, "write_controlboot") ;
			if (!stralloc_cats(&sa,bscan)) retstralloc(0, "write_controlboot") ;
			if (!stralloc_cats(&sa,"\n")) retstralloc(0, "write_controlboot") ;
			goto write ;
	}
	if (BOOT)
	{
		if (!stralloc_cats(&sa, EXECLINE_BINPREFIX "foreground { ")) retstralloc(0, "write_controlboot") ;
		if (!stralloc_cats(&sa, stage2tini)) retstralloc(0, "write_controlboot") ;
		if (!stralloc_cats(&sa, " }\n")) retstralloc(0, "write_controlboot") ;
	}
	else
	{
		if (!stralloc_cats(&sa, EXECLINE_BINPREFIX "foreground { " SS_BINPREFIX "66-all -v3 -l ")) retstralloc(0, "write_control") ;
		if (!stralloc_cats(&sa, tree)) retstralloc(0, "write_control") ;
		if (!stralloc_cats(&sa, " down }\n")) retstralloc(0, "write_control") ;
	}
	switch(file)
	{
		case USR2: 
			if (!stralloc_cats(&sa, S6_BINPREFIX "s6-svscanctl -0 -- ")) retstralloc(0, "write_controlboot") ;
			break ;
		case TERM:
			if (!stralloc_cats(&sa, S6_BINPREFIX "s6-svscanctl -t -- ")) retstralloc(0, "write_controlboot") ;
			break ;
		case QUIT:
			if (!stralloc_cats(&sa, S6_BINPREFIX "s6-svscanctl -7 -- ")) retstralloc(0, "write_controlboot") ;
			break ;
		case INT:
			if (!stralloc_cats(&sa, S6_BINPREFIX "s6-svscanctl -6 -- ")) retstralloc(0, "write_controlboot") ;
			break ;
		case HUP:
			if (!stralloc_cats(&sa, S6_BINPREFIX "s6-svscanctl -h -- ")) retstralloc(0, "write_controlboot") ;
			break ;
		default:
			break ;
	}
	if (!stralloc_cats(&sa,bscan)) retstralloc(0, "write_controlboot") ;
	if (!stralloc_cats(&sa,"\n")) retstralloc(0, "write_controlboot") ;
	
	write:
		VERBO3 strerr_warnt3x("write file: ",scandir,filename) ;
		if (!file_write_unsafe(scandir,filename,sa.s,sa.len))
		{
			VERBO3 strerr_warnwu3sys("write file: ",scandir,filename) ;
			return 0 ;
		}
		
		char mode[scandirlen + strlen(filename) + 1] ;
		memcpy(mode,scandir,scandirlen) ;
		memcpy(mode + scandirlen,filename,strlen(filename)) ;
		mode[scandirlen + strlen(filename)] = 0 ;
		
		VERBO3 strerr_warnt2x("chmod 0755: ",mode) ;
		if (chmod(mode,0755) < 0)
		{
			VERBO3 strerr_warnwu2sys("chmod: ",mode) ;
			return 0 ;
		}
		VERBO3 strerr_warnt6x("chown directory: ",mode," to: ",OWNERSTR,":",GIDSTR) ;
		if (chown(mode,OWNER,GIDOWNER) < 0)
		{
			VERBO3 strerr_warnwu2sys("chown: ",mode) ;
			return 0 ;
		}
	
	stralloc_free(&sa) ;
	
	return 1 ;
}

int create_scandir(char const *live, char const *scandir, char const *scanname, uid_t owner)
{
	int r,e ;

	size_t scanlen = strlen(scandir) ;
	size_t newlen ;
	size_t blen ;
	
	char scan[scanlen + 11 + 1] ;
	
	stralloc contents = STRALLOC_ZERO ;
	
	/** run/66/scandir/name */	
	memcpy(scan,scandir,scanlen) ;
	newlen = scanlen ;
	scan[newlen] = 0 ;
	blen = newlen ;
	
	r = scan_mode(scan,S_IFDIR) ;
	if (r < 0 || r) { errno = EEXIST ; return 0 ; }
	if (!r)
	{
		VERBO3 strerr_warnt2x("create directory: ",scan) ;
		r = dir_create(scan,0755) ;
		if (!r) 
		{
			VERBO3 strerr_warnwu2sys("create directory: ",scan) ;
			return 0 ;
		}

		VERBO3 strerr_warnt6x("chown directory: ",scan," to: ",OWNERSTR,":",GIDSTR) ;
		if (chown(scan,OWNER,GIDOWNER) < 0)
		{
			VERBO3 strerr_warnwu2sys("chown: ",scan) ;
			return 0 ;
		}
	}
	
	/** run/66/scandir/name/.svscan */
	memcpy(scan + newlen, "/.s6-svscan", 11) ;
	newlen = newlen + 11 ;
	scan[newlen] = 0 ;
	r = scan_mode(scan,S_IFDIR) ;
	if (r < 0) { errno = EEXIST ; return 0 ; }
	if (!r)
	{	
		VERBO3 strerr_warnt2x("create directory: ",scan) ;
		r = dir_create(scan,0755) ;
		if (!r) 
		{
			VERBO3 strerr_warnwu2sys("create directory: ",scan) ;
			return 0 ;
		}
		VERBO3 strerr_warnt6x("chown directory: ",scan," to: ",OWNERSTR,":",GIDSTR) ;
		if (chown(scan,OWNER,GIDOWNER) < 0)
		{
			VERBO3 strerr_warnwu2sys("chown: ",scan) ;
			return 0 ;
		}
	}
	
	char const *const file[] = { "/crash" ,
								 "/finish" ,
								 "/SIGHUP" ,
								 "/SIGINT" ,
								 "/SIGQUIT" ,
								 "/SIGTERM" ,
								 "/SIGUSR1" ,
								 "/SIGUSR2" } ;
	
	/** we do not exit here in case of error,
	 * those file are not mandatory as far as /.s6-svscan
	 * directory exit but warn the user in every case in case of failure */
	e = errno ;
	for (int i = 0 ; i < 8; i++)
	{	
		write_control(scan,live,file[i],i) ;
	}
	errno = e ;
	
	scan[blen] = 0 ;
	
	if (BOOT)
	{
		if (!write_bootlog(live, scan, scanname)) return 0 ;
	}
	else if (!write_log(scan,scanname)) return 0 ;
	
	stralloc_free(&contents) ;
	
	return 1 ;
}

int sanitize_live(char const *live, char const *scandir, char const *scanname, uid_t owner)
{
	int r ;
	size_t livelen = strlen(live) ;
	size_t scandirlen = strlen(scandir) ;
	size_t scannamelen = strlen(scanname) ;
	
	char basescan[scandirlen + 1 + scannamelen + 1] ;
	char tree[livelen + 4 + 1] ;
	char baselog[livelen + 3 + 1] ;
	/** run/66 */
	r = scan_mode(live,S_IFDIR) ;
	if (r < 0) { errno = EEXIST ; return 0 ; }
	if (!r)
	{
		VERBO3 strerr_warnt2x("create directory: ",live) ;
		r = dir_create(live,0755) ;
		if (!r) 
		{
			VERBO3 strerr_warnwu2sys("create directory: ",live) ;
			return 0 ;
		}
		VERBO3 strerr_warnt6x("chown directory: ",live," to: ",OWNERSTR,":",GIDSTR) ;
		if (chown(live,OWNER,GIDOWNER) < 0)
		{
			VERBO3 strerr_warnwu2sys("chown: ",live) ;
			return 0 ;
		}
	}
	memcpy(basescan,scandir,scandirlen - (scannamelen + 1)) ;
	basescan[scandirlen - (scannamelen + 1)] = 0 ;
	/** run/66/scandir */
	r = scan_mode(basescan,S_IFDIR) ;
	if (r < 0) { errno = EEXIST ; return 0 ; }
	if (!r)
	{	
		VERBO3 strerr_warnt2x("create directory: ",basescan) ;
		r = dir_create(basescan,0755) ;
		if (!r) 
		{
			VERBO3 strerr_warnwu2sys("create directory: ",basescan) ;
			return 0 ;
		}
		/** permissions 1777 */
		VERBO3 strerr_warnt2x("chmod 1777: ",basescan) ;
		if (chmod(basescan,S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO) < 0)
		{
			VERBO3 strerr_warnwu2sys("chmod: ",basescan) ;
			return 0 ;
		}
		VERBO3 strerr_warnt6x("chown directory: ",basescan," to: ",OWNERSTR,":",GIDSTR) ;
		if (chown(basescan,OWNER,GIDOWNER) < 0)
		{
			VERBO3 strerr_warnwu2sys("chown: ",basescan) ;
			return 0 ;
		}
	}
	/** run/66/tree */
	memcpy(tree, live, livelen) ;
	memcpy(tree + livelen,"tree",4) ;
	tree[livelen + 4] = 0  ;
	r = scan_mode(tree,S_IFDIR) ;
	if (r < 0) { errno = EEXIST ; return 0 ; }
	if (!r)
	{
		VERBO3 strerr_warnt2x("create directory: ",tree) ;
		r = dir_create(tree,0755) ;
		if (!r)
		{
			VERBO3 strerr_warnwu2sys("create directory: ",tree) ;
			return 0 ;
		}
		VERBO3 strerr_warnt2x("chmod 1777: ",tree) ;
		if (chmod(tree,S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO) < 0)
		{
			VERBO3 strerr_warnwu2sys("chmod: ",tree) ;
			return 0 ;
		}
		VERBO3 strerr_warnt6x("chown directory: ",tree," to: ",OWNERSTR,":",GIDSTR) ;
		if (chown(tree,OWNER,GIDOWNER) < 0)
		{
			VERBO3 strerr_warnwu2sys("chown: ",tree) ;
			return 0 ;
		}
	}
	/** run/66/log */
	memcpy(baselog, live, livelen) ;
	memcpy(baselog + livelen,"log",3) ;
	baselog[livelen + 3] = 0  ;
	
	r = scan_mode(baselog,S_IFDIR) ;
	if (r < 0) { errno = EEXIST ; return 0 ; }
	if (!r)
	{	
		VERBO3 strerr_warnt2x("create directory: ",baselog) ;
		r = dir_create(baselog,0755) ;
		if (!r)
		{
			VERBO3 strerr_warnwu2sys("create directory: ",baselog) ;
			return 0 ;
		}
		VERBO3 strerr_warnt2x("chmod 1777: ",baselog) ;
		if (chmod(baselog,S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO) < 0)
		{
			VERBO3 strerr_warnwu2sys("chmod: ",baselog) ;
			return 0 ;
		}
		VERBO3 strerr_warnt6x("chown directory: ",baselog," to: ",OWNERSTR,":",GIDSTR) ;
		if (chown(baselog,OWNER,GIDOWNER) < 0)
		{
			VERBO3 strerr_warnwu2sys("chown: ",baselog) ;
			return 0 ;
		}
	}
	
	return 1 ;
}

size_t make_env(char const *src,char const *const *envp,char const **newenv)
{
	stralloc sa = STRALLOC_ZERO ;
	
	size_t envlen = env_len(envp);
	size_t envdirlen = envdir(src,&sa) ;
	size_t newlen = envlen + envdirlen + 1 ;
	if (newlen > MAXENV)
	{
		VERBO3 strerr_warnw2x("to many key on envdir: ",src) ;
		return 0 ;
	} 
	if (!env_merge(newenv, newlen, envp, envlen, sa.s, sa.len))
	{
		VERBO3 strerr_warnwu2x("merge environment from: ",src) ;
		stralloc_free(&sa) ;
		return 0 ;
	}
	
	return newlen ;
}

int main(int argc, char const *const *argv, char const *const *envp)
{
	int r ;
	unsigned int up, down, rescan, create, remove ;
	
	stralloc live = STRALLOC_ZERO ;
	stralloc livetree = STRALLOC_ZERO ;
	stralloc scandir = STRALLOC_ZERO ;
	stralloc envdir = STRALLOC_ZERO ;
	stralloc signal = STRALLOC_ZERO ;
	
	char const *newenv[MAXENV] ;
	char const *const *genv = NULL ;
	
	up = down = rescan = create = remove = 0 ;
	
	PROG = "66-scandir" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, "hv:bl:t:3:2:e:crus:", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'h' : info_help(); return 0 ;
				case 'v' : if (!uint0_scan(l.arg, &VERBOSITY)) exitusage() ; break ;
				case 'b' : BOOT = 1 ; break ;
				case 'l' : if(!stralloc_cats(&live,l.arg)) retstralloc(111,"main") ;
						   if(!stralloc_0(&live)) retstralloc(111,"main") ;
						   break ;
				
				case 't' : if (uint0_scan(l.arg, &rescan)) break ;
				case '2' : stage2tini = l.arg ; break ;
				case '3' : stage3 = l.arg ; break ;
				case 'e' : if(!stralloc_cats(&envdir,l.arg)) retstralloc(111,"main") ;
						   if(!stralloc_0(&envdir)) retstralloc(111,"main") ;
						   break ;
				case 'c' : create = 1 ; if (remove) exitusage() ; break ;
				case 'r' : remove = 1 ; if (create) exitusage() ; break ;
				case 'u' : up = 1 ; if (down) exitusage() ; break ;
				case 's' : down = 1 ; if (up) exitusage() ; 
						   if(!stralloc_cats(&signal,l.arg)) retstralloc(111,"main") ;
						   if(!stralloc_0(&signal)) retstralloc(111,"main") ;
						   break ;
				default : exitusage() ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc > 1) exitusage() ;
	
	if (!argc) OWNER = MYUID ;
	else 
	{
		if (!YOURUID(&OWNER,*argv)) strerr_diefu1sys(111,"set owner") ;
	}
	if (BOOT && OWNER) strerr_dief1x(110,"-b options can be set only with root") ; 
	size_t OWNERLEN = uid_fmt(OWNERSTR,OWNER) ;
	OWNERSTR[OWNERLEN] = 0 ;
	
	if (!YOURGID(&GIDOWNER,OWNER)) strerr_diefu2sys(111,"set group for owner: ",OWNERSTR) ;
	size_t GIDOWNERLEN = gid_fmt(GIDSTR,GIDOWNER) ;
	GIDSTR[GIDOWNERLEN] = 0 ; 
		
	/** live -> /run/66/ */
	r = set_livedir(&live) ;
	if (r < 0) strerr_dief3x(110,"live: ",live.s," must be an absolute path") ;
	if (!r) strerr_diefu1sys(111,"set live directory") ;
	
	if (!stralloc_copy(&scandir,&live)) retstralloc(111,"main") ;
		
	/** scandir -> /run/66/scandir/ */
	r = set_livescan(&scandir,OWNER) ;
	if (r < 0) strerr_dief3x(110,"scandir: ", scandir.s, " must be an absolute path") ;
	if (!r) strerr_diefu1sys(111,"set scandir directory") ;
	
	size_t slash =  get_rlen_until(scandir.s,'/',scandir.len) + 1 ;// +1 remove first '/'
	size_t scannamelen = scandir.len - slash ;
	char scanname[scannamelen] ;
	memcpy(scanname, scandir.s + slash ,scannamelen) ;
	scanname[scannamelen] = 0 ;
	
	 if (stage2tini[0] != '/')
		strerr_dief3x(110, "stage2: ",stage2tini," is not absolute") ;
	
	 if (stage3[0] != '/')
		strerr_dief3x(110, "stage3: ",stage3," is not absolute") ;
	
	if (envdir.len)
	{
		r = dir_scan_absopath(envdir.s) ;
		if (r < 0)
			strerr_dief3x(110,"environment: ",envdir.s," must be an absolute path") ;
		
		if (!make_env(envdir.s,envp,newenv)) strerr_diefu2x(111,"parse environment directory; ",envdir.s) ;
		genv = newenv ;
	}
	else
	{
		genv = envp ;
	}

	r = scan_mode(scandir.s, S_IFDIR) ;
	if (r < 0) strerr_dief3x(111,"scandir: ",scandir.s," exist with unkown mode") ;
	if (!r && !create && !remove) strerr_dief3x(110,"scandir: ",scandir.s," doesn't exist") ;
	if (!r && create)
	{
		VERBO2 strerr_warni3x("sanitize ",live.s," ...") ;
		if (!sanitize_live(live.s,scandir.s,scanname,OWNER)) strerr_diefu2sys(111,"create: ",live.s) ;
		VERBO2 strerr_warni3x("create scandir ",scandir.s," ...") ;
		if (!create_scandir(live.s, scandir.s,scanname, OWNER)) strerr_diefu2sys(111,"create scandir: ", scandir.s) ;
	}
	if (r && create)
		VERBO2 strerr_warni3x("scandir: ",scandir.s," already exist, keep it") ;
			
	/**swap to char [] to be able to freed stralloc*/
	char rlive[scandir.len + 1] ;
	memcpy(rlive,scandir.s,scandir.len) ;
	rlive[scandir.len] = 0 ;
		
	if (!remove)
	{
		stralloc_free(&scandir) ;
		stralloc_free(&livetree) ;
	}
	else
	{
		if (!stralloc_copy(&livetree,&live)) retstralloc(111,"main") ;
		r = set_livetree(&livetree,OWNER) ;
		if (r < 0) strerr_dief3x(110,"scandir: ", livetree.s, " must be an absolute path") ;
		if (!r) strerr_diefu1sys(111,"set livetree directory") ;
	}
	stralloc_free(&live) ;
		
	if (down)
	{
		scandir_down(rlive,signal.s,genv) ;
	}
	if (up)
	{
		scandir_up(rlive,rescan,genv) ;
	}
	
	if (remove)
	{
		r = scandir_ok(rlive) ;
		if (r < 0) strerr_diefu2sys(111, "check: ", scandir.s) ;
		if (r) strerr_diefu3x(110,"remove: ",rlive,": is running")  ;
		
		r = scan_mode(rlive,S_IFDIR) ;
		if (r)
		{
			VERBO2 strerr_warni3x("removing ",rlive," ...") ;
			if (rm_rf(rlive) < 0) strerr_diefu2sys(111,"remove: ",rlive) ;
		}
		else VERBO2 strerr_warni2x(rlive," doesn't exist") ;
		
		r = scan_mode(livetree.s,S_IFDIR) ;
		if (r)
		{
			VERBO2 strerr_warni3x("removing ",livetree.s," ...") ;
			if (rm_rf(livetree.s) < 0) strerr_diefu2sys(111,"remove: ",livetree.s) ;
		}
		
	}
	stralloc_free(&livetree) ;
	stralloc_free(&scandir) ;
	stralloc_free(&envdir) ;
	stralloc_free(&signal) ;
	
	return 0 ;
}
	

