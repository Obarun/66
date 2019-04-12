/* 
 * 66-getenv.c
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
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <proc/readproc.h>
#include <regex.h>

#include <skalibs/sgetopt.h>
#include <skalibs/buffer.h>
#include <skalibs/strerr2.h>

#define MAX_ENV 4095
static char const *delim = "\n" ;
static char const *pattern = 0 ;
static unsigned int EXACT = 0 ;

#define USAGE "66-getenv [ -h ] [ -x ] [ -d delim ] process"
#define dieusage() strerr_dieusage(100, USAGE)

static inline void info_help (void)
{
  static char const *help =
"66-getenv <options> process\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-d: specify output delimiter\n"
"	-x: match exactly with the process name\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
}

static PROCTAB *open_proc (void)
{
	PROCTAB *ptp ;
	int flags = PROC_FILLCOM | PROC_FILLENV ;

	ptp = openproc (flags) ;
	
	return (ptp) ;
}

static regex_t *regex_cmp (void)
{
	regex_t *preg = 0 ;
	size_t plen = strlen(pattern) ;
	char re[plen + 4 + 1] ;
	char errbuf[256] ;
	int r ;
	
	preg = malloc (sizeof (regex_t)) ;
	if (!preg) strerr_diefu1sys(111,"allocate preg") ;
	if (EXACT)
	{
		memcpy(re,"^(",2) ;
		memcpy(re + 2,pattern,plen) ;
		memcpy(re + 2 + plen,")$",2) ;
		re[2 + plen + 2] = 0 ;
	}
	else
	{
		memcpy(re,pattern,plen) ;
		re[plen] = 0 ;
	}
	
	r = regcomp (preg, re, REG_EXTENDED | REG_NOSUB) ;
	if (r)
	{
		regerror (r, preg, errbuf, sizeof(errbuf)) ;
		strerr_diefu1x(111,errbuf) ;
	}
	
	return preg ;
}

void get_procs ()
{
	PROCTAB *ptp ;
	proc_t task ;
	
	int i = 0 ;
	regex_t *preg ;
	pid_t myself = getpid() ;
	char cmd[MAX_ENV+1] ;
	int bytes ;
	char *str ;
	
	ptp = open_proc() ;
	preg = regex_cmp() ;
	
	memset (&task, 0, sizeof (task)) ;
	while (readproc (ptp, &task)) 
	{
		int found = 1 ;
	
		if (task.XXXID == myself) continue ;
		
		if (!task.cmdline) continue ;
		
		{
			i = 0 ;
			bytes = sizeof(cmd) ;
			str = cmd ;
			/* make sure it is always NUL-terminated */
			*str = 0 ;

			while (task.cmdline[i] && bytes > 1)
			{
				const int len = snprintf(str, bytes, "%s%s", i ? " " : "", task.cmdline[i]) ;
				if (len < 0) {
					*str = 0 ;
					break ;
				}
				if (len >= bytes) break ;
				str += len ;
				bytes -= len ;
				i++ ;
			}
		}
		cmd[strlen(cmd)] = 0 ;
		if (regexec (preg, cmd, 0, NULL, 0) != 0)
			found = 0 ;
		
		if (!task.environ) continue ;		
		
		if (found) 
		{	
			for(;*task.environ;task.environ++)
			{
				if ((buffer_putsflush(buffer_1, *task.environ) < 0) ||
				(buffer_putsflush(buffer_1, delim) < 0)) strerr_diefu1sys(111, "write to stdout") ;	
			}
			break ;
		}
		memset (&task, 0, sizeof (task)) ;
	}
	regfree(preg) ;
	closeproc(ptp) ;
}

int main (int argc, char const *const *argv, char const *const *envp)
{
	
	PROG = "66-getenv" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;
		for (;;)
		{
			int opt = subgetopt_r(argc, argv, "hxd:", &l) ;
			if (opt == -1) break ;
			switch (opt)
			{
				case 'h' : info_help() ; return 0 ;
				case 'x' : EXACT = 1 ; break ;
				case 'd' : delim = l.arg ; break ;
				default : dieusage() ;
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	if (argc < 1) dieusage() ;
	pattern = argv[0] ;

 	get_procs() ;
	
	return 0 ;	
}



