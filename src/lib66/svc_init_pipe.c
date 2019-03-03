/* 
 * svc_init_pipe.c
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
 
#include <stddef.h>
#include <stdlib.h>

#include <66/svc.h>

#include <oblibs/error2.h>

#include <skalibs/genalloc.h>
#include <skalibs/stralloc.h>
#include <skalibs/tai.h>

#include <s6/ftrigr.h>
#include <s6/ftrigw.h>
#include <s6/s6-supervise.h>

#include <66/utils.h>
#include <66/resolve.h>


int svc_init_pipe(ftrigr_t *fifo,genalloc *gasv)
{
	tain_t ttmain ;
	
	tain_now_g() ;
	tain_addsec(&ttmain,&STAMP,2) ;
	
	ss_resolve_sig_t *svc ;
	
	//VERBO2 strerr_warni1x("initiate fifo: fifo") ;
	if (!ftrigr_startf(fifo, &ttmain, &STAMP))
		VERBO3 { strerr_warnwu1sys("initiate fifo") ; return 0 ; }
		
	for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_sig_t,gasv) ; i++)
	{
		svc = &genalloc_s(ss_resolve_sig_t,gasv)[i] ;
		int r ;
		size_t siglen = strlen(svc->sigtosend) ;
		char *svok = svc->res.sa.s + svc->res.runat ;
		size_t scanlen = strlen(svok) ;
		
		char svfifo[scanlen + 6 + 1] ;
		memcpy(svfifo, svok,scanlen) ;
		memcpy(svfifo + scanlen, "/event",6) ;
		svfifo[scanlen + 6] = 0 ;
	
		VERBO3 strerr_warnt2x("clean up fifo: ", svfifo) ;
		if (!ftrigw_clean (svok))
		{
			VERBO3 strerr_warnwu2sys("clean up fifo: ", svfifo) ;
			return 0 ;
		}
	
		VERBO3 strerr_warnt2x("subcribe to fifo: ",svfifo) ;
		svc->ids = ftrigr_subscribe_g(fifo, svfifo, "[DuUdOxs]", FTRIGR_REPEAT, &ttmain) ;
		if (!svc->ids)
		{
			VERBO3 strerr_warnwu2sys("subcribe to fifo: ",svfifo) ;
			goto end ;
		}
					
		VERBO3 strerr_warnt6x("send signal: ",svc->sigtosend," to: ", svok,"/",S6_SUPERVISE_CTLDIR) ;
		r = s6_svc_writectl(svok, S6_SUPERVISE_CTLDIR, svc->sigtosend, siglen) ;
		if (r < 0)
		{
			VERBO3 strerr_warnwu3sys("something is wrong with the ",svok, "/" S6_SUPERVISE_CTLDIR " directory. errno reported") ;
			goto end ;
		}
		if (!r)
		{
			VERBO3 strerr_warnw3x("sv: ",svok, " is not supervised") ;
			goto end ;
		}
	}
	
	
	return 1 ;
	
	end:
		
		return 0 ;
}
