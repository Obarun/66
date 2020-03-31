/* 
 * 66-scanctl.c
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
 
#include <string.h>
#include <oblibs/log.h>
#include <oblibs/obgetopt.h>

#include <skalibs/buffer.h>
#include <skalibs/stralloc.h>
#include <skalibs/types.h>

#include <66/utils.h>

#define SIGSIZE 64

#define USAGE "66-scanctl [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -o owner ] signal"

static inline void info_help (void)
{
  static char const *help =
"66-scanctl <options> signal\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-z: use color\n"
"	-v: increase/decrease verbosity\n"
"	-l: live directory\n"
"	-o: handle scandir of owner\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
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
  if (!signal_table[i]) i = 7 ;
  return i ;
}

int send_signal(char const *scandir, char const *signal)
{
	unsigned int sig = 5 ;
	size_t siglen = strlen(signal) ;
	char csig[SIGSIZE + 1] ;
	sig = parse_signal(signal) ;
	if (sig < 6)
	{
		switch(sig)
		{
			case 0: csig[0] = 'a' ; 
					csig[1] = 'n' ;
					csig[2] = 0 ;
					break ;
			case 1: csig[0] = 'i' ;
					csig[1] = 0 ;
					break ;
			case 2: csig[0] = 'q' ; 
					csig[1] = 0 ;
					break ;
			case 3: csig[0] = '0' ;
					csig[1] = 0 ;
					break ;
			case 4: csig[0] = '6' ;
					csig[1] = 0 ;
					break ;
			case 5: csig[0] = '7' ;
					csig[1] = 0 ;
					break ;
			default: break ;
		}
	}
	else
	{
		memcpy(csig,signal,siglen) ;
		csig[siglen] = 0 ;
	}
	log_info("Sending -",csig," signal to scandir: ",scandir,"...") ;
	return scandir_send_signal(scandir,csig) ;
}

int main(int argc, char const *const *argv)
{
	int r ;
	uid_t owner = MYUID ;
	char const *signal ;
	stralloc scandir = STRALLOC_ZERO ;
	
	log_color = &log_color_disable ;
	
	PROG = "66-scanctl" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">hv:l:o:z", &l) ;
			if (opt == -1) break ;
			if (opt == -2) log_die(LOG_EXIT_USER,"options must be set first") ;
			switch (opt)
			{
				case 'h' : info_help(); return 0 ;
				case 'v' : if (!uint0_scan(l.arg, &VERBOSITY)) log_usage(USAGE) ; break ;
				case 'l' : if (!stralloc_cats(&scandir,l.arg)) log_die_nomem("stralloc") ;
						   if (!stralloc_0(&scandir)) log_die_nomem("stralloc") ;
						   break ;
				case 'o' :
						if (MYUID) log_die(LOG_EXIT_USER, "only root can use -o options") ;
						else if (!youruid(&owner,l.arg)) log_dieusys(LOG_EXIT_SYS,"get uid of: ",l.arg) ;
						break ;
				case 'z' :	log_color = !isatty(1) ? &log_color_disable : &log_color_enable ; break ;
				default : log_usage(USAGE) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc < 1) log_usage(USAGE) ;
	signal = argv[0] ;
	r = set_livedir(&scandir) ;
	if (r < 0) log_die(LOG_EXIT_USER,"live: ",scandir.s," must be an absolute path") ;
	if (!r) log_dieusys(LOG_EXIT_SYS,"set live directory") ;
	r = set_livescan(&scandir,owner) ;
	if (r < 0) log_die(LOG_EXIT_USER,"scandir: ", scandir.s, " must be an absolute path") ;
	if (!r) log_dieusys(LOG_EXIT_SYS,"set scandir directory") ;
	r = scandir_ok(scandir.s) ;
	if (!r)log_diesys(LOG_EXIT_SYS,"scandir: ",scandir.s," is not running") ;
	else if (r < 0) log_dieusys(LOG_EXIT_SYS, "check: ", scandir.s) ;
	
	if (send_signal(scandir.s,signal) <= 0) goto err ;			
	
	stralloc_free(&scandir) ;
	return 0 ;
	err:
		stralloc_free(&scandir) ;
		return 111 ;	
}
	

