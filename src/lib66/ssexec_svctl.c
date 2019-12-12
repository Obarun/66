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
//#include <stdlib.h>

#include <oblibs/obgetopt.h>
#include <oblibs/log.h>
#include <oblibs/string.h>

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
unsigned int DEATHSV = 5 ;
ftrigr_t fifo = FTRIGR_ZERO ;

typedef struct pidindex_s pidindex_t ;
struct pidindex_s
{
	ss_resolve_sig_t_ref sv ;
	pid_t pid ;
} ;

static pidindex_t *pidindex ;
static unsigned int npids = 0 ;

static int read_file (char const *file, char *buf, size_t n)
{
	ssize_t r = openreadnclose_nb(file, buf, n) ;
	if (r < 0)
	{
		if (errno != ENOENT) 
			log_warnusys_return(LOG_EXIT_ZERO,"open: ", file) ;
	}
	buf[byte_chr(buf, r, '\n')] = 0 ;
	return 1 ;
}

static int read_uint (char const *file, unsigned int *fd)
{
	char buf[UINT_FMT + 1] ;
	if (!read_file(file, buf, UINT_FMT)) return 0 ;
	if (!uint0_scan(buf, fd))
		log_warn_return(LOG_EXIT_ZERO,"invalid: ", file) ;
	
	return 1 ;
}

/** @Return 0 on fail
 * @Return 1 on success */
int handle_signal_svc(ss_resolve_sig_t *sv_signal)
{
	s6_svstatus_t status = S6_SVSTATUS_ZERO ;
	char *sv = sv_signal->res.sa.s + sv_signal->res.runat ;
	if (!s6_svstatus_read(sv,&status))
		log_warnusys_return(LOG_EXIT_ZERO,"read status of: ",sv) ;

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
 * 		If this file doesn't exist a number of 5 death (death -> number 
 * 		of wrong signal received) is set by default.
 * 		This ndeath value can be set on commandline by the -n options.
 *
 * In all case the loop is broken to avoids infinite iopause 
 * 
 * The notication-fd file is used as the original one. If the file 
 * exist and the SIGNAL is u , we change SIGNAL to U.
 */
 	
/**@Return 0 on success
 * @Return 1 on if signal is not complete (e.g. want U receive only u)
 * @Return 2 on fail
 * @Return 3 for PERMANENT failure */
int handle_case(stralloc *sa, ss_resolve_sig_t *svc)
{
	int p, h, err ;
	unsigned int state, i = 0 ;
	state = svc->sig ;
	err = 2 ;

	for (;i < sa->len ; i++)
	{
		p = chtenum[(unsigned char)sa->s[i]] ;
		unsigned char action = actions[state][p] ;
		switch (action)
		{
			case GOTIT:
						h = handle_signal_svc(svc) ;
						if (!h)
						{
							err = 0 ;
							break ;
						}
						return 1 ; 
			case WAIT:	err = 0 ;
						break ;
			case DEAD:	return 2 ; 
			case DONE:	return 1 ; 
			case PERM:	return 3 ; 
			default:	log_warn_return(2,"invalid state, make a bug report") ; 
		}
	}
	return err ;
}

static void write_state(ss_resolve_sig_t *svc)
{
	ss_state_t sta = STATE_ZERO ;
	char const *state = svc->res.sa.s + svc->res.state ;
	char const *sv = svc->res.sa.s + svc->res.name ;
	if (svc->sig <= 3)
	{
		if (svc->state <= 1)
		{
			ss_state_setflag(&sta,SS_FLAGS_PID,svc->pid) ;
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
		if (svc->state <=1)
		{
			ss_state_setflag(&sta,SS_FLAGS_PID,SS_FLAGS_FALSE) ;
			ss_state_setflag(&sta,SS_FLAGS_STATE,SS_FLAGS_FALSE) ;
		}
		else
		{
			ss_state_setflag(&sta,SS_FLAGS_PID,svc->pid) ;
			ss_state_setflag(&sta,SS_FLAGS_STATE,SS_FLAGS_TRUE) ;
		}
	}
	ss_state_setflag(&sta,SS_FLAGS_RELOAD,SS_FLAGS_FALSE) ;
	ss_state_setflag(&sta,SS_FLAGS_INIT,SS_FLAGS_FALSE) ;
//	ss_state_setflag(&sta,SS_FLAGS_UNSUPERVISE,SS_FLAGS_FALSE) ;
	log_trace("Write state file of: ",sv) ;
	if (!ss_state_write(&sta,state,sv))
		log_warnusys("write state file of: ",sv) ;
}

static int announce(ss_resolve_sig_t *svc)
{
	int r = svc->state ;
	char *sv = svc->res.sa.s + svc->res.name ;
	/** special case time out reached or number execeeded,
	 *  last check with s6-svstat framboise*/
	if (r == 0 || r == 4 || r == 5)
	{
		int e = handle_signal_svc(svc) ;
		if (!e){
			if (!r) r = 2 ;
			else r = r ;
		}
		else r = r == 0 ? 0 : 1 ;
		svc->state = r ;
	}
	switch(r)
	{
		case 0: log_info(sv," is ",(svc->sig > 3) ? "down" : "up"," but not notified by the daemon itself") ; 
				break ;
		case 1: log_info(sv,": ",(svc->sig > 3) ? "stopped" : (svc->sig == 2 || svc->sig == 3) ? "reloaded" : "started"," successfully") ;
				break ;
		case 2: log_info((svc->sig > 3) ? "stop " : (svc->sig == 2 || svc->sig == 3) ? "reload" : "start ", sv) ; 
				break ;
		case 3: log_info(sv," report permanent failure -- unable to ",(svc->sig > 1) ? "stop" : (svc->sig == 2 || svc->sig == 3) ? "reload" : "start") ;
				break ;
		case 4: log_info((svc->sig > 3) ? "stop: " : (svc->sig == 2 || svc->sig == 3) ? "reload" : "start: ",sv, ": number of try exceeded") ;
				break ;
		case 5: log_info((svc->sig > 3) ? "stop: " : (svc->sig == 2 || svc->sig == 3) ? "reload" : "start: ",sv, ": time out reached") ;
				break ;
		case-1: 
		default:log_warn("unexpected data in state file of: ",sv," -- please make a bug report") ;
				break ;
	}		
	return r ;
}

static inline void kill_all (void)
{
	unsigned int j = npids ;
	while (j--) kill(pidindex[j].pid, SIGTERM) ;
}

static int handle_signal_pipe(genalloc *gakeep)
{
	int ok = 1 ;
	for (;;)
	{
		switch (selfpipe_read())
		{
			case -1 : log_warnusys_return(LOG_EXIT_ZERO,"selfpipe_read") ;
			case 0 : goto end ;
			case SIGCHLD:
				for (;;)
				{
					unsigned int j = 0 ;
					int wstat ;
					pid_t r = wait_nohang(&wstat) ;
					if (r < 0)
						if (errno = ECHILD) break ;
						else log_dieusys(LOG_EXIT_SYS,"wait for children") ;
					else if (!r) break ;
					for (; j < npids ; j++) if (pidindex[j].pid == r) break ;
					if (j < npids)
					{ 
						ss_resolve_sig_t_ref sv = pidindex[j].sv ;
						pidindex[j] = pidindex[--npids] ;
						if (!WIFSIGNALED(wstat) && !WEXITSTATUS(wstat))
						{
							if (announce(sv) > 1) ok = 0 ; 
							write_state(sv) ;
						}
						else
						{
							ok = 0 ; announce(sv) ; write_state(sv) ;
						}
					}
				}
				break ;
			case SIGTERM: 		
			case SIGINT: 		
					log_warn("received SIGINT, aborting service transition") ;
					kill_all() ;
					break ;
			default : log_warn("unexpected data in selfpipe") ; 
		}
	}
	end:
	return ok ;

}
static int compute_timeout(tain_t *start,tain_t *tsv)
{
	tain_t now,tpass ;
	tain_now_g() ;
	tain_copynow(&now) ;
	tain_sub(&tpass,&now,start) ;
	if (tain_less(tsv,&tpass)) return 0 ;
	return 1 ;	
}

static void svc_listen_less(int state_val, int *state,unsigned int *did,unsigned int *loop,unsigned int pos)
{
	(*state) = state_val ;
	did[pos] = 1 ;
	(*loop)--;
}
static void svc_listen(unsigned int nsv,tain_t *deadline)
{
	int r ;
	tain_t start ;
	stralloc sa = STRALLOC_ZERO ;
	iopause_fd x = { .fd = ftrigr_fd(&fifo), .events = IOPAUSE_READ } ;
	ss_resolve_sig_t_ref svc ;
	int *state = 0 ;
	unsigned int *ndeath = 0, i, j ;
	unsigned int did[nsv] ;
	i = j = nsv ;
	
	tain_now_g() ;
	tain_copynow(&start) ;
	
	while(i--)
		did[i] = 0 ;

	while(j)
	{
		r = iopause_g(&x, 1, deadline) ;
		if (r < 0) log_diesys(LOG_EXIT_SYS,"listen iopause") ;
		else if (!r) log_die(LOG_EXIT_SYS,"listen time out") ;
		if (x.revents & IOPAUSE_READ)  
		{	
			i = 0 ;
			if (ftrigr_update(&fifo) < 0) log_dieusys(LOG_EXIT_SYS,"update fifo") ;
			for (;i < nsv;i++)
			{
				if (did[i]) continue ;
				svc = pidindex[i].sv ;
				state = &svc->state ;
				ndeath = &svc->ndeath ;
				if (!compute_timeout(&start,&pidindex[i].sv->deadline))
				{ svc_listen_less(5,state,did,&j,i) ; continue ; }
				sa.len = 0 ;
				r = ftrigr_checksa(&fifo,svc->ids, &sa) ;
				if (r < 0) log_dieusys(LOG_EXIT_SYS,"check fifo") ; 
				else if (r)
				{
					(*ndeath)-- ;
					(*state) = handle_case(&sa,svc) ;
					if (!(*ndeath)) svc_listen_less(4,state,did,&j,i) ;
					if ((*state) >= 1) svc_listen_less(*state,state,did,&j,i) ;
				}
				
			}			
		}
	}
	stralloc_free(&sa) ;
}
/* @return 111 unable to control
 * @return 100 "something is wrong with the S6_SUPERVISE_CTLDIR directory. errno reported
 * @return 99 supervisor not listening */
static int svc_writectl(ss_resolve_sig_t *svc)
{
	int r ;
	char *sv = svc->res.sa.s + svc->res.runat ;
	size_t siglen = strlen(svc->sigtosend) ;
	log_trace("send signal: ",svc->sigtosend," to: ", sv,"/",S6_SUPERVISE_CTLDIR) ;
	r = s6_svc_writectl(sv, S6_SUPERVISE_CTLDIR, svc->sigtosend, siglen) ;
	if (r == -1) return 111 ;
	else if (r == -2) return 100 ;
	else if (!r) return 99 ;
	return 0 ;
}

static void svc_async(unsigned int i,unsigned int nsv)
{
	int r ;
	pid_t pid ;
	pid = fork() ;
	if (pid < 0) return ;
	if (!pid)
	{
		r = svc_writectl(pidindex[i].sv) ;
		_exit(r) ;
	}
	
	pidindex[npids].pid = pid ;
	pidindex[npids++].sv = pidindex[i].sv ;
	return ;
}

int doit (int spfd, genalloc *gakeep, tain_t *deadline)
{
	iopause_fd x = { .fd = spfd, .events = IOPAUSE_READ } ;
	unsigned int nsv = genalloc_len(ss_resolve_sig_t,gakeep) ;
	unsigned int i = 0 ;
	int exitcode = 1 ;
	pidindex_t pidindextable[nsv] ;
	pidindex = pidindextable ;
	/** keep the good order service declaration 
	 * of the genalloc*/
	while(i<nsv){
		pidindex[i].sv = &genalloc_s(ss_resolve_sig_t,gakeep)[i] ;
		pidindex[i].pid = 0 ;
		i++ ;
	}
	i = 0 ;
	while(i < nsv)
	{ 
		svc_async(i,nsv) ;
		i++ ;
	}
	svc_listen(nsv,deadline) ;
	while (npids)
	{
		int r = iopause_g(&x,1,deadline) ;
		if (r < 0) log_dieusys(LOG_EXIT_SYS,"iopause") ;
		if (!r) log_diesys(LOG_EXIT_SYS,"time out") ;
		if (!handle_signal_pipe(gakeep)) exitcode = 0 ;
	}
	return exitcode ;
}

int ssexec_svctl(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
	
	// be sure that the global var are set correctly
	SV_DEADLINE = 3000 ;
	DEATHSV = 5 ;
	
	int e, isup, r, ret = 1 ;
	unsigned int death, tsv, reverse, tsv_g ;
	int SIGNAL = -1 ;
	tain_t ttmain ;
	
	genalloc gakeep = GENALLOC_ZERO ; //type ss_resolve_sig
	stralloc sares = STRALLOC_ZERO ;
	ss_resolve_graph_t graph = RESOLVE_GRAPH_ZERO ;
	ss_resolve_t res = RESOLVE_ZERO ;
	ss_state_t sta = STATE_ZERO ;
	
	char *sig = 0 ;
	
	s6_svstatus_t status = S6_SVSTATUS_ZERO ;
	
	tsv = death = reverse = 0 ;
	tsv_g = SV_DEADLINE ;
	
	//PROG = "66-svctl" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, "n:urdXK", &l) ;
			if (opt == -1) break ;
			if (opt == -2) log_die(LOG_EXIT_USER,"options must be set first") ;
			switch (opt)
			{
				case 'n' :	if (!uint0_scan(l.arg, &death)) log_usage(usage_svctl) ; break ;
				case 'u' :	if (SIGNAL > 0) log_usage(usage_svctl) ; SIGNAL = SIGUP ; sig ="u" ; break ;
				case 'r' :	if (SIGNAL > 0) log_usage(usage_svctl) ; SIGNAL = SIGR ; sig = "r" ; break ;
				case 'd' : 	if (SIGNAL > 0) log_usage(usage_svctl) ; SIGNAL = SIGDOWN ; sig = "d" ; break ;
				case 'X' :	if (SIGNAL > 0) log_usage(usage_svctl) ; SIGNAL = SIGX ; sig = "xd" ; break ;
				case 'K' :	if (SIGNAL > 0) log_usage(usage_svctl) ; SIGNAL = SIGRDOWN ; sig = "kd" ; break ;
				
				default : log_usage(usage_svctl) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc < 1 || (SIGNAL < 0)) log_usage(usage_svctl) ;
	if (info->timeout) tsv = info->timeout ;
	if ((scandir_ok(info->scandir.s)) !=1 ) log_diesys(LOG_EXIT_SYS,"scandir: ", info->scandir.s," is not running") ;
	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC)) log_dieusys(LOG_EXIT_SYS,"set revolve pointer to source") ;
	if (SIGNAL > SIGR) reverse = 1 ;
	for(;*argv;argv++)
	{
		char const *name = *argv ;
		int logname = 0 ;
		logname = get_rstrlen_until(name,SS_LOG_SUFFIX) ;
		if (!ss_resolve_check(sares.s,name)) log_diesys(LOG_EXIT_SYS,"unknown service: ",name) ;
		if (!ss_resolve_read(&res,sares.s,name)) log_dieusys(LOG_EXIT_SYS,"read resolve file of: ",name) ;
		if (res.type >= BUNDLE) log_die(LOG_EXIT_SYS,name," has type ",get_keybyid(res.type)) ;
		if (SIGNAL == SIGR && logname < 0) reverse = 1 ;
		if (!ss_resolve_graph_build(&graph,&res,sares.s,reverse)) log_dieusys(LOG_EXIT_SYS,"build services graph") ;
	}
	
	r = ss_resolve_graph_publish(&graph,reverse) ;
	if (r < 0) log_die(LOG_EXIT_SYS,"cyclic dependencies detected") ;
	if (!r) log_dieusys(LOG_EXIT_SYS,"publish service graph") ;

	for(unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,&graph.sorted) ; i++)
	{
		ss_resolve_sig_t sv_signal = RESOLVE_SIG_ZERO ;
		sv_signal.res = genalloc_s(ss_resolve_t,&graph.sorted)[i] ;
		char *string = sv_signal.res.sa.s ;
		char *svok = string + sv_signal.res.runat ;
		char *state = string + sv_signal.res.state ;
		size_t svoklen = strlen(svok) ;
		char file[svoklen + SS_NOTIFICATION_LEN + 1 + 1] ;
		memcpy(file,svok,svoklen) ;
		if (!ss_state_check(state,string + sv_signal.res.name)) log_die(LOG_EXIT_SYS,"unitialized service: ",string + sv_signal.res.name) ;
		if (!ss_state_read(&sta,state,string + sv_signal.res.name)) log_dieusys(LOG_EXIT_SYS,"read state of: ",string + sv_signal.res.name) ;
		if (sta.init) log_die(LOG_EXIT_SYS,"unitialized service: ",string + sv_signal.res.name) ;
		if (!s6_svstatus_read(svok,&status)) log_dieusys(LOG_EXIT_SYS,"read status of: ",svok) ;
		isup = status.pid && !status.flagfinishing ;
						
		if (isup && (SIGNAL == SIGUP))
		{
			log_info("Already up: ",string + sv_signal.res.name) ;
			continue ;
		}
		else if (!isup && (SIGNAL >= SIGDOWN))
		{
			log_info("Already down: ",string + sv_signal.res.name) ;
			continue ;
		}
		/** special on reload signal, if the process is down
		 * simply bring it up */
		else if (!isup && (SIGNAL == SIGR))
		{
			sig = "u" ;
			SIGNAL = SIGUP ;
		}
		sv_signal.sigtosend = sig ;	sv_signal.sig = SIGNAL ;
		
		/** notification-fd */
		memcpy(file + svoklen,"/" SS_NOTIFICATION,SS_NOTIFICATION_LEN + 1) ;
		file[svoklen + SS_NOTIFICATION_LEN + 1] = 0 ;
		e = errno ;
		errno = 0 ;
		
		if (access(file, F_OK) < 0 && errno != ENOENT)
			log_warnsys("conflicting format of file: " SS_NOTIFICATION) ;
		else if (errno != ENOENT)
		{
			
			if (!read_uint(file,&sv_signal.notify)) log_dieusys(LOG_EXIT_SYS,"read: ",file) ;
			if (SIGNAL == SIGUP)
			{ sv_signal.sig = SIGRUP ; sv_signal.sigtosend = "uwU" ; }
			else if (SIGNAL == SIGR)
			{ sv_signal.sig = SIGRR ; sv_signal.sigtosend = "rwR" ; }
			else if (SIGNAL == SIGDOWN)
			{ sv_signal.sig = SIGRDOWN ; sv_signal.sigtosend = "dwD" ; }
		}
		/** max-death-tally */
		if (!death)
		{
			memcpy(file + svoklen,"/" SS_MAXDEATHTALLY, SS_MAXDEATHTALLY_LEN + 1) ;
			file[svoklen + SS_MAXDEATHTALLY_LEN + 1] = 0 ;
			errno = 0 ;
			if (access(file, F_OK) < 0)
				if (errno != ENOENT) log_dieusys(LOG_EXIT_SYS, "access ", file) ;
			
			if (errno == ENOENT)
				sv_signal.ndeath = DEATHSV ;
			else
			{
				if (!read_uint(file,&sv_signal.ndeath)) log_dieusys(LOG_EXIT_SYS,"read: ",file) ;
			}
		}
		else sv_signal.ndeath = death ;
		
		tain_t tcheck ;
		tain_from_millisecs(&tcheck,tsv) ;
		int check = tain_to_millisecs(&tcheck) ;
		if (check > 0)
		{ 
			tain_from_millisecs(&sv_signal.deadline, tsv) ; 
			tsv_g += tsv ; 
		}
		else
		{
			/** timeout-{up/down} */
			char *tm = NULL ;
			unsigned int t ;
			if (SIGNAL <= SIGR)
				tm="/timeout-up" ;
			else tm="/timeout-down" ;
			errno = 0 ;
			size_t tmlen = strlen(tm) ;
			memcpy(file + svoklen,tm, tmlen) ;
			file[svoklen + tmlen] = 0 ;
			if (access(file, F_OK) < 0)
				if (errno != ENOENT) log_dieusys(LOG_EXIT_SYS, "access ", file) ;
			
			if (errno == ENOENT)
			{
				tain_from_millisecs(&sv_signal.deadline, SV_DEADLINE) ;
				tsv_g += SV_DEADLINE ;
			}
			else
			{
				if (!read_uint(file,&t)) log_dieusys(LOG_EXIT_SYS,"read: ",file) ;
				{	
					tain_from_millisecs(&sv_signal.deadline, t) ;
					tsv_g += t ;
				}
			}
		}
		errno = e ;
		if (!genalloc_append(ss_resolve_sig_t,&gakeep,&sv_signal)) log_dieusys(LOG_EXIT_SYS,"append services selection with: ",string + sv_signal.res.name) ;
	}
	/** nothing to do */
	if (!genalloc_len(ss_resolve_sig_t,&gakeep)) goto finish ;
	
	//ttmain = tain_infinite_relative ;
	tain_from_millisecs(&ttmain,tsv_g) ;
	tain_now_set_stopwatch_g() ;
	tain_add_g(&ttmain,&ttmain) ;

	int spfd = selfpipe_init() ;
	if (spfd < 0) log_dieusys(LOG_EXIT_SYS, "selfpipe_trap") ;
	if (selfpipe_trap(SIGCHLD) < 0) log_dieusys(LOG_EXIT_SYS, "selfpipe_trap") ;
	if (selfpipe_trap(SIGINT) < 0) log_dieusys(LOG_EXIT_SYS, "selfpipe_trap") ;
	if (selfpipe_trap(SIGTERM) < 0) log_dieusys(LOG_EXIT_SYS, "selfpipe_trap") ;
	if (sig_ignore(SIGPIPE) < 0) log_dieusys(LOG_EXIT_SYS,"ignore SIGPIPE") ;
	
	if (!svc_init_pipe(&fifo,&gakeep,&ttmain)) log_dieu(LOG_EXIT_SYS,"init pipe") ;
		
	ret = doit(spfd,&gakeep,&ttmain) ;
	
	finish:
		ftrigr_end(&fifo) ;
		selfpipe_finish() ;
		stralloc_free(&sares) ;
		ss_resolve_graph_free(&graph) ;
		genalloc_free(ss_resolve_sig_t,&gakeep) ;
		ss_resolve_free(&res) ;
	
	return (!ret) ? 111 : 0 ;		
}
