/* 
 * 66-svctl.c
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
#include <stdlib.h>

#include <oblibs/obgetopt.h>
#include <oblibs/error2.h>
#include <oblibs/types.h>
#include <oblibs/string.h>
#include <oblibs/stralist.h>

#include <skalibs/buffer.h>
#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/bytestr.h>
#include <skalibs/selfpipe.h>
#include <skalibs/iopause.h>
#include <skalibs/sig.h>

#include <s6/s6-supervise.h>//s6_svstatus_t
#include <s6/ftrigw.h>
#include <s6/ftrigr.h>

#include <66/constants.h>
#include <66/utils.h>
#include <66/tree.h>


unsigned int VERBOSITY = 1 ;
stralloc svkeep = STRALLOC_ZERO ;
genalloc gakeep = GENALLOC_ZERO ; //type svc_sig_s
stralloc saresolve = STRALLOC_ZERO ;
typedef struct svc_sig_s svc_sig, *svc_sig_t_ref ;
struct svc_sig_s
{
	unsigned int scan ; //pos in sv
	size_t scanlen ;
	unsigned int name ; //pos in sv
	size_t namelen ;
	unsigned int src ; //pos in sv
	size_t srclen ;
	unsigned int notify ;
	unsigned int ndeath;
	tain_t deadline ;
} ;

#define SVC_SIG_ZERO \
{ \
	.scan = 0 , \
	.name = 0 , \
	.namelen = 0 , \
	.src = 0 , \
	.srclen = 0, \
	.notify = 0, \
	.ndeath = 3, \
	.deadline = TAIN_ZERO \
}

unsigned int SV_DEADLINE = 1000 ;

typedef enum state_e state_t, *state_t_ref ;
enum state_e
{
	UP = 0, // u
	RUP , // U - really up
	DOWN , // d
	RDOWN ,// D - really down
	SUP ,//supervise
	O , //0
	X //x
} ;
typedef enum actions_e actions_t, *actions_t_ref ;
enum actions_e
{
	GOTIT = 0 ,
	WAIT ,
	DEAD ,
	DONE ,
	PERM ,
	OUT ,
	UKNOW 
} ;

int SIGNAL = -1 ;

#define USAGE "66-svctl [ -h help ] [ -v verbosity ] [ -l live ] [ -t tree ] [ -T timeout ] [ -S service timeout ] [ -n death ] [ -u|U up ] [ -d|D down ] service(s)"

static inline void info_help (void)
{
  static char const *help =
"66-svctl <options> tree\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-v: increase/decrease verbosity\n"
"	-l: live directory\n"
"	-t: tree to use\n"
"	-T: general timeout\n"
"	-S: service timeout\n"
"	-n: number of death\n"
"	-u: bring up service\n"
"	-U: really up\n"
"	-d: bring down service\n"
"	-D: really down\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
}

static int read_file (char const *file, char *buf, size_t n)
{
	ssize_t r = openreadnclose_nb(file, buf, n) ;
	if (r < 0)
	{
		if (errno != ENOENT) VERBO3 strerr_warnwu2sys("open: ", file) ;
		return 0 ;
	}
	buf[byte_chr(buf, r, '\n')] = 0 ;
	return 1 ;
}

static int read_uint (char const *file, unsigned int *fd)
{
	char buf[UINT_FMT + 1] ;
	if (!read_file(file, buf, UINT_FMT)) return 0 ;
	if (!uint0_scan(buf, fd))
	{
		VERBO3 strerr_warnw2x("invalid: ", file) ;
		return 0 ;
	}
	return 1 ;
}

/** @Return -1 on error
 * @Return 0 on fail
 * @Return 1 on success */
int handle_signal_svc(svc_sig *sv_signal)
{
	s6_svstatus_t status = S6_SVSTATUS_ZERO ;
	char *sv = svkeep.s + sv_signal->scan ;
	if (!s6_svstatus_read(sv,&status))
	{ 
		VERBO3 strerr_warnwu2sys("read status of: ",sv) ;
		return 0 ;
	}
	
	int exitcode, exitstatus ;
	for (;;)
	{
		int sig = selfpipe_read() ;
		switch (sig)
		{
			case -1:	VERBO3 strerr_warnwu1sys("selfpipe_read()") ;
						return -1 ;
			case 0: 	
				exitcode = WIFEXITED(status.wstat) ;
				exitstatus = WEXITSTATUS(status.wstat) ;
			
				if (exitcode && exitstatus <= 1) 
					return 1 ;
				else
					return 0 ;
			case SIGTERM:
			case SIGINT:
				/** TODO, little ugly for me
				 * KILL work but s6-supervice reload it*/
				if (SIGNAL == UP || SIGNAL == RUP)
					s6_svc_writectl(sv, S6_SUPERVISE_CTLDIR, "d", 1) ;
			//	kill(status.pid, SIGKILL) ;
				return -1 ;
			default : VERBO3 strerr_warnw1x("inconsistent signal state") ; return 0 ;
		}
	}
}

/** we use three file more for s6-svc service :
 * 	 
 *	 timeout-up and timeout-down will define the deadline for the iopause
 * 		each service contain his deadline into the sv_signal.deadline key.
 * 		If thoses files doesn't exit 1000 millisec is set by default.
 * 		This deadline value can be set on commandline by the -S options.
 *	 max-death-tally
 * 		We let s6-supervise make what it need to do with this file but
 * 		we take the same number to set the sv_signal.death value
 * 		accepted for every daemon before exiting if the SIGNAL is not reached.
 * 		If this file doesn't exist a number of 3 death (death -> number 
 * 		of wrong signal received) is set by default.
 * 		This ndeath value can be set on commandline by the -n options.
 *
 * In all case the loop is broken to avoids infinite iopause 
 * 
 * The notication-fd file is used as the original one. If the file 
 * doesn't exist and the SIGNAL is U , we change SIGNAL to u and warn
 * the user about it.
 *
 * In the iopause_svc function a reached timeout is not an absolute error
 * because we can have a notification-file in the servicedirs of the
 * service with a deamon which doesn't support readiness notification. 
 * In this case, we send U and only receive u as event, the daemon is
 * running but not notified by the daemon itself.
 * We accept this case as an "half" success. We exit with success but
 * the user is warned about the state. */
 	
/** @Return -2 on SIGINT reception 
 * @Return -1 on error 
 * @Return 0 on fail
 * @Return 1 on success
 * @Return 2 if signal is not complete (e.g. want U receive only u)
 * @Return 3 for PERMANENT failure
 * @Return 4 is a last status checking is need */
int iopause_svc(svc_sig *sv_signal,char const *sig, ftrigr_t *fifo)
{

	int id ;
	
	stralloc sa = STRALLOC_ZERO ;
	
	static unsigned char const actions[7][8] = 
	{
 //signal receive:
 //		c->UP,		RUP,	DOWN,	RDOWN 	O		X		SUP		UNKNOW
	    { GOTIT,	DONE,	DEAD,	DEAD,	PERM,	PERM, 	DEAD,	UKNOW }, 	// u
	    { WAIT,		GOTIT,	DEAD, 	DEAD, 	PERM,	PERM, 	DEAD,	UKNOW }, 	// U signal
	    { DEAD,		DEAD,	GOTIT, 	DONE,	DONE,	DONE,	DEAD,	UKNOW },	// d wanted
	    { DEAD,		DEAD,	WAIT, 	GOTIT, 	DONE,	DONE, 	DEAD,	UKNOW }, 	// D
	    { DEAD,		DEAD,	DEAD, 	DEAD, 	OUT,	OUT, 	GOTIT,	UKNOW }, 	// s
	    { PERM,		PERM,	WAIT, 	WAIT, 	GOTIT,	WAIT, 	PERM,	UKNOW }, 	// O
	    { PERM,		PERM,	DONE, 	GOTIT, 	DONE,	GOTIT, 	PERM,	UKNOW }, 	// x
																
	
	} ;
//	convert signal receive into enum number
	static const uint8_t chtenum[128] = 
	{	
		0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
		0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
		0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
		0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
		0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
		0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
		0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
		0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
		0 ,  0 ,  0 ,  0 ,  3 ,  0 ,  0 ,  0 , 
		0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  5 , 
		0 ,  0 ,  0 ,  0 ,  0 ,  1,   0 ,  0 , 
		0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
		0 ,  0 ,  0 ,  0 ,  2 ,  0 ,  0 ,  0 , 
		0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
		0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
		6 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 
	} ;

	int spfd = selfpipe_init() ;
	if (spfd < 0){ VERBO3 strerr_warnwu1sys("init selfpipe") ; return -1 ; }
	{
		sigset_t set ;
		sigemptyset(&set) ;
		sigaddset(&set, SIGINT) ;
		sigaddset(&set, SIGTERM) ;
		if (selfpipe_trapset(&set) < 0)
		{
			VERBO3 strerr_warnwu1sys("trap signals") ;
			return -1 ;
		}
	}

	iopause_fd x[2] = { { .fd = ftrigr_fd(fifo), .events = IOPAUSE_READ }, { .fd = spfd, .events = IOPAUSE_READ, .revents = 0 } } ;
	
	unsigned int t, h, p, state, err, check ;
		
	err = h = t = check = 0 ;
	state = SIGNAL ;
	
	tain_now_g() ;
	tain_add_g(&sv_signal->deadline,&sv_signal->deadline) ;

	int n = sv_signal->ndeath ;
	/** loop even number of death is reached */
	while(n>0)
	{
		int r = iopause_g(x, 2, &sv_signal->deadline) ;
		if (r < 0){ VERBO3 strerr_warnwu1sys("iopause") ; err = -1 ; goto end ; }
		/** timeout is not a error , the iopause process did right */
		else if (!r) { VERBO3 strerr_warnt2x("timed out reached for: ",svkeep.s + sv_signal->scan) ; t = 1 ; goto end ; } 
		if (x[1].revents & IOPAUSE_READ)
			if (handle_signal_svc(sv_signal) < 0){ err = -2 ; goto end ; }
		
		if (x->revents & IOPAUSE_READ)  
		{	
			
			id = ftrigr_updateb(fifo) ;
			if (id < 0){ VERBO3 strerr_warnwu1sys("ftrigr_update") ; return -1 ; } 
			if (id)
			{
				sa.len = 0 ;
				r = ftrigr_checksa(fifo, id, &sa) ;
				if (r < 0) { VERBO3 strerr_warnwu1sys("ftrigr_update") ; return -1 ; } 
				if (r)
				{
					VERBO3
					{
						buffer_putsflush(buffer_1,"66-svctl: tracing: ") ;
						buffer_putsflush(buffer_1,"signal received: ") ;
						buffer_putflush(buffer_1,sa.s,1) ;
						buffer_putsflush(buffer_1,"\n") ;
					}
					for (int i = 0;i < sa.len ; i++)
					{	
						
						p = chtenum[(unsigned char)sa.s[i]] ;
						{
							unsigned char action = actions[state][p] ;
						
							switch (action)
							{
								case GOTIT:
									h = handle_signal_svc(sv_signal) ;
									if (h)
									{
										check = 4 ;
										break ;
									}
									err = 1 ;
									goto end ;
								case WAIT:
									err = 2 ;
									break ;
								case DEAD:
									err = -1 ;
									break ;
								case DONE:
									err = 1 ;
									goto end ;
								case OUT:
									err = 0 ;
									goto end ;
								case PERM:
									err = 3 ;
									goto end ;
								case UKNOW :
								default:
									VERBO3 strerr_warnw1x("invalid state");
									err = -1 ;
									goto end ;
							}
						}
					}
				}
			}
			n-- ;
		}
	}
	end:
		stralloc_free(&sa) ;
		selfpipe_finish() ;
	
		if (err == 2 && sv_signal->ndeath == 1 && !h)
			err = 2 ;
				
		if ((err < 0)  && (t) )
			err = 2 ;
	
		if (check) return check ;

		return err ;
}

int send_svc(svc_sig *sv_signal, char const *sig, ftrigr_t *fifo,tain_t const *ttmain)
{
	int r, id, fdlock ;
	size_t siglen = strlen(sig) ;
	
	size_t scanlen = sv_signal->scanlen ;
	char *svok = svkeep.s + sv_signal->scan ;
	
	char svfifo[scanlen + 6 + 1] ;
	memcpy(svfifo, svok,scanlen) ;
	memcpy(svfifo + scanlen, "/event",6 + 1) ;
	
	VERBO3 strerr_warnt2x("take lock of: ",svok) ;
	fdlock = s6_svc_lock_take(svok) ;
	if (fdlock < 0)
	{
		VERBO3 strerr_warnwu2sys("take lock of: ",svok) ;
		return 0 ;
	}
	VERBO3 strerr_warnt2x("clean up fifo: ", svfifo) ;
	if (!ftrigw_clean (svok))
	{
		VERBO3 strerr_warnwu2sys("clean up fifo: ", svfifo) ;
		return 0 ;
	}
	VERBO3 strerr_warnt2x("subcribe to fifo: ",svfifo) ;
	/** we let opened the fifo for the iopause work */
	id = ftrigr_subscribe_g(fifo, svfifo, "[DuUdOx]", 1, ttmain) ;
	if (!id)
	{
		VERBO3 strerr_warnwu2sys("subcribe to fifo: ",svfifo) ;
		goto errlock ;
	}
	VERBO3 strerr_warnt2x("unlock: ",svok) ;
	s6_svc_lock_release(fdlock) ;
	VERBO3 strerr_warnt6x("send signal: ",sig," to: ", svok,"/",S6_SUPERVISE_CTLDIR) ;
	r = s6_svc_writectl(svok, S6_SUPERVISE_CTLDIR, sig, siglen) ;
	if (r < 0)
	{
		VERBO3 strerr_warnwu3sys("something is wrong with the ",svok, "/" S6_SUPERVISE_CTLDIR " directory. errno reported") ;
		goto errunsub ;
	}
	if (!r)
	{
		VERBO3 strerr_warnw3x("sv: ",svok, " is not supervised") ;
		goto errunsub ;
	}
	VERBO3 strerr_warnt2x("start iopause for: ",svok) ;
	r = iopause_svc(sv_signal,sig,fifo) ;

	if (r < 0)
	{
		VERBO3 strerr_warnt1sys("handle iopause") ;
		return -1 ;
	}

	if (!ftrigr_unsubscribe_g(fifo,id, ttmain)) 
	{
		VERBO3 strerr_warnwu2sys("unsubcribe from: ",svfifo) ;
		return 0 ;
	}
	
	return r ;
	
	errlock:
		s6_svc_lock_release(fdlock) ;
	errunsub:
		if (!ftrigr_unsubscribe_g(fifo,id, &sv_signal->deadline)) VERBO3 strerr_warnwu2sys("unsubcribe from: ",svfifo) ;
		return 0 ;
}

int main(int argc, char const *const *argv,char const *const *envp)
{
	int r, e ;
	
	uid_t owner ;
	
	svc_sig sv_signal = SVC_SIG_ZERO ;
	
	char *treename = 0 ;
	
	stralloc base = STRALLOC_ZERO ;
	stralloc tree = STRALLOC_ZERO ;
	stralloc live = STRALLOC_ZERO ;
	stralloc scandir = STRALLOC_ZERO ;
		
	ftrigr_t fifo = FTRIGR_ZERO ;
	s6_svstatus_t status = S6_SVSTATUS_ZERO ;
	
	tain_t ttmain ; 
	unsigned int deathsv = 0 ;
	unsigned int tmain = 0 ;
	unsigned int tsv = 0 ;
	
	PROG = "66-svctl" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, "hv:l:t:T:S:n:udUD", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'h' : 	info_help(); return 0 ;
				case 'v' :  if (!uint0_scan(l.arg, &VERBOSITY)) exitusage() ; break ;
				case 'l' : 	if (!stralloc_cats(&live,l.arg)) retstralloc(111,"main") ;
							if (!stralloc_0(&live)) retstralloc(111,"main") ;
							break ;
				case 't' : 	if(!stralloc_cats(&tree,l.arg)) retstralloc(111,"main") ;
							if(!stralloc_0(&tree)) retstralloc(111,"main") ;
							break ;
				case 'T' :	if (!uint0_scan(l.arg, &tmain)) exitusage() ; break ;
				case 'S' : 	if (!uint0_scan(l.arg, &tsv)) exitusage() ; break ;
				case 'n' :	if (!uint0_scan(l.arg, &deathsv)) exitusage() ; break ;
				case 'u' :	if (SIGNAL == DOWN || SIGNAL == RDOWN) exitusage() ; SIGNAL = UP ; break ;
				case 'U' :	if (SIGNAL == DOWN || SIGNAL == DOWN) exitusage() ; SIGNAL = RUP ; break ;
				case 'd' : 	if (SIGNAL == UP || SIGNAL == RUP) ; SIGNAL = DOWN ; break ;
				case 'D' :	if (SIGNAL == UP || SIGNAL == RUP) ; SIGNAL = RDOWN ; break ;
				
				default : exitusage() ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc < 1 || (SIGNAL < 0)) exitusage() ;
	
	if (tmain) tain_from_millisecs(&ttmain, tmain) ;
	else ttmain = tain_infinite_relative ;
	
	owner = MYUID ;
	
	if (!set_ownersysdir(&base,owner)) strerr_diefu1sys(111, "set owner directory") ;
	
	r = tree_sethome(&tree,base.s) ;
	if (r < 0) strerr_diefu1x(110,"find the current tree. You must use -t options") ;
	if (!r) strerr_diefu2sys(111,"find tree: ", tree.s) ;
	
	treename = tree_setname(tree.s) ;
	if (!treename) strerr_diefu1x(111,"set the tree name") ;
		
	if (!tree_get_permissions(tree.s))
		strerr_dief2x(110,"You're not allowed to use the tree: ",tree.s) ;
		
	r = set_livedir(&live) ;
	if (!r) retstralloc(111,"main") ;
	if (r < 0 ) strerr_dief3x(111,"live: ",live.s," must be an absolute path") ;
	
	if (!stralloc_copy(&scandir,&live)) retstralloc(111,"main") ;
	
	r = set_livescan(&scandir,owner) ;
	if (!r) retstralloc(111,"main") ;
	if (r < 0 ) strerr_dief3x(111,"scandir: ",scandir.s," must be an absolute path") ;
	
	if ((scandir_ok(scandir.s)) !=1 ) strerr_dief3sys(111,"scandir: ", scandir.s," is not running") ;
	

	for(;*argv;argv++)
	{
		char const *svname = *argv ;
		size_t namelen = strlen(*argv) ;
		size_t srclen = tree.len - 1 ;
		char src[srclen + SS_SVDIRS_LEN + SS_SVC_LEN + 1 + namelen + 16 + 1] ;
		memcpy(src,tree.s,srclen) ;
		memcpy(src + srclen,SS_SVDIRS, SS_SVDIRS_LEN) ;
		memcpy(src + srclen + SS_SVDIRS_LEN, SS_SVC, SS_SVC_LEN) ;
		src[srclen + SS_SVDIRS_LEN + SS_SVC_LEN] = '/' ;
		memcpy(src + srclen + SS_SVDIRS_LEN + SS_SVC_LEN + 1,*argv,namelen) ;
		src[srclen + SS_SVDIRS_LEN + SS_SVC_LEN + 1 + namelen ] = 0 ;
		srclen = srclen + SS_SVDIRS_LEN + SS_SVC_LEN + 1 + namelen ;
		
		size_t svoklen = scandir.len - 1  ;
		char svok[svoklen + 1 + namelen + 1] ;
		memcpy(svok,scandir.s,svoklen) ;
		svok[svoklen] = '/' ;
		memcpy(svok + svoklen + 1 ,svname,namelen + 1) ;
		svoklen = svoklen + 1 + namelen ;
		/** do not check the logger*/
		if (!resolve_pointo(&saresolve,base.s,live.s,tree.s,treename,0,SS_RESOLVE_SRC))
		{
			VERBO3 strerr_warnwu1x("set revolve pointer to source") ;
			return 0 ;
		}
		if (resolve_read(&saresolve,saresolve.s,svname,"logger"))
		{
			if (str_diff(saresolve.s,svname))
			{
				r = s6_svc_ok(svok) ;
				if (r < 0) strerr_diefu2sys(111,"check ", svok) ;
				if (!r)	strerr_dief2x(110,svok," is not initialized") ;
			}
		}
		sv_signal.scan = svkeep.len ;
		if (!stralloc_catb(&svkeep, svok, svoklen + 1)) retstralloc(111,"main") ;
		sv_signal.scanlen = svoklen ;
				
		sv_signal.name = svkeep.len ;
		if (!stralloc_catb(&svkeep, svname, namelen + 1)) retstralloc(111,"main") ;
		sv_signal.namelen = namelen ;
		
		sv_signal.src = svkeep.len ;
		if (!stralloc_catb(&svkeep, src, srclen + 1)) retstralloc(111,"main") ;
		sv_signal.srclen = srclen ;
		
		
		/** notification-fd */
		memcpy(svok + svoklen,"/notification-fd",16) ;
		svok[svoklen + 16] = 0 ;
		e = errno ;
		errno = 0 ;
		if (access(svok, F_OK) < 0)
		{
			if (errno != ENOENT) strerr_diefu2sys(111, "access ", svok) ;
			if (SIGNAL == RUP) SIGNAL = UP ;
			if (SIGNAL == RDOWN) SIGNAL = DOWN ;
			
			VERBO2 strerr_warnw2x(svok, " is not present - ignoring request for readiness notification") ;
		}
		else
		{
			if (!read_uint(svok,&sv_signal.notify)) strerr_diefu2sys(111,"read: ",svok) ;
		}
		
		/** max-death-tally */
		if (!deathsv)
		{
			memcpy(svok + svoklen,"/max-death-tally", 16) ;
			svok[svoklen + 16] = 0 ;
			errno = 0 ;
			if (access(svok, F_OK) < 0)
			{
				if (errno != ENOENT) strerr_diefu2sys(111, "access ", svok) ;
			}
			else
			{
				if (!read_uint(svok,&sv_signal.ndeath)) strerr_diefu2sys(111,"read: ",svok) ;
			}
		}
		else sv_signal.ndeath = deathsv ;

		errno = 0 ;
	
		if (!tsv)
		{
			/** timeout-{up/down} */
			{
				char *tm = NULL ;
				unsigned int t ;
				if (SIGNAL == RUP || SIGNAL == UP)
				{
					tm="/timeout-up" ;
				}
				else
				{
					tm="/timeout-down" ;
				}
			
				memcpy(svok + svoklen,tm, strlen(tm) + 1) ;
				if (access(svok, F_OK) < 0)
				{
					if (errno != ENOENT) strerr_diefu2sys(111, "access ", svok) ;
				}
				if (errno == ENOENT)
				{
					tain_from_millisecs(&sv_signal.deadline, SV_DEADLINE) ;
				}
				else
				{
					if (!read_uint(svok,&t)) strerr_diefu2sys(111,"read: ",svok) ;
					tain_from_millisecs(&sv_signal.deadline, t) ;
				}
			}
		}
		else tain_from_millisecs(&sv_signal.deadline, tsv) ;
		errno = e ;
		
		if (!genalloc_append(svc_sig,&gakeep,&sv_signal)) strerr_diefu1sys(111,"append genalloc") ;
				
	}
	
	stralloc_free(&base) ;
	stralloc_free(&live) ;
	stralloc_free(&tree) ;
	stralloc_free(&scandir) ;
	
	if (!genalloc_len(svc_sig,&gakeep))
	{
		VERBO2 strerr_warni1x("nothing to do") ;
		return 0 ;
	}
	
	tain_now_g() ;
	tain_add_g(&ttmain, &ttmain) ;
	
	VERBO2 strerr_warni1x("initiate fifo: fifo") ;
	if (!ftrigr_startf(&fifo, &ttmain, &STAMP))
		strerr_diefu1sys(111,"initiate fifo") ;
	
	for (unsigned int i = 0 ; i < genalloc_len(svc_sig,&gakeep) ; i++)
	{
		int isup ;
		char *sig ;
		
		char *sv = svkeep.s + genalloc_s(svc_sig,&gakeep)[i].scan ;
		if (!s6_svstatus_read(sv,&status)) strerr_diefu2sys(111,"read status of: ",sv) ;
		isup = status.pid && !status.flagfinishing ;
			
		if (isup && (SIGNAL == RUP || SIGNAL == UP))
		{
			VERBO2 strerr_warni2x(sv,": already up") ;
			continue ;
		}
		
		if (isup && (SIGNAL == RDOWN || SIGNAL == DOWN))
		{
			if (SIGNAL == RDOWN)
				sig = "dwD" ;
			else sig = "dwd" ;
			
			VERBO2 strerr_warni3x("bring down ", sv," ...") ;
			r = send_svc(&genalloc_s(svc_sig,&gakeep)[i],sig,&fifo,&ttmain) ;
			if (r == 4)
			{
				if (!s6_svstatus_read(sv,&status)) strerr_diefu2sys(111,"read status of: ",sv) ;
				isup = status.pid && !status.flagfinishing ;
				if (!isup) r = 1 ;
				else r = 0 ;
			}
			if (r == 3)
			{
				VERBO2 strerr_warnw2x(sv," report permanent failure to bring down") ;
				goto errftrigr ;
			}
			if (r == 2) VERBO2 strerr_warni2x(sv," is down but not notified by the daemon itself") ; 
			if (!r)
			{
				VERBO2 strerr_warnwu2x("bring down: ",  sv) ;
				goto errftrigr ;
			}
			if (r < -1)
			{
				VERBO2 strerr_warnw1x("received SIGINT, aborting service transition") ;
				goto errftrigr ;
			}
			if (r == 1) VERBO2 strerr_warni2x(sv,": stopped successfully") ;
			continue ;
		}
		
		if (!isup && (SIGNAL == RDOWN || SIGNAL == DOWN))
		{
			VERBO2 strerr_warni2x(sv,": already down") ;
			continue ;
		}
		
		if (!isup && (SIGNAL == RUP || SIGNAL == UP))
		{
			if (SIGNAL == RUP)
				sig = "uwU" ;
			else sig = "uwu" ;
	
			VERBO2 strerr_warni3x("bring up ", sv," ...") ;
			r = send_svc(&genalloc_s(svc_sig,&gakeep)[i],sig,&fifo,&ttmain) ;
			if (r == 4)
			{
				if (!s6_svstatus_read(sv,&status)) strerr_diefu2sys(111,"read status of: ",sv) ;
				isup = status.pid && !status.flagfinishing ;
				if (isup) r = 1 ;
				else r = 0 ;
			}
			if (r == 3)
			{
				VERBO2 strerr_warnw2x(sv,"report permanent failure to bring up") ;
				goto errftrigr ;
			}
			if (r == 2) VERBO2 strerr_warni2x(sv," is up but not notified by the daemon itself") ; 
			if (!r)
			{
				VERBO2 strerr_warnwu2x("bring up: ",  sv) ;
				goto errftrigr ;
			}
			if (r < -1)
			{
				VERBO2 strerr_warnw1x("received SIGINT, aborting service transition") ;
				goto errftrigr ;
			}
			if (r == 1) VERBO2 strerr_warni2x(sv,": started successfully") ;
			
		}
	}
	
	ftrigr_end(&fifo) ;
	stralloc_free(&svkeep) ;
	genalloc_free(svc_sig,&gakeep) ;
	free(treename) ;
	
	return 0 ;		
	
	errftrigr:
		ftrigr_end(&fifo) ;
		stralloc_free(&svkeep) ;
		genalloc_free(svc_sig,&gakeep) ;
		free(treename) ;
		return 111 ;
}
	

