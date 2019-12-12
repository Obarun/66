/* 
 * scandir_ok.c
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
 
#include <66/utils.h>

#include <stddef.h>

#include <oblibs/log.h>

#include <s6/s6-supervise.h>

#define DATASIZE 64

int scandir_send_signal(char const *scandir,char const *signal)
{
	char data[DATASIZE] ;
	size_t datalen = 0 ;
	
	size_t id = strlen(signal) ;
	while (datalen < id)
	{
		data[datalen] = signal[datalen] ;
		datalen++ ;
	}	
	if (datalen >= DATASIZE)
		log_warn_return(LOG_EXIT_ZERO,"too many command to send to: ",scandir) ;
	
	switch (s6_svc_writectl(scandir, S6_SVSCAN_CTLDIR, data, datalen))
	{
		case -1: log_warnusys("control: ", scandir) ; 
				return 0 ;
		case -2: log_warnsys("something is wrong with the ", scandir, "/" S6_SVSCAN_CTLDIR " directory. errno reported") ; 
				return 0 ;
		case 0: log_warnu("control: ", scandir, ": supervisor not listening") ;
				return 0 ;
	}

	return 1 ;
}
