/* 
 * svc.h
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
 
#ifndef SVC_H
#define SVC_H

#include <skalibs/tai.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <s6/ftrigr.h>

#include <66/resolve.h>
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

typedef struct ss_resolve_sig_s ss_resolve_sig_t, *ss_resovle_sig_t_ref ;
struct ss_resolve_sig_s
{
	ss_resolve_t res ;
	unsigned int notify ;
	unsigned int ndeath;
	tain_t deadline ;
	uint16_t ids ;
	char *sigtosend ;
	int sig ;
	int state ;
} ;

#define RESOLVE_SIG_ZERO \
{ \
	.res = RESOLVE_ZERO, \
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
extern int svc_init(ssexec_t *info,char const *src, genalloc *ga) ;
extern int svc_init_pipe(ftrigr_t *fifo,genalloc *gasv) ;
extern int svc_shutnremove(ssexec_t *info, genalloc *ga,char const *sig,  char const *const *envp) ;
extern int svc_send(ssexec_t *info,genalloc *ga,char const *sig,char const *const *envp) ;
#endif
