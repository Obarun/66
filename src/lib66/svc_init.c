/* 
 * svc_init.c
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

#include <66/svc.h>

#include <string.h>
#include <stdlib.h>

#include <oblibs/error2.h>
#include <oblibs/stralist.h>

#include <skalibs/genalloc.h>
#include <skalibs/types.h>
#include <skalibs/tai.h>
#include <skalibs/djbunix.h>

#include <s6/s6-supervise.h>
#include <s6/ftrigr.h>
#include <s6/ftrigw.h>

#include <66/utils.h>

int svc_init(char const *scandir,char const *src, genalloc *ga)
{
	
	int r ;
	gid_t gid = getgid() ;
	uint16_t id ;
		
	ftrigr_t fifo = FTRIGR_ZERO ;
	genalloc gadown = GENALLOC_ZERO ;
	
	tain_t deadline ;
	tain_now_g() ;
	tain_addsec(&deadline,&STAMP,2) ;	
	
	VERBO3 strerr_warnt1x("iniate fifo: fifo") ;
		if (!ftrigr_startf(&fifo, &deadline, &STAMP))
			return 0 ;
		
	for (unsigned int i=0 ; i < genalloc_len(svstat_t,ga); i++) 
	{
		char const *name = genalloc_s(svstat_t,ga)[i].name ;
		size_t namelen = genalloc_s(svstat_t,ga)[i].namelen ;
			
		size_t srclen = strlen(src) ;	
		char svsrc[srclen + 1 + namelen + 1] ;
		memcpy(svsrc,src,srclen) ;
		svsrc[srclen] = '/' ;
		memcpy(svsrc + srclen + 1, name,namelen) ;
		svsrc[srclen + 1 + namelen] = 0 ;
		
		size_t svscanlen = strlen(scandir) ;
		char svscan[svscanlen + 1 + namelen + 6 + 1] ;
		memcpy(svscan,scandir,svscanlen) ;
		svscan[svscanlen] = '/' ;
		memcpy(svscan + svscanlen + 1, name,namelen) ;
		svscanlen = svscanlen + 1 + namelen ;
		svscan[svscanlen] = 0 ;
		
		VERBO3 strerr_warnt2x("init service: ", svscan) ;
			
		VERBO3 strerr_warnt4x("copy: ",svsrc, " to ", svscan) ;
		if (!hiercopy(svsrc,svscan ))
		{
			VERBO3 strerr_warnwu4sys("copy: ",svsrc," to: ",svscan) ;
			goto err ;
		}
		memcpy(svscan + svscanlen, "/down", 5) ;
		svscan[svscanlen + 5] = 0 ;
		
		if (!genalloc_s(svstat_t,ga)[i].down)
		{
			if (!stra_add(&gadown,svscan))
			{
				VERBO3 strerr_warnwu3x("add: ",svscan," to genalloc") ;
				goto err ;
			}
		}
		
		VERBO3 strerr_warnt2x("create file: ",svscan) ;
		if (!touch(svscan))
		{
			VERBO3 strerr_warnwu2sys("create file: ",svscan) ;
			goto err ;
		}
		memcpy(svscan + svscanlen, "/event", 6) ;
		svscan[svscanlen + 6] = 0 ;	
		VERBO3 strerr_warnt2x("create fifo: ",svscan) ;
		if (!ftrigw_fifodir_make(svscan, gid, 0))
		{
			VERBO3 strerr_warnwu2sys("create fifo: ",svscan) ;
			goto err ;
		}
		VERBO3 strerr_warnt2x("subcribe to fifo: ",svscan) ;
		/** unsubscribe automatically, options is 0 */
		id = ftrigr_subscribe_g(&fifo, svscan, "s", 0, &deadline) ;
		if (!id)
		{
			VERBO3 strerr_warnwu2x("subcribe to fifo: ",svscan) ;
			goto err ;
		}
	}
	VERBO3 strerr_warnt2x("reload scandir: ",scandir) ;
	r = s6_svc_writectl(scandir, S6_SVSCAN_CTLDIR, "an", 2) ;
	if (r < 0)
	{
		VERBO3 strerr_warnw3sys("something is wrong with the ",scandir, "/" S6_SVSCAN_CTLDIR " directory. errno reported") ;
		goto err ;
	}
	if (!r)
	{
		VERBO3 strerr_warnw3x("scandir: ",scandir, " is not running, make a bug report") ;
		goto err ;
	}
	
	VERBO3 strerr_warnt1x("waiting for events on fifo") ;
	if (ftrigr_wait_and_g(&fifo, &id, 1, &deadline) < 0)
			goto err ;
	
	for (unsigned int i = 0 ; i < genalloc_len(stralist,&gadown) ; i++)
	{
		VERBO3 strerr_warnt2x("delete down file for: ",genalloc_s(svstat_t,ga)[i].name) ;
		if (unlink(gaistr(&gadown,i)) < 0 && errno != ENOENT) return 0 ;
	}
		
	ftrigr_end(&fifo) ;
	genalloc_deepfree(stralist,&gadown,stra_free) ;
	
	return 1 ;
	
	err:
		ftrigr_end(&fifo) ;
		return 0 ;

}
