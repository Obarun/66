/* 
 * svc.h
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
 
#ifndef SVC_H
#define SVC_H

#include <skalibs/tai.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <s6/ftrigr.h>

#include <66/ssexec.h>

typedef struct svstat_s svstat_t, *svstat_t_ref ;
struct svstat_s
{
	int type ;
	char const *name ;
	size_t namelen ;
	int down ;
	int reload ;
	int init ;
	int unsupervise ;
	int remove ;
} ;

#define SVSTAT_ZERO \
{ \
	.type = 0, \
	.name = 0, \
	.namelen = 0, \
	.down = 0, \
	.init = 0, \
	.reload = 0, \
	.unsupervise = 0, \
	.remove = 0 \
}

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
	uint16_t ids ;
	char *sigtosend ;
	int sig ;
	int state ;
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
	.deadline = TAIN_ZERO, \
	.ids = 0, \
	.sigtosend = 0, \
	.sig = 0, \
	.state = -1 \
} 

typedef enum state_e state_t, *state_t_ref ;
enum state_e
{
	SIGUP = 0, // u
	SIGRUP , // U - really up
	SIGR, // r
	SIGRR, // R - really up
	SIGDOWN , // d
	SIGRDOWN ,// D - really down
	SIGX, //X
	SIGO , //0
	SIGSUP //s supervise
} ;
typedef enum sigactions_e sigactions_t, *sigactions_t_ref ;
enum sigactions_e
{
	GOTIT = 0 ,
	WAIT ,
	DEAD ,
	DONE ,
	PERM ,
	UKNOW 
} ;


extern int svc_switch_to(ssexec_t *info,unsigned int where) ;
extern int svc_init(char const *scandir,char const *src, genalloc *ga) ;
extern int svc_init_pipe(ftrigr_t *fifo,genalloc *gasv,stralloc *sasv) ;

#endif
