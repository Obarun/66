/* 
 * svc_init.c
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

#include <66/svc.h>

#include <string.h>
#include <stdlib.h>

#include <oblibs/error2.h>
#include <oblibs/stralist.h>
#include <oblibs/string.h>
#include <oblibs/directory.h>
#include <oblibs/types.h>

#include <skalibs/genalloc.h>
#include <skalibs/types.h>
#include <skalibs/tai.h>
#include <skalibs/djbunix.h>

#include <s6/s6-supervise.h>
#include <s6/ftrigr.h>
#include <s6/ftrigw.h>

#include <66/utils.h>
#include <66/resolve.h>
#include <66/constants.h>
#include <66/ssexec.h>

#include <stdio.h>

int svc_init(ssexec_t *info,char const *src, genalloc *ga)
{
	
	int logname, writein ;
	gid_t gid = getgid() ;
	uint16_t id ;
		
	ftrigr_t fifo = FTRIGR_ZERO ;
	genalloc gadown = GENALLOC_ZERO ;
	genalloc ids = GENALLOC_ZERO ; // uint16_t
	stralloc sares = STRALLOC_ZERO ;
	
	tain_t deadline ;
	tain_now_g() ;
	tain_addsec(&deadline,&STAMP,2) ;	
	
	if (!access(info->tree.s,W_OK)) writein = SS_DOUBLE ;
	else writein = SS_SIMPLE ;
	
	if (!ftrigr_startf(&fifo, &deadline, &STAMP))
		goto err ;
	
	if (!ss_resolve_create_live(info)) { VERBO1 strerr_warnwu1sys("create live state") ; goto err ; }
	
	for (unsigned int i=0 ; i < genalloc_len(ss_resolve_t,ga); i++) 
	{
		logname = 0 ;
		char *string = genalloc_s(ss_resolve_t,ga)[i].sa.s ;
		char *name = string + genalloc_s(ss_resolve_t,ga)[i].name ;
		if (s6_svc_ok(string + genalloc_s(ss_resolve_t,ga)[i].runat))
		{
			VERBO1 strerr_warni3x("Initialization aborted -- ",name," already initialized") ;
			continue ;
		}
		logname = get_rstrlen_until(name,SS_LOG_SUFFIX) ;
		if (logname > 0) name = string + genalloc_s(ss_resolve_t,ga)[i].logassoc ;
		
		size_t namelen = strlen(name) ;
		size_t srclen = strlen(src) ;	
		char svsrc[srclen + 1 + namelen + 1] ;
		memcpy(svsrc,src,srclen) ;
		svsrc[srclen] = '/' ;
		memcpy(svsrc + srclen + 1,name,namelen) ;
		svsrc[srclen + 1 + namelen] = 0 ;
		
		size_t svscanlen ;
		if (logname > 0) svscanlen = strlen(string + genalloc_s(ss_resolve_t,ga)[i].runat) - SS_LOG_SUFFIX_LEN ;
		else svscanlen = strlen(string + genalloc_s(ss_resolve_t,ga)[i].runat) ;
		char svscan[svscanlen + 6 + 1] ;
		memcpy(svscan,string + genalloc_s(ss_resolve_t,ga)[i].runat,svscanlen) ;
		svscan[svscanlen] = 0 ;
		
		VERBO3 strerr_warnt2x("init service: ", string + genalloc_s(ss_resolve_t,ga)[i].name) ;
		/** if logger was created do not pass here to avoid to erase
		 * the fifo of the logger*/
		if (!scan_mode(svscan,S_IFDIR))
		{
			VERBO3 strerr_warnt4x("copy: ",svsrc, " to ", svscan) ;
			if (!hiercopy(svsrc,svscan))
			{
				VERBO3 strerr_warnwu4sys("copy: ",svsrc," to: ",svscan) ;
				goto err ;
			}
		}
		
		/** if logger you need to copy again the real path */
		svscanlen = strlen(string + genalloc_s(ss_resolve_t,ga)[i].runat) ;
		memcpy(svscan,string + genalloc_s(ss_resolve_t,ga)[i].runat,svscanlen) ;
		svscan[svscanlen] = 0 ;
		/** if logger and the reload was asked the folder xxx/log doesn't exist
		 * check it and create again if doesn't exist */
		if (!scan_mode(svscan,S_IFDIR))
		{
			size_t tmplen = strlen(svsrc) ;
			char tmp[tmplen + 4 + 1] ;
			memcpy(tmp,svsrc,tmplen) ;
			memcpy(tmp + tmplen,"/log",4) ;
			tmp[tmplen + 4] = 0 ;
			VERBO3 strerr_warnt4x("copy: ",tmp, " to ", svscan) ;
			if (!hiercopy(tmp,svscan))
			{
				VERBO3 strerr_warnwu4sys("copy: ",tmp," to: ",svscan) ;
				goto err ;
			}
		}
		memcpy(svscan + svscanlen, "/down", 5) ;
		svscan[svscanlen + 5] = 0 ;
	
		if (!genalloc_s(ss_resolve_t,ga)[i].down)
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
		if (!genalloc_append(uint16_t, &ids, &id)) goto err ;
	}
	if (genalloc_len(uint16_t,&ids))
	{
		VERBO3 strerr_warnt2x("reload scandir: ",info->scandir.s) ;
		if (scandir_send_signal(info->scandir.s,"an") <= 0) 
		{
			VERBO3 strerr_warnwu2sys("reload scandir: ",info->scandir.s) ;
			goto err ;
		}
	
	
		VERBO3 strerr_warnt1x("waiting for events on fifo") ;
		if (ftrigr_wait_and_g(&fifo, genalloc_s(uint16_t, &ids), genalloc_len(uint16_t, &ids), &deadline) < 0)
				goto err ;
	
		for (unsigned int i = 0 ; i < genalloc_len(stralist,&gadown) ; i++)
		{
			VERBO3 strerr_warnt2x("Delete down file at: ",gaistr(&gadown,i)) ;
			if (unlink(gaistr(&gadown,i)) < 0 && errno != ENOENT) goto err ;
		}
		if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_LIVE))		
		{
			VERBO3 strerr_warnwu1x("set revolve pointer to live") ;
			goto err ;
		}
		for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,ga) ; i++)
		{
			char const *string = genalloc_s(ss_resolve_t,ga)[i].sa.s ;
			char const *name = string + genalloc_s(ss_resolve_t,ga)[i].name  ;
			ss_resolve_setflag(&genalloc_s(ss_resolve_t,ga)[i],SS_FLAGS_INIT,SS_FLAGS_FALSE) ;
			ss_resolve_setflag(&genalloc_s(ss_resolve_t,ga)[i],SS_FLAGS_RUN,SS_FLAGS_TRUE) ;
			VERBO2 strerr_warni2x("Write resolve file of: ",name) ;
			if (!ss_resolve_write(&genalloc_s(ss_resolve_t,ga)[i],sares.s,name,writein))
			{
				VERBO1 strerr_warnwu2sys("write resolve file of: ",name) ;
				goto err ;
			}
			VERBO1 strerr_warni2x("Initialized successfully: ",name) ;
		}
	}
	ftrigr_end(&fifo) ;
	genalloc_deepfree(stralist,&gadown,stra_free) ;
	genalloc_free(uint16_t, &ids) ;
	stralloc_free(&sares) ;
	return 1 ;
	
	err:
		ftrigr_end(&fifo) ;
		genalloc_free(uint16_t, &ids) ;
		genalloc_deepfree(stralist,&gadown,stra_free) ;
		ftrigr_end(&fifo) ;
		stralloc_free(&sares) ;
		return 0 ;

}
