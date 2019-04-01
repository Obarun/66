/* 
 * ssexec_svctl.c
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
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>//access
//#include <stdio.h>

#include <oblibs/obgetopt.h>
#include <oblibs/error2.h>

#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/bytestr.h>
#include <skalibs/selfpipe.h>
#include <skalibs/iopause.h>
#include <skalibs/tai.h>
#include <skalibs/sig.h>//sig_ignore

#include <s6/s6-supervise.h>//s6_svstatus_t
#include <s6/ftrigr.h>

#include <66/utils.h>
#include <66/constants.h>
#include <66/svc.h>
#include <66/ssexec.h>
#include <66/resolve.h>
#include <66/state.h>

unsigned int SV_DEADLINE = 3000 ;
unsigned int DEATHSV = 10 ;

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

/** @Return 0 on fail
 * @Return 1 on success */
int handle_signal_svc(ss_resolve_sig_t *sv_signal)
{
	s6_svstatus_t status = S6_SVSTATUS_ZERO ;
	char *sv = sv_signal->res.sa.s + sv_signal->res.runat ;
	if (!s6_svstatus_read(sv,&status))
	{ 
		VERBO3 strerr_warnwu2sys("read status of: ",sv) ;
		return 0 ;
	}
	sv_signal->pid = status.pid ;
	
	if (WIFSIGNALED(status.wstat) && !WEXITSTATUS(status.wstat) && (WTERMSIG(status.wstat) == 15 )) return 1 ;
	if (!WIFSIGNALED(status.wstat) && !WEXITSTATUS(status.wstat)) return 1 ;
	else return 0 ;
}

static unsigned char const actions[9][9] = 
{
 //signal receive:
 //  c->u		U		r/u		R/U		d		D		x		O		s						
																				//signal wanted
    { GOTIT,	DONE,	GOTIT,	DONE,	DEAD,	DEAD,	PERM,	PERM,	UKNOW },// SIGUP 
    { WAIT,		GOTIT,	WAIT,	GOTIT,	DEAD,	DEAD,	PERM,	PERM,	UKNOW },// SIGRUP
    { GOTIT,	DONE,	GOTIT,	DONE,	WAIT,	WAIT,	PERM,	PERM,	UKNOW },// SIGR
    { WAIT,		GOTIT,	WAIT,	GOTIT,	WAIT,	WAIT,	PERM,	PERM,	UKNOW },// SIGRR
    { DEAD,		DEAD,	DEAD,	DEAD,	GOTIT,	DONE,	PERM,	PERM,	UKNOW },// SIGDOWN
    { DEAD,		DEAD,	DEAD,	DEAD,	WAIT,	GOTIT,	PERM,	PERM,	UKNOW },// SIGRDOWN
    { DEAD,		DEAD,	DEAD,	DEAD,	WAIT,	WAIT,	DONE,	WAIT, 	DONE },// SIGX
    { WAIT,		WAIT,	WAIT,	WAIT,	WAIT,	WAIT,	PERM,	GOTIT, 	UKNOW },// SIGO
    { UKNOW,	UKNOW,	UKNOW,	UKNOW,	UKNOW,	UKNOW,	UKNOW,	UKNOW, 	DONE },	// SIGSUP
    
} ;

//	convert signal receive into enum number
static const uint8_t chtenum[128] = 
{	
	0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , //8
	0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , //16
	0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , //24
	0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , //32
	0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , //40
	0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , //48
	0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , //56
	0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , //64
	0 ,  0 ,  0 ,  0 ,  5 ,  0 ,  0 ,  0 , //72
	0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  7 , //80
	0 ,  0 ,  0 ,  0 ,  0 ,  1,   0 ,  0 , //88
	6 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , //96
	0 ,  0 ,  0 ,  0 ,  4 ,  0 ,  0 ,  0 , //104
	0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , //112
	0 ,  0 ,  0 ,  8 ,  0 ,  0 ,  0 ,  0 , //120
	6 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0   //128
} ;
/** we use three file more than s6-svc program :
 * 	 
 *	 timeout-up and timeout-down will define the deadline for the iopause
 * 		each service contain his deadline into the sv_signal.deadline key.
 * 		If thoses files doesn't exit 3000 millisec is set by default.
 * 		This deadline value can be set on commandline by the -T options.
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
 * In the send_svc function a reached timeout is not an absolute error
 * because we can have a notification-file in the servicedirs of the
 * service with a deamon which doesn't support readiness notification. 
 * In this case, we send U and only receive u as event, the daemon is
 * running but not notified by the daemon itself.
 * We accept this case as an "half" success. We exit with success but
 * the user is warned about the state. */
 	
/**@Return 0 on success
 * @Return 1 on if signal is not complete (e.g. want U receive only u)
 * @Return 2 on fail
 * @Return 3 for PERMANENT failure */
int handle_case(stralloc *sa, ss_resolve_sig_t *sv_signal)
{
	int p, h, err ;
	unsigned int state, i = 0 ;
	state = sv_signal->sig ;
	err = 2 ;
	
	for (;i < sa->len ; i++)
	{
		p = chtenum[(unsigned char)sa->s[i]] ;
		
		unsigned char action = actions[state][p] ;
		
		
		switch (action)
		{
			case GOTIT:
						h = handle_signal_svc(sv_signal) ;
						if (!h)
						{
							err = 1 ;
							break ;
						}
						err = 0 ;
						return err ; 
			case WAIT:
						err = 1 ;
						break ;
			case DEAD:
						err = 2 ;
						return err ; 
			case DONE:
						err = 0 ;
						return err ; 
			case PERM:
						err = 3 ;
						return err ; 
			default:
						VERBO3 strerr_warnw1x("invalid state, make a bug report");
						err = 2 ;
						return err ; 
		}
	}
	return err ;
}

static int handle_signal_pipe(ss_resolve_sig_t *svc)
{
	char *sv = svc->res.sa.s + svc->res.runat ;
	for (;;)
	{
		switch (selfpipe_read())
		{
			case -1 : strerr_warnwu1sys("selfpipe_read") ; return 0 ;
			case 0 : return 1 ;
			case SIGTERM: 		
			case SIGINT: 		
					// TODO, little ugly for me
					VERBO2 strerr_warnw1x("received SIGINT, aborting service transition") ;
					if (svc->sig <= SIGRR)
						s6_svc_writectl(sv, S6_SUPERVISE_CTLDIR, "d", 1) ;
					return 0 ;
			default : strerr_warn1x("unexpected data in selfpipe") ; return 0 ;
		}
	}
}

static void announce(ss_resolve_sig_t *sv_signal)
{

	int r = sv_signal->state ;
	char *sv = sv_signal->res.sa.s + sv_signal->res.runat ;
	if (r == 3) { VERBO2 strerr_warnw3x(sv," report permanent failure -- unable to ",(sv_signal->sig > 1) ? "stop" : "start") ; }
	else if (r == 2) { VERBO2 strerr_warnwu2x((sv_signal->sig > 3) ? "stop " : "start ", sv) ; }
	else if (r == 1) { VERBO2 strerr_warni4x(sv," is ",(sv_signal->sig > 3) ? "down" : "up"," but not notified by the daemon itself") ; }
	else if (!r) { VERBO2 strerr_warni4x(sv,": ",(sv_signal->sig > 3) ? "stopped" : "started"," successfully") ; }
}

int svc_listen(genalloc *gasv, ftrigr_t *fifo,int spfd,ss_resolve_sig_t *svc)
{
	int r ;

	stralloc sa = STRALLOC_ZERO ;
	
	iopause_fd x[2] = { { .fd = ftrigr_fd(fifo), .events = IOPAUSE_READ } , { .fd = spfd, .events = IOPAUSE_READ, .revents = 0 } } ;
		
	tain_t t ;
	tain_now_g() ;
	tain_add_g(&t,&svc->deadline) ;
	
	VERBO3 strerr_warnt2x("start iopause for: ",svc->res.sa.s + svc->res.runat) ;
	
	for(;;)
	{
		r = ftrigr_checksa(fifo,svc->ids, &sa) ;
		if (r < 0) { VERBO3 strerr_warnwu1sys("ftrigr_check") ; return 0 ; } 
		if (r)
		{
			svc->state = handle_case(&sa,svc) ;
			if (!svc->state){ announce(svc) ; goto end ; }
			else if (svc->state >= 2){ announce(svc) ; goto end ; }
		}
		if (!(svc->ndeath))
		{
			VERBO2 strerr_warnw2x("number of try exceeded for: ",svc->res.sa.s + svc->res.runat) ;
			announce(svc) ;
			goto end ;
		}
		
		r = iopause_g(x, 2, &t) ;
		if (r < 0){ VERBO3 strerr_warnwu1sys("iopause") ; return 0 ; }
		/** timeout is not a error , the iopause process did right */
		else if (!r) { VERBO3 strerr_warnt2x("timed out reached for: ",svc->res.sa.s + svc->res.runat) ; announce(svc) ; break ; } 
		
		if (x[1].revents & IOPAUSE_READ) return handle_signal_pipe(svc) ? 1 : 0 ;
				
		if (x[0].revents & IOPAUSE_READ)  
		{	
			if (ftrigr_update(fifo) < 0){ VERBO3 strerr_warnwu1sys("ftrigr_update") ; return 0 ; }
			svc->ndeath--;
		}
	}
	end:
	stralloc_free(&sa) ;
	return svc->state ;
}

int ssexec_svctl(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
	
	// be sure that the global var are set correctly
	SV_DEADLINE = 3000 ;
	DEATHSV = 10 ;
	tain_t ttmain ;
	
	int e, isup, ret, r ;
	unsigned int death, tsv, reverse ;
	int SIGNAL = -1 ;
	
	genalloc gakeep = GENALLOC_ZERO ; //type ss_resolve_sig
	stralloc sares = STRALLOC_ZERO ;
	ss_resolve_graph_t graph = RESOLVE_GRAPH_ZERO ;
	ss_resolve_t res = RESOLVE_ZERO ;
	ss_state_t sta = STATE_ZERO ;
	
	char *sig = 0 ;
	
	ftrigr_t fifo = FTRIGR_ZERO ;
	
	s6_svstatus_t status = S6_SVSTATUS_ZERO ;
	
	tsv = death = ret = reverse = 0 ;
	
	//PROG = "66-svctl" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, "n:udUDXrRK", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'n' :	if (!uint0_scan(l.arg, &death)) exitusage(usage_svctl) ; break ;
				case 'u' :	if (SIGNAL > 0) exitusage(usage_svctl) ; SIGNAL = SIGUP ; sig ="u" ; break ;
				case 'U' :	if (SIGNAL > 0) exitusage(usage_svctl) ; SIGNAL = SIGRUP ; sig = "uwU" ; break ;
				case 'r' :	if (SIGNAL > 0) ; SIGNAL = SIGR ; sig = "r" ; break ;
				case 'R' :	if (SIGNAL > 0) ; SIGNAL = SIGRR ; sig = "rwR" ; break ;
				case 'd' : 	if (SIGNAL > 0) ; SIGNAL = SIGDOWN ; sig = "d" ; break ;
				case 'D' :	if (SIGNAL > 0) ; SIGNAL = SIGRDOWN ; sig = "dwD" ; break ;
				case 'X' :	if (SIGNAL > 0) ; SIGNAL = SIGX ; sig = "xd" ; break ;
				case 'K' :	if (SIGNAL > 0) ; SIGNAL = SIGRDOWN ; sig = "kd" ; break ;
				
				default : exitusage(usage_svctl) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc < 1 || (SIGNAL < 0)) exitusage(usage_svctl) ;
	
	if (info->timeout) tsv = info->timeout ;
			
	if ((scandir_ok(info->scandir.s)) !=1 ) strerr_dief3sys(111,"scandir: ", info->scandir.s," is not running") ;
		
	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC)) strerr_diefu1sys(111,"set revolve pointer to source") ;
	
	if (SIGNAL > SIGRUP) reverse = 1 ;
	
	for(;*argv;argv++)
	{
		char const *name = *argv ;
		if (!ss_resolve_check(sares.s,name)) strerr_dief2sys(111,"unknown service: ",name) ;
		if (!ss_resolve_read(&res,sares.s,name)) strerr_diefu2sys(111,"read resolve file of: ",name) ;
		if (res.type >= BUNDLE) strerr_dief3x(111,name," has type ",get_keybyid(res.type)) ;
		if (!ss_resolve_graph_build(&graph,&res,sares.s,reverse)) strerr_diefu1sys(111,"build services graph") ;
	}
	
	if (SIGNAL == SIGR || SIGNAL == SIGRR) reverse = 0 ;
	r = ss_resolve_graph_publish(&graph,reverse) ;
	if (r < 0) strerr_dief1x(111,"cyclic dependencies detected") ;
	if (!r) strerr_diefu1sys(111,"publish service graph") ;
	
	for(unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,&graph.sorted) ; i++)
	{
		ss_resolve_sig_t sv_signal = RESOLVE_SIG_ZERO ;
		sv_signal.res = genalloc_s(ss_resolve_t,&graph.sorted)[i] ;
		char *string = sv_signal.res.sa.s ;
		char *svok = string + sv_signal.res.runat ;
		char *state = string + sv_signal.res.state ;
		size_t svoklen = strlen(svok) ;
		char file[svoklen + 16 + 1] ;
		memcpy(file,svok,svoklen) ;
		if (!ss_state_check(state,string + sv_signal.res.name)) strerr_dief2x(111,"unitialized service: ",string + sv_signal.res.name) ;
		if (!ss_state_read(&sta,state,string + sv_signal.res.name)) strerr_diefu2sys(111,"read state of: ",string + sv_signal.res.name) ;
		if (sta.init) strerr_dief2x(111,"unitialized service: ",string + sv_signal.res.name) ;
		if (!s6_svstatus_read(svok,&status)) strerr_diefu2sys(111,"read status of: ",svok) ;
		isup = status.pid && !status.flagfinishing ;
						
		if (isup && (SIGNAL <= SIGRUP))
		{
			VERBO1 strerr_warni2x("Already up: ",string + sv_signal.res.name) ;
			continue ;
		}
		else if (!isup && (SIGNAL >= SIGDOWN))
		{
			VERBO1 strerr_warni2x("Already down: ",string + sv_signal.res.name) ;
			continue ;
		}
		/*else if ((SIGNAL == SIGR) || (SIGNAL == SIGRR)) 
		{
			SIGNAL = SIGRUP ;
		}*/
		sv_signal.sigtosend = sig ;
		sv_signal.sig = SIGNAL ;
		
		/** notification-fd */
		
		memcpy(file + svoklen,"/notification-fd",16) ;
		file[svoklen + 16] = 0 ;
		e = errno ;
		errno = 0 ;
		
		if (access(file, F_OK) < 0)
		{
			if (errno != ENOENT) strerr_diefu2sys(111, "access ", file) ;
			if (SIGNAL == SIGRUP)
			{
				sv_signal.sig = SIGUP ;
				sv_signal.sigtosend = "uwu" ;
				VERBO1 strerr_warnw2x(file, " is not present - ignoring request for readiness notification") ;
			}
			if (SIGNAL == SIGRR)
			{
				sv_signal.sig = SIGR ;
				sv_signal.sigtosend = "uwr" ;
				VERBO1 strerr_warnw2x(file, " is not present - ignoring request for readiness notification") ;
			}
			if (SIGNAL == SIGRDOWN)
			{
				sv_signal.sig = SIGDOWN ;
				sv_signal.sigtosend = "dwd" ;
				VERBO1 strerr_warnw2x(file, " is not present - ignoring request for readiness notification") ;
			}		
		}
		else if (!read_uint(file,&sv_signal.notify)) strerr_diefu2sys(111,"read: ",file) ;
		
		/** max-death-tally */
		if (!death)
		{
			memcpy(file + svoklen,"/max-death-tally", 16) ;
			file[svoklen + 16] = 0 ;
			errno = 0 ;
			if (access(file, F_OK) < 0)
			{
				if (errno != ENOENT) strerr_diefu2sys(111, "access ", file) ;
			}
			if (errno == ENOENT)
			{
				sv_signal.ndeath = DEATHSV ;
			}
			else
			{
				if (!read_uint(file,&sv_signal.ndeath)) strerr_diefu2sys(111,"read: ",file) ;
			}
		}
		else sv_signal.ndeath = death ;
		
		tain_t tcheck ;
		tain_from_millisecs(&tcheck,tsv) ;
		int check = tain_to_millisecs(&tcheck) ;
		if (check > 0) tain_from_millisecs(&sv_signal.deadline, tsv) ;
		else
		{
			/** timeout-{up/down} */
			{
				char *tm = NULL ;
				unsigned int t ;
				if (SIGNAL <= SIGRR)
				{
					tm="/timeout-up" ;
				}
				else
				{
					tm="/timeout-down" ;
				}
				errno = 0 ;
				size_t tmlen = strlen(tm) ;
				memcpy(file + svoklen,tm, tmlen) ;
				file[svoklen + tmlen] = 0 ;
				if (access(file, F_OK) < 0)
				{
					if (errno != ENOENT) strerr_diefu2sys(111, "access ", file) ;
				}
				if (errno == ENOENT)
				{
					tain_from_millisecs(&sv_signal.deadline, SV_DEADLINE) ;
				}
				else
				{
					if (!read_uint(file,&t)) strerr_diefu2sys(111,"read: ",file) ;
					tain_from_millisecs(&sv_signal.deadline, t) ;
				}
			}
		}
	
		errno = e ;
	
		if (!genalloc_append(ss_resolve_sig_t,&gakeep,&sv_signal)) strerr_diefu2sys(111,"append services selection with: ",string + sv_signal.res.name) ;
	}
	/** nothing to do */
	if (!genalloc_len(ss_resolve_sig_t,&gakeep)) goto finish ;
	
	int spfd = selfpipe_init() ;
	if (spfd < 0) strerr_diefu1sys(111, "selfpipe_trap") ;
	if (selfpipe_trap(SIGINT) < 0) strerr_diefu1sys(111, "selfpipe_trap") ;
	if (selfpipe_trap(SIGTERM) < 0) strerr_diefu1sys(111, "selfpipe_trap") ;
	if (sig_ignore(SIGPIPE) < 0) strerr_diefu1sys(111,"ignore SIGPIPE") ;
	
	if (!svc_init_pipe(&fifo,&gakeep)) strerr_diefu1x(111,"init pipe") ;
	
	for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_sig_t,&gakeep) ; i++)
	{ 
		int nret = 0 ;
		ss_resolve_sig_t *sv = &genalloc_s(ss_resolve_sig_t,&gakeep)[i] ;
		char const *name = sv->res.sa.s + sv->res.name ;
		char const *state = sv->res.sa.s + sv->res.state ;
		nret = svc_listen(&gakeep,&fifo,spfd,sv) ;
		if (nret > 1) ret = 111 ;
		if (sv->sig <= 3)
		{
			if (nret <= 1)
			{
				ss_state_setflag(&sta,SS_FLAGS_PID,sv->pid) ;
				ss_state_setflag(&sta,SS_FLAGS_STATE,SS_FLAGS_TRUE) ;
			}
			else 
			{
				ss_state_setflag(&sta,SS_FLAGS_PID,SS_FLAGS_FALSE) ;
				ss_state_setflag(&sta,SS_FLAGS_STATE,SS_FLAGS_FALSE) ;
			}
		}
		else
		{
			if (nret <=1)
			{
				ss_state_setflag(&sta,SS_FLAGS_PID,SS_FLAGS_FALSE) ;
				ss_state_setflag(&sta,SS_FLAGS_STATE,SS_FLAGS_FALSE) ;
			}
			else
			{
				ss_state_setflag(&sta,SS_FLAGS_PID,sv->pid) ;
				ss_state_setflag(&sta,SS_FLAGS_STATE,SS_FLAGS_TRUE) ;
			}
		}
		ss_state_setflag(&sta,SS_FLAGS_RELOAD,SS_FLAGS_FALSE) ;
		ss_state_setflag(&sta,SS_FLAGS_INIT,SS_FLAGS_FALSE) ;
		ss_state_setflag(&sta,SS_FLAGS_UNSUPERVISE,SS_FLAGS_FALSE) ;
		VERBO2 strerr_warni2x("Write state file of: ",name) ;
		if (!ss_state_write(&sta,state,name))
		{
			VERBO1 strerr_warnwu2sys("write state file of: ",name) ;
			ret = 111 ;
		}
		if (!nret) VERBO1 strerr_warni3x((sv->sig > 3) ? "Stopped" : "Started"," successfully: ",name) ; 
	}
	
	tain_now_g() ;
	tain_addsec(&ttmain,&STAMP,2) ;
	
	for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_sig_t,&gakeep) ; i++)
	{ 
		ss_resolve_sig_t *sv = &genalloc_s(ss_resolve_sig_t,&gakeep)[i] ;
		if (!ftrigr_unsubscribe_g(&fifo, sv->ids, &ttmain))
		{ 
			VERBO3 strerr_warnwu2sys("unsubscribe to fifo of: ",sv->res.sa.s + sv->res.name) ;
			ret = 111 ;
		}
	}
	
	finish:
	ftrigr_end(&fifo) ;
	selfpipe_finish() ;
	stralloc_free(&sares) ;
	ss_resolve_graph_free(&graph) ;
	genalloc_free(ss_resolve_sig_t,&gakeep) ;
	ss_resolve_free(&res) ;
			
	return (ret > 1) ? 111 : 0 ;		
}
