/* 
 * ssexec_dbctl.c
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
#include <sys/types.h>//pid_t
#include <stdio.h>

#include <oblibs/obgetopt.h>
#include <oblibs/error2.h>
#include <oblibs/string.h>

#include <skalibs/tai.h>
#include <skalibs/types.h>//UINT_FMT
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>

#include <s6/s6-supervise.h>
#include <s6-rc/config.h>

#include <66/constants.h>
#include <66/utils.h>
#include <66/db.h>
#include <66/enum.h>
#include <66/resolve.h>
#include <66/ssexec.h>
#include <66/state.h>

static unsigned int DEADLINE = 0 ;

static void rebuild_list(ss_resolve_graph_t *graph,ssexec_t *info, int what)
{
	int isup ;
	s6_svstatus_t status = S6_SVSTATUS_ZERO ;
	genalloc gatmp = GENALLOC_ZERO ;
	ss_state_t sta = STATE_ZERO ;

	for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,&graph->sorted) ; i++)
	{
		char *string = genalloc_s(ss_resolve_t,&graph->sorted)[i].sa.s ;
		char *name = string + genalloc_s(ss_resolve_t,&graph->sorted)[i].name ;
		char *runat = string + genalloc_s(ss_resolve_t,&graph->sorted)[i].runat ;
		char *state = string + genalloc_s(ss_resolve_t,&graph->sorted)[i].state ;
		if (!ss_state_check(state,name)) strerr_dief2x(111,"unitialized service: ",name) ;
		if (!ss_state_read(&sta,state,name)) strerr_diefu2sys(111,"read state of: ",name) ;
		if (sta.init) strerr_dief2x(111,"unitialized service: ",name) ;
		
		int type = genalloc_s(ss_resolve_t,&graph->sorted)[i].type ;
		if (type == LONGRUN && genalloc_s(ss_resolve_t,&graph->sorted)[i].disen)
		{
			if (!s6_svstatus_read(runat,&status)) strerr_diefu2sys(111,"read status of: ",runat) ;
			isup = status.pid && !status.flagfinishing ;
				
			if (isup && !what)
			{
				VERBO1 strerr_warni2x("Already up: ",name) ;
				continue ;
			}
			else if (!isup && what)
			{
				VERBO1 strerr_warni2x("Already down: ",name) ;
				continue ;
			}
		}
		else
		{
			if (!sta.state && what)
			{
				VERBO1 strerr_warni2x("Already down: ",name) ;
				continue ;
			}
			if (sta.state && !what)
			{
				VERBO1 strerr_warni2x("Already up: ",name) ;
				continue ;
			}
		}
		genalloc_append(ss_resolve_t,&gatmp,&genalloc_s(ss_resolve_t,&graph->sorted)[i]) ;
	}
	genalloc_copy(ss_resolve_t,&graph->sorted,&gatmp) ;
	genalloc_free(ss_resolve_t,&gatmp) ;
}

/* signal = 0 -> reload
 * signal = 1 -> up 
 * signal > 1 -> down*/
static int check_status(genalloc *gares,ssexec_t *info,int signal)
{
	int reload = 0 , up = 0 , ret = 0 ;
	
	s6_svstatus_t status = S6_SVSTATUS_ZERO ;
	ss_state_t sta = STATE_ZERO ;
	if (!signal) reload = 1 ;
	else if (signal == 1) up = 1 ;
	
	for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,gares) ; i++)
	{ 
		int nret = 0 ;
		ss_resolve_t_ref pres = &genalloc_s(ss_resolve_t,gares)[i] ;
		char const *name = pres->sa.s + pres->name ;
		char const *state = pres->sa.s + pres->state ;
		/** do not touch the Master resolve file*/
		if (obstr_equal(name,SS_MASTER + 1)) continue ;
		/** only check longrun service */
		if (pres->type == LONGRUN)
		{	
			if (!s6_svstatus_read(pres->sa.s + pres->runat,&status)) strerr_diefu2sys(111,"read status of: ",pres->sa.s + pres->runat) ;
			else if (up)
			{
				if ((!WEXITSTATUS(status.wstat) && !WIFSIGNALED(status.wstat)) || (WIFSIGNALED(status.wstat) && !WEXITSTATUS(status.wstat) && (WTERMSIG(status.wstat) == 15 )))
				{
					ss_state_setflag(&sta,SS_FLAGS_PID,status.pid) ;
					ss_state_setflag(&sta,SS_FLAGS_STATE,SS_FLAGS_TRUE) ;
				}
				else
				{
					VERBO1 strerr_warnwu2x("start: ",name) ;
					nret = 1 ;
					ss_state_setflag(&sta,SS_FLAGS_PID,SS_FLAGS_FALSE) ;
					ss_state_setflag(&sta,SS_FLAGS_STATE,SS_FLAGS_FALSE) ;
				}
			}
			else 
			{
				if ((!WEXITSTATUS(status.wstat) && !WIFSIGNALED(status.wstat)) || (WIFSIGNALED(status.wstat) && !WEXITSTATUS(status.wstat) && (WTERMSIG(status.wstat) == 15 )))
				{
					ss_state_setflag(&sta,SS_FLAGS_PID,SS_FLAGS_FALSE) ;
					ss_state_setflag(&sta,SS_FLAGS_STATE,SS_FLAGS_FALSE) ;
				}
				else
				{
					VERBO1 strerr_warnwu2x("stop: ",name) ;
					ss_state_setflag(&sta,SS_FLAGS_PID,status.pid) ;
					ss_state_setflag(&sta,SS_FLAGS_STATE,SS_FLAGS_TRUE) ;
					nret = 1 ;
				}
			}
		}
		if (nret) ret = 111 ;
		ss_state_setflag(&sta,SS_FLAGS_RELOAD,SS_FLAGS_FALSE) ;
		ss_state_setflag(&sta,SS_FLAGS_INIT,SS_FLAGS_FALSE) ;
		ss_state_setflag(&sta,SS_FLAGS_UNSUPERVISE,SS_FLAGS_FALSE) ;
		if (pres->type == BUNDLE || pres->type == ONESHOT)
			ss_state_setflag(&sta,SS_FLAGS_STATE,SS_FLAGS_TRUE) ;
			
		VERBO2 strerr_warni2x("Write state file of: ",name) ;
		if (!ss_state_write(&sta,state,name))
		{
			VERBO1 strerr_warnwu2sys("write state file of: ",name) ;
			ret = 111 ;
		}
		if (!nret) VERBO1 strerr_warni3x(reload ? "Reloaded" : up ? "Started" : "Stopped"," successfully: ",name) ;
	}	
	
	return ret ;
}
static pid_t send(genalloc *gasv, char const *livetree, char const *signal,char const *const *envp)
{
	tain_t deadline ;
    tain_from_millisecs(&deadline, DEADLINE) ;
       
    tain_now_g() ;
    tain_add_g(&deadline, &deadline) ;

	char const *newargv[10 + genalloc_len(ss_resolve_t,gasv)] ;
	unsigned int m = 0 ;
	char fmt[UINT_FMT] ;
	fmt[uint_fmt(fmt, VERBOSITY)] = 0 ;
	
	char tt[UINT32_FMT] ;
	tt[uint32_fmt(tt,DEADLINE)] = 0 ;
	
	newargv[m++] = S6RC_BINPREFIX "s6-rc" ;
	newargv[m++] = "-v" ;
	newargv[m++] = fmt ;
	newargv[m++] = "-t" ;
	newargv[m++] = tt ;
	newargv[m++] = "-l" ;
	newargv[m++] = livetree ;
	newargv[m++] = signal ;
	newargv[m++] = "change" ;
	
	for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,gasv); i++)
		newargv[m++] = genalloc_s(ss_resolve_t,gasv)[i].sa.s + genalloc_s(ss_resolve_t,gasv)[i].name  ;
	
	newargv[m++] = 0 ;

	return child_spawn0(newargv[0],newargv,envp) ;
	
}

int ssexec_dbctl(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
	DEADLINE = 0 ;
	
	if (info->timeout) DEADLINE = info->timeout ;

	unsigned int up, down, reload, ret, reverse ;
	
	int r, wstat ;
	pid_t pid ;
	
	char *signal = 0 ;
	char *mainsv = SS_MASTER + 1 ;
	
	genalloc gares = GENALLOC_ZERO ; //ss_resolve_t
	stralloc tmp = STRALLOC_ZERO ;
	stralloc sares = STRALLOC_ZERO ;
	ss_resolve_graph_t graph = RESOLVE_GRAPH_ZERO ;
	ss_resolve_t res = RESOLVE_ZERO ;
	
	up = down = reload = ret = reverse = 0 ;
	
	//PROG = "66-dbctl" ;
	{
		subgetopt_t l = SUBGETOPT_ZERO ;

		for (;;)
		{
			int opt = getopt_args(argc,argv, "udr", &l) ;
			if (opt == -1) break ;
			if (opt == -2) strerr_dief1x(110,"options must be set first") ;
			switch (opt)
			{
				case 'u' :	up = 1 ; if (down || reload) exitusage(usage_dbctl) ; break ;
				case 'd' : 	down = 1 ; if (up || reload) exitusage(usage_dbctl) ; break ;
				case 'r' : 	reload = 1 ; if (down || up) exitusage(usage_dbctl) ; break ;
				default : exitusage(usage_dbctl) ; 
			}
		}
		argc -= l.ind ; argv += l.ind ;
	}
	
	if (!up && !down && !reload){ strerr_warnw1x("signal must be set") ; exitusage(usage_dbctl) ; }
	
	if (down) 
	{
		signal = "-d" ;
	}
	else signal = "-u" ;

	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC)) strerr_diefu1sys(111,"set revolve pointer to source") ;
	
	if (argc < 1)
	{
		
		if (!ss_resolve_check(sares.s,mainsv)) strerr_dief1sys(111,"inner bundle doesn't exit -- please make a bug report") ;
		if (!ss_resolve_read(&res,sares.s,mainsv)) strerr_diefu1sys(111,"read resolve file of inner bundle") ;
		if (res.ndeps)
		{
			if (!ss_resolve_append(&gares,&res)) strerr_diefu1sys(111,"append services selection with inner bundle") ;
		}
		else
		{
			VERBO1 strerr_warni1x("nothing to do") ;
			ss_resolve_free(&res) ;
			goto freed ;
		}
		
	}
	else 
	{
		for(;*argv;argv++)
		{
			char const *name = *argv ;
			if (!ss_resolve_check(sares.s,name)) strerr_dief2sys(111,"unknown service: ",name) ;
			if (!ss_resolve_read(&res,sares.s,name)) strerr_diefu2sys(111,"read resolve file of: ",name) ;
			if (res.type == CLASSIC) strerr_dief2x(111,name," has type classic") ;
			if (!ss_resolve_append(&gares,&res)) strerr_diefu2sys(111,"append services selection with: ", name) ;
		}
	}
	
	if (!db_ok(info->livetree.s,info->treename.s))
		strerr_dief5sys(111,"db: ",info->livetree.s,"/",info->treename.s," is not running") ;

	if (!stralloc_cats(&tmp,info->livetree.s)) retstralloc(111,"main") ;
	if (!stralloc_cats(&tmp,"/")) retstralloc(111,"main") ;
	if (!stralloc_cats(&tmp,info->treename.s)) retstralloc(111,"main") ;
	if (!stralloc_0(&tmp)) retstralloc(111,"main") ;

	if (reload)
	{
		reverse = 1 ;
		for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,&gares) ; i++)
		{
			if (!ss_resolve_graph_build(&graph,&genalloc_s(ss_resolve_t,&gares)[i],sares.s,reverse)) strerr_diefu1sys(111,"build services graph") ;
		}
		r = ss_resolve_graph_publish(&graph,reverse) ;
		if (r < 0) strerr_dief1x(111,"cyclic dependencies detected") ;
		if (!r) strerr_diefu1sys(111,"publish service graph") ;
		
		rebuild_list(&graph,info,reverse) ;
		 
		pid = send(&graph.sorted,tmp.s,"-d",envp) ;
		
		if (waitpid_nointr(pid,&wstat, 0) < 0)
			strerr_diefu1sys(111,"wait for s6-rc") ;
		
		if (wstat) strerr_diefu1x(111," stop services selection") ;
		
		ret = check_status(&graph.sorted,info,2) ;
		if (ret) goto freed ;
		ss_resolve_graph_free(&graph) ;
	}
	
	if (down) reverse = 1 ;
	else reverse = 0 ;
	
	for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,&gares) ; i++)
	{
		int ireverse = reverse ;
		int logname = get_rstrlen_until(genalloc_s(ss_resolve_t,&gares)[i].sa.s + genalloc_s(ss_resolve_t,&gares)[i].name,SS_LOG_SUFFIX) ;
		if (logname > 0 && (!ss_resolve_cmp(&gares,genalloc_s(ss_resolve_t,&gares)[i].sa.s + genalloc_s(ss_resolve_t,&gares)[i].logassoc)) && down)
			ireverse = 1  ;
		
		if (reload) ireverse = 1 ; 
		if (!ss_resolve_graph_build(&graph,&genalloc_s(ss_resolve_t,&gares)[i],sares.s,ireverse)) strerr_diefu1sys(111,"build services graph") ;
	}
	r = ss_resolve_graph_publish(&graph,reverse) ;
	if (r < 0) strerr_dief1x(111,"cyclic dependencies detected") ;
	if (!r) strerr_diefu1sys(111,"publish service graph") ;

	rebuild_list(&graph,info,reverse) ;
	
	pid = send(&graph.sorted,tmp.s,signal,envp) ;
	
	if (waitpid_nointr(pid,&wstat, 0) < 0)
		strerr_diefu1sys(111,"wait for s6-rc") ;
	
	if (wstat) strerr_diefu2x(111,down ? "start" : "stop"," services selection") ;
	
	ret = check_status(&graph.sorted,info,down ? 2 : 1) ;
	
	freed:
	stralloc_free(&tmp) ;	
	stralloc_free(&sares) ;
	ss_resolve_graph_free(&graph) ;
	ss_resolve_free(&res) ;
	genalloc_deepfree(ss_resolve_t,&gares,ss_resolve_free) ;
	
	return ret ;
}
