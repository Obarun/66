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
 


int svc_init(char const *scandir,char const *svsrc, char const *sv,ftrigr_t *fifo)
{
	int r ;
	gid_t gid = getgid() ;
	uint16_t id ;
	size_t svlen = strlen(sv) ;
	char s[svlen + 6 + 1] ;
	memcpy(s,sv,svlen) ;
	s[svlen] = 0 ;
		
	VERBO3 strerr_warnt4x("copy: ",svsrc, " to ", s) ;
	if (!hiercopy(svsrc, s))
	{
		VERBO3 strerr_warnwu4sys("copy: ",svsrc," to: ",s) ;
		return 0 ;
	}
	memcpy(s + svlen, "/down", 5) ;
	s[svlen + 5] = 0 ;
				
	VERBO3 strerr_warnt2x("create file: ",s) ;
	if (!touch(s))
	{
		VERBO3 strerr_warnwu2sys("create file: ",s) ;
		return 0 ;
	}
	memcpy(s + svlen, "/event", 6) ;
	s[svlen + 6] = 0 ;	
	VERBO3 strerr_warnt2x("create fifo: ",s) ;
	if (!ftrigw_fifodir_make(s, gid, 0))
	{
		VERBO3 strerr_warnwu2sys("create fifo: ",s) ;
		return 0 ;
	}
	VERBO3 strerr_warnt2x("subcribe to fifo: ",s) ;
	/** unsubscribe automatically, options is 0 */
	id = ftrigr_subscribe_g(fifo, s, "s", 0, &DEADLINE) ;
	if (!id)
	{
		VERBO3 strerr_warnwu2x("subcribe to fifo: ",s) ;
		return 0 ;
	}
	
	VERBO3 strerr_warnt2x("reload scandir: ",scandir) ;
	r = s6_svc_writectl(scandir, S6_SVSCAN_CTLDIR, "an", 2) ;
	if (r < 0)
	{
		VERBO3 strerr_warnw3sys("something is wrong with the ",scandir, "/" S6_SVSCAN_CTLDIR " directory. errno reported") ;
		return -1 ;
	}
	if (!r)
	{
		VERBO3 strerr_warnw3x("scandir: ",scandir, " is not running") ;
		return -1 ;
	}
	VERBO3 strerr_warnt2x("waiting for events on fifo: ",s) ;
	if (ftrigr_wait_and_g(fifo, &id, 1, &DEADLINE) < 0)
			return 0 ;
	

	return 1 ;

}
