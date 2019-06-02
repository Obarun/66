/* 
 * 66-scanctl.c
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
#include <oblibs/error2.h>
#include <oblibs/obgetopt.h>

#include <skalibs/buffer.h>
#include <skalibs/stralloc.h>
#include <skalibs/types.h>

#include <66/utils.h>

#define SIGSIZE 64

#define USAGE "66-scanctl [ -h ] [ -v verbosity ] [ -l live ] [ -o owner ] signal"

unsigned int VERBOSITY = 1 ;

static inline void info_help (void)
{
  static char const *help =
"66-scanctl <options> signal\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-v: increase/decrease verbosity\n"
"	-l: live directory\n"
"	-o: handle scandir of owner\n"
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
	VERBO1 strerr_warni5x("Sending -",csig," signal to scandir: ",scandir," ...") ;
	return scandir_send_signal(scandir,csig) ;
}

int main(int argc, char const *const *argv)
{
	int r ;
	uid_t owner = MYUID ;
	char const *signal ;
	stralloc scandir = STRALLOC_ZERO ;
		
	PROG = "66-scanctl" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, ">hv:l:o:", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'h' : info_help(); return 0 ;
				case 'v' : if (!uint0_scan(l.arg, &VERBOSITY)) exitusage(USAGE) ; break ;
				case 'l' : if (!stralloc_cats(&scandir,l.arg)) retstralloc(111,"main") ;
						   if (!stralloc_0(&scandir)) retstralloc(111,"main") ;
						   break ;
				case 'o' :
						if (MYUID) strerr_dief1x(110, "only root can use -o options") ;
						else if (!youruid(&owner,l.arg)) strerr_diefu2sys(111,"get uid of: ",l.arg) ;
						break ;
				default : exitusage(USAGE) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc < 1) exitusage(USAGE) ;
	signal = argv[0] ;
	r = set_livedir(&scandir) ;
	if (r < 0) strerr_dief3x(110,"live: ",scandir.s," must be an absolute path") ;
	if (!r) strerr_diefu1sys(111,"set live directory") ;
	r = set_livescan(&scandir,owner) ;
	if (r < 0) strerr_dief3x(110,"scandir: ", scandir.s, " must be an absolute path") ;
	if (!r) strerr_diefu1sys(111,"set scandir directory") ;
	r = scandir_ok(scandir.s) ;
	if (!r) strerr_dief3sys(111,"scandir: ",scandir.s," is not running") ;
	else if (r < 0) strerr_diefu2sys(111, "check: ", scandir.s) ;
	
	if (send_signal(scandir.s,signal) <= 0) goto err ;			
	
	stralloc_free(&scandir) ;
	return 0 ;
	err:
		stralloc_free(&scandir) ;
		return 111 ;	
}
	

