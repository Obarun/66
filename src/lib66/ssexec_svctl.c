/* 
 * ssexec_svctl.c
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
#include <signal.h>
#include <stdio.h>
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
#include <s6/ftrigr.h>

#include <66/svc.h>
#include <66/constants.h>
#include <66/utils.h>
#include <66/tree.h>
#include <66/ssexec.h>

//#include <stdio.h>


static stralloc svkeep = STRALLOC_ZERO ;
static genalloc gakeep = GENALLOC_ZERO ; //type svc_sig_s
static stralloc saresolve = STRALLOC_ZERO ;

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
	exitcode = WIFEXITED(status.wstat) ;
	exitstatus = WEXITSTATUS(status.wstat) ;
	if (exitcode && exitstatus <= 1) return 1 ;
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
    { DEAD,		DEAD,	DEAD,	DEAD,	WAIT,	WAIT,	GOTIT,	WAIT, 	UKNOW },// SIGX
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

int handle_case(char c, svc_sig *sv_signal)
{
	int p, h, err ;
	unsigned int state ;
	state = sv_signal->sig ;
	err = 2 ;
	
	p = chtenum[(unsigned char)c] ;
	
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
					break ;
		case WAIT:
					err = 1 ;
					break ;
		case DEAD:
					err = 2 ;
					break ;
		case DONE:
					err = 0 ;
					break ;
		case PERM:
					err = 3 ;
					break ;
		case UKNOW:
		default:
					VERBO3 strerr_warnw1x("invalid state, make a bug report");
					err = 2 ;
					break ;
	}
	
	return err ;
}

static int handle_signal_pipe(svc_sig *svc)
{
	char *sv = svkeep.s + svc->scan ;
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
static void announce(svc_sig *sv_signal)
{

	int r = sv_signal->state ;
	char *sv = svkeep.s + sv_signal->scan ;
	if (r == 3) { VERBO2 strerr_warnw3x(sv," report permanent failure to bring ",(sv_signal->sig > 1) ? "down" : "up") ; }
	else if (r == 2) { VERBO2 strerr_warnwu3x("bring ", (sv_signal->sig > 3) ? "down " : "up ", sv) ; }
	else if (r == 1) { VERBO2 strerr_warni4x(sv," is ",(sv_signal->sig > 3) ? "down" : "up"," but not notified by the daemon itself") ; }
	else if (!r) { VERBO2 strerr_warni4x(sv,": ",(sv_signal->sig > 3) ? "stopped" : "started"," successfully") ; }
}

int svc_listen(genalloc *gasv, ftrigr_t *fifo,int spfd,svc_sig *svc)
{
	int r ;
	iopause_fd x[2] = { { .fd = ftrigr_fd(fifo), .events = IOPAUSE_READ } , { .fd = spfd, .events = IOPAUSE_READ, .revents = 0 } } ;
		
	tain_t t ;
	tain_now_g() ;
	tain_add_g(&t,&svc->deadline) ;
	
	VERBO3 strerr_warnt2x("start iopause for: ",svkeep.s + svc->name) ;
	
	for(;;)
	{
		char receive ;
		r = ftrigr_check(fifo,svc->ids, &receive) ;
		if (r < 0) { VERBO3 strerr_warnwu1sys("ftrigr_check") ; return 0 ; } 
		if (r)
		{
			svc->state = handle_case(receive, svc) ;
			if (svc->state < 0) return 0 ;
			if (!svc->state){ announce(svc) ; break ; }
			else if (svc->state > 2){ announce(svc) ; break ; }
		}
		if (!(svc->ndeath))
		{
			VERBO2 strerr_warnw2x("number of try exceeded for: ",svkeep.s + svc->scan) ;
			announce(svc) ;
			break ;
		}
		
		r = iopause_g(x, 2, &t) ;
		if (r < 0){ VERBO3 strerr_warnwu1sys("iopause") ; return 0 ; }
		/** timeout is not a error , the iopause process did right */
		else if (!r) { VERBO3 strerr_warnt2x("timed out reached for: ",svkeep.s + svc->name) ; announce(svc) ; break ; } 
		
		if (x[1].revents & IOPAUSE_READ) return handle_signal_pipe(svc) ? 1 : 0 ;
				
		if (x[0].revents & IOPAUSE_READ)  
		{	
			if (ftrigr_update(fifo) < 0){ VERBO3 strerr_warnwu1sys("ftrigr_update") ; return 0 ; }
			svc->ndeath--;
		}
	}

	return 1 ;
}

int ssexec_svctl(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
	
	// be sure that the global var are set correctly
	svkeep = stralloc_zero ;
	gakeep = genalloc_zero ; //type svc_sig_s
	saresolve = stralloc_zero ;
	SV_DEADLINE = 3000 ;
	DEATHSV = 10 ;
	
	int r, e, isup ;
	unsigned int death, tsv ;
	int SIGNAL = -1 ;
	
	svc_sig sv_signal = SVC_SIG_ZERO ;
	
	char *sig = 0 ;
	
	ftrigr_t fifo = FTRIGR_ZERO ;
	s6_svstatus_t status = S6_SVSTATUS_ZERO ;
	
	tsv = death = 0 ;
	
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
				case 'U' :	if (SIGNAL > 0) exitusage(usage_svctl) ; SIGNAL = SIGRUP ; sig = "u" ; break ;
				case 'd' : 	if (SIGNAL > 0) ; SIGNAL = SIGDOWN ; sig = "d" ; break ;
				case 'D' :	if (SIGNAL > 0) ; SIGNAL = SIGRDOWN ; sig = "d" ; break ;
				case 'r' :	if (SIGNAL > 0) ; SIGNAL = SIGR ; sig = "r" ; break ;
				case 'R' :	if (SIGNAL > 0) ; SIGNAL = SIGRR ; sig = "r" ; break ;
				case 'X' :	if (SIGNAL > 0) ; SIGNAL = SIGX ; sig = "dx" ; break ;
				case 'K' :	if (SIGNAL > 0) ; SIGNAL = SIGRDOWN ; sig = "kd" ; break ;
				
				default : exitusage(usage_svctl) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (argc < 1 || (SIGNAL < 0)) exitusage(usage_svctl) ;
	
	if (info->timeout) tsv = info->timeout ;
	
		
	if ((scandir_ok(info->scandir.s)) !=1 ) strerr_dief3sys(111,"scandir: ", info->scandir.s," is not running") ;
	

	for(;*argv;argv++)
	{
		char const *svname = *argv ;
		size_t namelen = strlen(*argv) ;
		size_t srclen ;
		char src[info->tree.len + SS_SVDIRS_LEN + SS_SVC_LEN + 1 + namelen + 16 + 1] ;
		memcpy(src,info->tree.s,info->tree.len) ;
		memcpy(src + info->tree.len,SS_SVDIRS, SS_SVDIRS_LEN) ;
		memcpy(src + info->tree.len + SS_SVDIRS_LEN, SS_SVC, SS_SVC_LEN) ;
		src[info->tree.len + SS_SVDIRS_LEN + SS_SVC_LEN] = '/' ;
		memcpy(src + info->tree.len + SS_SVDIRS_LEN + SS_SVC_LEN + 1,*argv,namelen) ;
		src[info->tree.len + SS_SVDIRS_LEN + SS_SVC_LEN + 1 + namelen ] = 0 ;
		srclen = info->tree.len + SS_SVDIRS_LEN + SS_SVC_LEN + 1 + namelen ;
		
		size_t svoklen ;
		char svok[info->scandir.len + 1 + namelen + 1] ;
		memcpy(svok,info->scandir.s,info->scandir.len) ;
		svok[info->scandir.len] = '/' ;
		memcpy(svok + info->scandir.len + 1 ,svname,namelen + 1) ;
		svoklen = info->scandir.len + 1 + namelen ;
					
		/** do not check the logger*/
		if (!resolve_pointo(&saresolve,info,0,SS_RESOLVE_SRC))
		{
			VERBO3 strerr_warnwu1x("set revolve pointer to source") ;
			return 0 ;
		}
		if (resolve_read(&saresolve,saresolve.s,svname,"logger"))
		{
			if (str_diff(saresolve.s,svname))//0 mean equal
			{
				r = s6_svc_ok(svok) ;
				if (r < 0) strerr_diefu2sys(111,"check ", svok) ;
				if (!r)	strerr_dief2x(110,svok," is not initialized") ;
			}
		}
		
		if (!s6_svstatus_read(svok,&status)) strerr_diefu2sys(111,"read status of: ",svok) ;
		isup = status.pid && !status.flagfinishing ;
			
		if (isup && (SIGNAL <= SIGRUP))
		{
			VERBO2 strerr_warni2x(svok,": already up") ;
			continue ;
		}
		else if (!isup && (SIGNAL >= SIGDOWN))
		{
			VERBO2 strerr_warni2x(svok,": already down") ;
			continue ;
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
		
		sv_signal.sigtosend = sig ;
		sv_signal.sig = SIGNAL ;
		
		/** notification-fd */
		memcpy(svok + svoklen,"/notification-fd",16) ;
		svok[svoklen + 16] = 0 ;
		e = errno ;
		errno = 0 ;
		if (access(svok, F_OK) < 0)
		{
			if (errno != ENOENT) strerr_diefu2sys(111, "access ", svok) ;
			if (SIGNAL == SIGRUP)
			{
				sv_signal.sig = SIGUP ;
				sv_signal.sigtosend = "uwu" ;
				VERBO2 strerr_warnw2x(svok, " is not present - ignoring request for readiness notification") ;
			}
			if (SIGNAL == SIGRR)
			{
				sv_signal.sig = SIGR ;
				sv_signal.sigtosend = "uwr" ;
				VERBO2 strerr_warnw2x(svok, " is not present - ignoring request for readiness notification") ;
			}
			if (SIGNAL == SIGRDOWN)
			{
				sv_signal.sig = SIGDOWN ;
				sv_signal.sigtosend = "dwd" ;
				VERBO2 strerr_warnw2x(svok, " is not present - ignoring request for readiness notification") ;
			}		
		}
		else
		{
			if (!read_uint(svok,&sv_signal.notify)) strerr_diefu2sys(111,"read: ",svok) ;
		}
		
		/** max-death-tally */
		if (!death)
		{
			memcpy(svok + svoklen,"/max-death-tally", 16) ;
			svok[svoklen + 16] = 0 ;
			errno = 0 ;
			if (access(svok, F_OK) < 0)
			{
				if (errno != ENOENT) strerr_diefu2sys(111, "access ", svok) ;
			}
			if (errno == ENOENT)
			{
				sv_signal.ndeath = DEATHSV ;
			}
			else
			{
				if (!read_uint(svok,&sv_signal.ndeath)) strerr_diefu2sys(111,"read: ",svok) ;
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
		
		errno = e ;
		
		if (!genalloc_append(svc_sig,&gakeep,&sv_signal)) strerr_diefu1sys(111,"append genalloc") ;
				
	}
	
	/** nothing to do */
	if (!genalloc_len(svc_sig,&gakeep))	goto finish ;
	
	int spfd = selfpipe_init() ;
	if (spfd < 0) strerr_diefu1sys(111, "selfpipe_trap") ;
	if (selfpipe_trap(SIGINT) < 0) strerr_diefu1sys(111, "selfpipe_trap") ;
	if (selfpipe_trap(SIGTERM) < 0) strerr_diefu1sys(111, "selfpipe_trap") ;
	if (sig_ignore(SIGPIPE) < 0) strerr_diefu1sys(111,"ignore SIGPIPE") ;
		
	if (!svc_init_pipe(&fifo,&gakeep,&svkeep)) strerr_diefu1x(111,"init pipe") ;

	for (unsigned int i = 0 ; i < genalloc_len(svc_sig,&gakeep) ; i++)
	{ 
		svc_sig *sv = &genalloc_s(svc_sig,&gakeep)[i] ;
		if (!svc_listen(&gakeep,&fifo,spfd,sv)) strerr_diefu1x(111,"listen pipe") ;
	}
	
	finish:
		ftrigr_end(&fifo) ;
		selfpipe_finish() ;
		stralloc_free(&svkeep) ;
		genalloc_free(svc_sig,&gakeep) ;
	
	return 0 ;		
}

	

