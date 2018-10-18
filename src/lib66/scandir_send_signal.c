/* 
 * scandir_ok.c
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
 
#include <66/utils.h>

#include <oblibs/error2.h>

#include <s6/s6-supervise.h>

#define DATASIZE 64

int scandir_send_signal(char const *scandir,char const *signal)
{
	char data[DATASIZE] ;
	unsigned int datalen = 0 ;
	
	int id = strlen(signal) ;
	while (datalen < id)
	{
		data[datalen] = signal[datalen] ;
		datalen++ ;
	}	
	if (datalen >= DATASIZE)
	{
		VERBO3 strerr_warnw2x("too many command to send to: ",scandir) ;
		return 0 ;
	}
	
	switch (s6_svc_writectl(scandir, S6_SVSCAN_CTLDIR, data, datalen))
	{
		case -1: VERBO3 strerr_warnwu2sys("control: ", scandir) ; 
				return 0 ;
		case -2: VERBO3 strerr_warnw3sys("something is wrong with the ", scandir, "/" S6_SVSCAN_CTLDIR " directory. errno reported") ; 
				return 0 ;
		case 0: VERBO3 strerr_warnwu3x("control: ", scandir, ": supervisor not listening") ;
				return 0 ;
	}

	return 1 ;
}
