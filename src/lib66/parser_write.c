/* 
 * parser_write.c
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

#include <66/parser.h>

#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/error2.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>

#include <skalibs/strerr2.h>
#include <skalibs/types.h>
#include <skalibs/bytestr.h>
#include <skalibs/djbunix.h>
#include <skalibs/diuint32.h>

#include <66/constants.h>
#include <66/enum.h>
#include <66/utils.h>
#include <66/enum.h>
#include <66/resolve.h>
#include <66/ssexec.h>

#include <s6/config.h>//S6_BINPREFIX
#include <execline/config.h>//EXECLINE_BINPREFIX

#include <stdio.h>
/** @Return 0 on fail
 * @Return 1 on success
 * @Return 2 if the service is ignored */
int write_services(ssexec_t *info,sv_alltype *sv, char const *workdir, unsigned int force)
{
	int r ;
	
	size_t workdirlen = strlen(workdir) ;
	char *name = keep.s+sv->cname.name ;
	size_t namelen = strlen(name) ;
	int type = sv->cname.itype ;
	
	{
		ss_resolve_t res = RESOLVE_ZERO ;
		if (ss_resolve_check(workdir,name)) 
		{
			if (!ss_resolve_read(&res,workdir,name)) strerr_diefu2sys(111,"read resolve file of: ",name) ;
			if (res.type != type && res.disen) strerr_dief6x(111,"Detection of incompatible type format for: ",name," -- current: ",get_keybyid(type)," previous: ",get_keybyid(res.type)) ;
		}
		ss_resolve_free(&res) ;
	}
	
	size_t wnamelen ;
	char wname[workdirlen + SS_SVC_LEN + SS_SRC_LEN + namelen + 1 + 1] ;
	memcpy(wname,workdir,workdirlen) ;
	wnamelen = workdirlen ;
	
	if (type == CLASSIC)
	{
		memcpy(wname + wnamelen, SS_SVC, SS_SVC_LEN) ;
		memcpy(wname + wnamelen + SS_SVC_LEN, "/", 1) ;
		memcpy(wname + wnamelen + SS_SVC_LEN + 1, name, namelen) ;
		wnamelen = wnamelen + SS_SVC_LEN + 1 + namelen ;
		wname[wnamelen] = 0 ;
		
	}
	else
	{
		memcpy(wname + wnamelen, SS_DB, SS_DB_LEN) ;
		memcpy(wname + wnamelen + SS_DB_LEN, SS_SRC,SS_SRC_LEN) ;
		memcpy(wname + wnamelen + SS_DB_LEN + SS_SRC_LEN, "/", 1) ;
		memcpy(wname + wnamelen + SS_DB_LEN + SS_SRC_LEN + 1, name, namelen) ;
		wnamelen = wnamelen + SS_DB_LEN + SS_SRC_LEN + 1 + namelen ;
		wname[wnamelen] = 0 ;
	}

	r = scan_mode(wname,S_IFDIR) ;
	if (r < 0)
	{
		VERBO3 strerr_warnw2x("unvalide source: ",wname) ;
		return 0 ;
	}
	if ((r && force) || !r)
	{
		if (rm_rf(wname) < 0)
		{
			VERBO3 strerr_warnwu2sys("remove: ",wname) ;
			return 0 ;
		}
		r = dir_create(wname, 0755) ;
		if (!r)
		{
			VERBO3 strerr_warnwu2sys("create ",wname) ;
			return 0 ;
		}
	}
	else if (r && !force)
	{
		VERBO1 strerr_warnw3x("ignoring: ",name," service: already enabled") ;
		return 2 ;
	}
	
	VERBO2 strerr_warnt3x("Write service ", name," ...") ;
	
	switch(type)
	{
		case CLASSIC:
			if (!write_classic(sv, wname, force))
			{
				VERBO3 strerr_warnwu2x("write: ",wname) ;
				return 0 ;
			}
			break ;
		case LONGRUN:
			if (!write_longrun(sv, wname, force))
			{
				VERBO3 strerr_warnwu2x("write: ",wname) ;
				return 0 ;
			}
			
			break ;
		case ONESHOT:
			if (!write_oneshot(sv, wname, force))
			{
				VERBO3 strerr_warnwu2x("write: ",wname) ;
				return 0 ;
			}
			
			break ;
		case BUNDLE:
			if (!write_bundle(sv, wname, force))
			{
				VERBO3 strerr_warnwu2x("write: ",wname) ;
				return 0 ;
			}
			
			break ;
		default: 
			VERBO3 strerr_warni2x("unkown type: ", get_keybyid(sv->cname.itype)) ;
			return 0 ;
	}
		
	return 1 ;
}

int write_classic(sv_alltype *sv, char const *dst, unsigned int force)
{	
	/**notification,timeout, ...*/
	if (!write_common(sv, dst))
	{
		VERBO3 strerr_warnwu1x("write common files") ;
		return 0 ;
	}
	/** run file*/
	if (!write_exec(sv, &sv->type.classic_longrun.run,"run",dst,0755))
	{
		VERBO3 strerr_warnwu3x("write: ",dst,"/run") ;
		return 0 ;
	}
	/** finish file*/
	if (sv->type.classic_longrun.finish.exec) 
	{	
		if (!write_exec(sv, &sv->type.classic_longrun.finish,"finish",dst,0755))
		{
			VERBO3 strerr_warnwu3x("write: ",dst,"/finish") ;
			return 0 ;
		}
	}
	/**logger */
	if (sv->opts[0])
	{
		if (!write_logger(sv, &sv->type.classic_longrun.log,"log",dst,0755, force))
		{
			VERBO3 strerr_warnwu3x("write: ",dst,"/log") ;
			return 0 ;
		}
	}


	return 1 ;
}

int write_longrun(sv_alltype *sv,char const *dst, unsigned force)
{	
	size_t r ;
	char *name = keep.s+sv->cname.name ;
	size_t namelen = strlen(name) ;
	size_t dstlen = strlen(dst) ;
	char logname[namelen + SS_LOG_SUFFIX_LEN + 1] ;
	char dstlog[dstlen + 1] ;
	
	/**notification,timeout ...*/
	if (!write_common(sv, dst))
	{
		VERBO3 strerr_warnwu1x("write common files") ;
		return 0 ;
	}
	/**run file*/
	if (!write_exec(sv, &sv->type.classic_longrun.run,"run",dst,0644))
	{
		VERBO3 strerr_warnwu3x("write: ",dst,"/run") ;
		return 0 ;
	}
	/**finish file*/
	if (sv->type.classic_longrun.finish.exec) 
	{
		
		if (!write_exec(sv, &sv->type.classic_longrun.finish,"finish",dst,0644))
		{
			VERBO3 strerr_warnwu3x("write: ",dst,"/finish") ;
			return 0 ;
		}
	}
	
	/**logger*/
	if (sv->opts[0])
	{
		memcpy(logname,name,namelen) ;
		memcpy(logname + namelen,SS_LOG_SUFFIX,SS_LOG_SUFFIX_LEN) ;
		logname[namelen + SS_LOG_SUFFIX_LEN] = 0 ;
	
		r = get_rstrlen_until(dst,name) ;
		r--;//remove the last slash
		memcpy(dstlog,dst,r) ;
		dstlog[r] = 0 ;
		
		if (!write_logger(sv, &sv->type.classic_longrun.log,logname,dstlog,0644,force)) 
		{
			VERBO3 strerr_warnwu4x("write: ",dstlog,"/",logname) ;
			return 0 ;
		}
		if (!write_consprod(sv,name,logname,dst,dstlog))
		{
			VERBO3 strerr_warnwu1x("write consumer/producer files") ;
			return 0 ;
		}
			
	}
	/** dependencies */
	if (!write_dependencies(sv->cname.nga,sv->cname.idga, dst, "dependencies", &gadeps,force))
	{
		VERBO3 strerr_warnwu3x("write: ",dst,"/dependencies") ;
		return 0 ;
	}
	
	
	return 1 ;
}

int write_oneshot(sv_alltype *sv,char const *dst, unsigned int force)
{
	
	if (!write_common(sv, dst))
	{
		VERBO3 strerr_warnwu1x("write common files") ;
		return 0 ;
	}
	/** up file*/
	if (!write_exec(sv, &sv->type.oneshot.up,"up",dst,0644))
	{
		VERBO3 strerr_warnwu3x("write: ",dst,"/up") ;
		return 0 ;
	}
	/** down file*/
	if (sv->type.oneshot.down.exec) 
	{	
		if (!write_exec(sv, &sv->type.oneshot.down,"down",dst,0644))
		{
			VERBO3 strerr_warnwu3x("write: ",dst,"/down") ;
			return 0 ;
		}
	}
	
	if (!write_dependencies(sv->cname.nga,sv->cname.idga, dst, "dependencies", &gadeps,force))
	{
		VERBO3 strerr_warnwu3x("write: ",dst,"/dependencies") ;
		return 0 ;
	}
		
	return 1 ;
}

int write_bundle(sv_alltype *sv, char const *dst, unsigned int force)
{
	/** type file*/
	if (!file_write_unsafe(dst,"type","bundle",6))
	{
		VERBO3 strerr_warnwu3x("write: ",dst,"/type") ;
		return 0 ;
	}
	/** contents file*/
	if (!write_dependencies(sv->cname.nga,sv->cname.idga, dst, "contents", &gadeps, force))
	{
		VERBO3 strerr_warnwu3x("write: ",dst,"/contents") ;
		return 0 ;
	}
		
	return 1 ;
}

int write_logger(sv_alltype *sv, sv_execlog *log,char const *name, char const *dst, int mode, unsigned int force)
{
	int r ;
	int logbuild = log->run.build ;
	
	char *time = NULL ;
	char *pmax = NULL ;
	char *pback = NULL ;
	char *timestamp = "t" ;
	char max[UINT32_FMT] ;
	char back[UINT32_FMT] ;
	char const *userhome ;
	char *svname = keep.s + sv->cname.name ;
		
	stralloc ddst = STRALLOC_ZERO ;
	stralloc shebang = STRALLOC_ZERO ;
	stralloc ui = STRALLOC_ZERO ;
	stralloc exec = STRALLOC_ZERO ;
	stralloc destlog = STRALLOC_ZERO ;
		
	if(!stralloc_cats(&ddst,dst)) retstralloc(0,"write_logger") ;
	if(!stralloc_cats(&ddst,"/")) retstralloc(0,"write_logger") ;
	if(!stralloc_cats(&ddst,name)) retstralloc(0,"write_logger") ;
	if(!stralloc_0(&ddst)) retstralloc(0,"write_logger") ;

	r = scan_mode(ddst.s,S_IFDIR) ;
	if (r && force)
	{
		if (rm_rf(ddst.s) < 0)
		{
			VERBO3 strerr_warnwu2sys("remove: ",ddst.s) ;
			return 0 ;
		}
		r = dir_create(ddst.s, 0755) ;
		if (!r)
		{
			VERBO3 strerr_warnwu3sys("create ",ddst.s," directory") ;
			return 0 ;
		}
	}
	else if (r)
	{
		VERBO3 strerr_warnw3x("ignoring ",name,": already enabled") ;
		return 0 ;
	}
	else
	{
		r = dir_create(ddst.s, 0755) ;
		if (!r)
		{
			VERBO3 strerr_warnwu3sys("create ",ddst.s," directory") ;
			return 0 ;
		}
	}
	
	userhome = get_userhome(MYUID) ;

	/**timeout family*/
	for (uint32_t i = 0; i < 2;i++)
	{
		if (log->timeout[i][0])
		{
			
			if (!i)
				time = "timeout-kill" ;
			if (i)
				time = "timeout-finish" ;
			if (!write_uint(ddst.s,time,log->timeout[i][0])) return 0 ;	
			
		}
		
	}
	/** dependencies*/
	if (log->nga)
	{
		if (!write_dependencies(log->nga,log->idga,ddst.s,"dependencies",&gadeps,force))
		{
			VERBO3 strerr_warnwu3x("write: ",ddst.s,"/dependencies") ;
			return 0 ;
		}
	}
	
	if (sv->cname.itype > CLASSIC)
	{
		if (!file_write_unsafe(ddst.s,"type","longrun",7))
		{
			VERBO3 strerr_warnwu3sys("write: ",ddst.s,"/type") ;
			return 0 ;
		}
	}
	
	/**logger section may not be set
	 * pick auto by default*/
	if (!logbuild)
		logbuild=AUTO ; 
	
	switch(logbuild)
	{
		case AUTO:
			/** uid */
			if (!stralloc_cats(&shebang, "#!" EXECLINE_SHEBANGPREFIX "execlineb -P\n")) retstralloc(0,"write_logger") ;
			if (!stralloc_0(&shebang)) retstralloc(0,"write_logger") ;
			if ((!MYUID && log->run.runas))
			{
				if (!stralloc_cats(&ui,S6_BINPREFIX "s6-setuidgid ")) retstralloc(0,"write_logger") ;
				if (!get_namebyuid(log->run.runas,&ui))
				{
					VERBO3 strerr_warnwu1sys("set owner for the logger") ;
					return 0 ;
				}
			}
			if (!stralloc_cats(&ui,"\n")) retstralloc(0,"write_logger") ;
			if (!stralloc_0(&ui)) retstralloc(0,"write_logger") ;
			/** destination */		
			if (!log->destination)
			{	
				if(MYUID > 0)
				{	
				
					if (!stralloc_cats(&destlog,userhome)) retstralloc(0,"write_logger") ;
					if (!stralloc_cats(&destlog,"/")) retstralloc(0,"write_logger") ;
					if (!stralloc_cats(&destlog,SS_LOGGER_USERDIR)) retstralloc(0,"write_logger") ;
					if (!stralloc_cats(&destlog,svname)) retstralloc(0,"write_logger") ;
				}
				else
				{
					if (!stralloc_cats(&destlog,SS_LOGGER_SYSDIR)) retstralloc(0,"write_logger") ;
					if (!stralloc_cats(&destlog,svname)) retstralloc(0,"write_logger") ;
				}
			}
			else
			{
				if (!stralloc_cats(&destlog,keep.s+log->destination)) retstralloc(0,"write_logger") ;
			}
			if (!stralloc_0(&destlog)) retstralloc(0,"write_logger") ;

			if (log->timestamp == ISO) timestamp = "T" ;
			else if (log->timestamp == NONE) timestamp = "" ;
			
			if (log->backup > 0)
			{
				back[uint32_fmt(back,log->backup)] = 0 ;
				pback = back ;
			}
			else
				pback = "3" ;
			
			if (log->maxsize > 0)
			{
				max[uint32_fmt(max,log->maxsize)] = 0 ;
				pmax = max ;
			}
			else
				pmax = "1000000" ;
			
			if (!stralloc_cats(&exec,shebang.s)) retstralloc(0,"write_logger") ;
			if (!stralloc_cats(&exec,EXECLINE_BINPREFIX "fdmove -c 2 1\n")) retstralloc(0,"write_logger") ;
			if (!stralloc_cats(&exec,ui.s)) retstralloc(0,"write_logger") ;
			if (!stralloc_cats(&exec,S6_BINPREFIX "s6-log " "n")) retstralloc(0,"write_logger") ;
			if (!stralloc_cats(&exec,pback)) retstralloc(0,"write_logger") ;
			if (!stralloc_cats(&exec," ")) retstralloc(0,"write_logger") ;
			if (log->timestamp < NONE) 
			{
				if (!stralloc_cats(&exec,timestamp)) retstralloc(0,"write_logger") ;
				if (!stralloc_cats(&exec," ")) retstralloc(0,"write_logger") ;
			}
			if (!stralloc_cats(&exec,"s")) retstralloc(0,"write_logger") ;
			if (!stralloc_cats(&exec,pmax)) retstralloc(0,"write_logger") ;
			if (!stralloc_cats(&exec," ")) retstralloc(0,"write_logger") ;
			if (!stralloc_cats(&exec,destlog.s)) retstralloc(0,"write_logger") ;
			if (!stralloc_cats(&exec,"\n")) retstralloc(0,"write_logger") ;
			
			/**write it*/
			if (!file_write_unsafe(ddst.s,"run",exec.s,exec.len))
			{
				VERBO3 strerr_warnwu3sys("write: ",ddst.s,"/run") ;
				return 0 ;
			}
			
			if (sv->cname.itype == CLASSIC)
			{
				ddst.len-- ;
				if (!stralloc_cats(&ddst,"/run")) retstralloc(0,"write_logger") ;
				if (!stralloc_0(&ddst)) retstralloc(0,"write_logger") ;
				
				if (chmod(ddst.s, mode) < 0)
				{
					VERBO3 strerr_warnwu2sys("chmod ", ddst.s) ;
					return 0 ;
				}
			}
			break;
		case CUSTOM:
			if (!write_exec(sv, &log->run,"run",ddst.s,mode))
			{ 
				VERBO3 strerr_warnwu3x("write: ",ddst.s,"/run") ;
				return 0 ;
			}
			break;
		default:
			VERBO3 strerr_warnw2x("unknown build value: ",get_keybyid(logbuild)) ;
			return 0 ;
	
	}
	size_t destlen = get_rlen_until(destlog.s,'/',destlog.len) ;
	destlog.len = destlen ;
	if (!stralloc_0(&destlog)) retstralloc(0,"write_logger") ;
	
	r = dir_search(destlog.s,svname,S_IFDIR) ;
	if (r < 0)
	{
		VERBO3 strerr_warnw4x(destlog.s,"/",svname," already exist with different mode") ;
		return 0 ;
	}
	if (!r)
	{
		r = dir_create_under(destlog.s,svname,0755) ;
		if (r < 0)
		{
			VERBO3 strerr_warnwu5sys("create ",destlog.s,"/",svname," directory") ;
			return 0 ;
		}
	}

	stralloc_free(&shebang) ;
	stralloc_free(&ui) ;
	stralloc_free(&exec) ;
	stralloc_free(&destlog) ;
	stralloc_free(&ddst) ;
		
	return 1 ;
}

int write_consprod(sv_alltype *sv,char const *prodname,char const *consname,char const *proddst,char const *consdst)
{
	size_t consdstlen = strlen(consdst) ;
	size_t consnamelen = strlen(consname) ;
	size_t proddstlen = strlen(proddst) ;
	
	char consfile[consdstlen + 1 + consnamelen + 1] ;
	memcpy(consfile,consdst,consdstlen) ; 
	consfile[consdstlen] = '/' ; 
	memcpy(consfile + consdstlen + 1, consname,consnamelen) ;
	consfile[consdstlen + 1 + consnamelen] = 0 ; 
	
	char prodfile[proddstlen + 1] ;
	memcpy(prodfile,proddst,proddstlen) ;
	prodfile[proddstlen] = 0 ;
	
	char pipefile[consdstlen + consnamelen + 1 + 1] ;
	
	/**producer-for*/
	if (!file_write_unsafe(consfile,get_keybyid(CONSUMER),prodname,strlen(prodname))) 
	{
		VERBO3 strerr_warnwu3x("write: ",consfile,get_keybyid(CONSUMER)) ;
		return 0 ;
	}
	/**consumer-for*/
	if (!file_write_unsafe(prodfile,get_keybyid(PRODUCER),consname,strlen(consname)))
	{
		VERBO3 strerr_warnwu3x("write: ",prodfile,get_keybyid(PRODUCER)) ;
		return 0 ;
	}
	/**pipeline**/
	if (sv->opts[1]) 
	{
		size_t len = strlen(deps.s+sv->pipeline) ;
		char pipename[len + 1] ;
		memcpy(pipefile,consdst,consdstlen) ;
		memcpy(pipefile + consdstlen, consname,consnamelen) ;
		memcpy(pipefile + consdstlen + consnamelen, "/", 1) ;
		pipefile[consdstlen + consnamelen + 1] = 0  ;
		memcpy(pipename,deps.s+sv->pipeline,len) ;
		pipename[len] = 0 ;
		if (!file_write_unsafe(pipefile,PIPELINE_NAME,pipename,len))
		{
			VERBO3 strerr_warnwu3x("write: ",pipefile,PIPELINE_NAME) ;
			return 0 ;
		}
	}	
	
	return 1 ;
}

int write_common(sv_alltype *sv, char const *dst)
{
	char *time = NULL ;
	
	/**down file*/
	if (sv->flags[0])
	{
		if (!file_create_empty(dst,"down",0644))
		{
			VERBO3 strerr_warnwu1sys("create down file") ;
			return 0 ;
		}
	}
	/**nosetsid file*/
	if (sv->flags[1])
	{
		if (!file_create_empty(dst,"nosetsid",0644))
		{
			VERBO3 strerr_warnwu1sys("create nosetsid file") ;
			return 0 ;
		}
	}
	
	/**notification-fd*/
	if (sv->notification)
	{
		if (!write_uint(dst,"notification-fd", sv->notification))
		{
			VERBO3 strerr_warnwu1x("write notification file") ;
			return 0 ;
		}
	}
	/**timeout family*/
	for (uint32_t i = 0; i < 4;i++)
	{
		if (sv->timeout[i][0])
		{
			
			if (!i)
				time = "timeout-kill" ;
			if (i)
				time = "timeout-finish" ;
			if (i > 1)
				time = "timeout-up" ;
			if (i > 2)
				time = "timeout-down" ;

			if (!write_uint(dst, time, sv->timeout[i][0])) 	
			{
				VERBO3 strerr_warnwu2x("write file: ",time) ;
				return 0 ;
			}
		}
		
	}
	/** type file*/
	if (sv->cname.itype > CLASSIC)
	{
		if (!file_write_unsafe(dst,"type",get_keybyid(sv->cname.itype),strlen(get_keybyid(sv->cname.itype))))
		{
			VERBO3 strerr_warnwu1sys("write type file") ;
			return 0 ;
		}
	}
	/** max-death-tally */
	if (sv->death)
	{
		if (!write_uint(dst, "max-death-tally", sv->death))
		{
			VERBO3 strerr_warnwu1x("write max-death-tally file") ;
			return 0 ;
		}
	}
	/**down-signal*/
	if (sv->signal)
	{
		if (!write_uint(dst,"down-signal", sv->signal))
		{
			VERBO3 strerr_warnwu1x("write down-signal file") ;
			return 0 ;
		}
	}
	/** environment */
	if (sv->opts[2])
	{
		char *dst = 0 ;
		uid_t owner = MYUID ;
		stralloc home = STRALLOC_ZERO ;
		if (!owner) dst = SS_SERVICE_SYSCONFDIR ;
		else
		{
			if (!set_ownerhome(&home,owner))
			{ VERBO3 strerr_warnwu1sys("set home directory") ; return 0 ; }
			if (!stralloc_cats(&home,SS_SERVICE_USERCONFDIR)) retstralloc(111,"write_common") ;
			if (!stralloc_0(&home)) retstralloc(111,"write_common") ;
			home.len-- ;
			dst = home.s ;
		}
			
		char *name = keep.s + sv->cname.name ;
				
		//if (!write_env(name,&sv->env,&saenv,dst))
		if (!write_env(name,&saenv,dst))
		{
			VERBO3 strerr_warnwu1x("write environment") ;
			return 0 ;
		}
		stralloc_free(&home) ;
	}
	
	
	return 1 ;
}


int write_exec(sv_alltype *sv, sv_exec *exec,char const *file,char const *dst,int mode)
{
	
	unsigned int type = sv->cname.itype ;
	char *name = keep.s+sv->cname.name ;
	uid_t owner = MYUID ;
	size_t filelen = strlen(file) ;
	size_t dstlen = strlen(dst) ;
	char write[dstlen + 1 + filelen + 1] ;
	
	stralloc home = STRALLOC_ZERO ;
	stralloc shebang = STRALLOC_ZERO ;
	stralloc ui = STRALLOC_ZERO ;
	stralloc env = STRALLOC_ZERO ;
	stralloc runuser = STRALLOC_ZERO ;
	stralloc execute = STRALLOC_ZERO ;
	
	
	char *envdata = 0 ;
	if (!owner) envdata = SS_SERVICE_SYSCONFDIR ;
	else
	{
		if (!set_ownerhome(&home,owner))
		{ VERBO3 strerr_warnwu1sys("set home directory") ; return 0 ; }
		if (!stralloc_cats(&home,SS_SERVICE_USERCONFDIR)) retstralloc(111,"write_exec") ;
		if (!stralloc_0(&home)) retstralloc(111,"write_exec") ;
		home.len-- ;
		envdata = home.s ;
	}	
	
	switch (exec->build)
	{
		case AUTO:
			/** uid */
			if ((!owner && exec->runas))
			{
				if (!stralloc_cats(&ui,S6_BINPREFIX "s6-setuidgid ")) retstralloc(0,"write_exec") ;
				if (!get_namebyuid(exec->runas,&ui))
				{
					VERBO3 strerr_warnwu1sys("set owner for the execute file") ;
					return 0 ;
				}
				if (!stralloc_cats(&ui,"\n")) retstralloc(0,"write_exec") ;
			}
			/** environment */
			if (sv->opts[2] && (exec->build == AUTO))
			{
				if (!stralloc_cats(&env,S6_BINPREFIX "66-envfile -f ")) retstralloc(0,"write_exec") ;
				if (!stralloc_cats(&env,name)) retstralloc(0,"write_exec") ;
				if (!stralloc_cats(&env," ")) retstralloc(0,"write_exec") ;
				if (!stralloc_cats(&env,envdata)) retstralloc(0,"write_exec") ;
				if (!stralloc_cats(&env,"\n")) retstralloc(0,"write_exec") ;
			}
			/** shebang */
			if (type != ONESHOT)
			{
				if (!stralloc_cats(&shebang, "#!" EXECLINE_SHEBANGPREFIX "execlineb -P\n")) retstralloc(0,"write_exec") ;
			}
			break ;
		case CUSTOM:
			if (type != ONESHOT)
			{
				if (!stralloc_cats(&shebang, "#!")) retstralloc(0,"write_exec") ;
				if (!stralloc_cats(&shebang, keep.s+exec->shebang)) retstralloc(0,"write_exec") ;
				if (!stralloc_cats(&shebang,"\n")) retstralloc(0,"write_exec") ;
			}
			else
			{
				if (!stralloc_cats(&shebang, keep.s+exec->shebang)) retstralloc(0,"write_exec") ;			
				if (!stralloc_cats(&shebang," \"")) retstralloc(0,"write_exec") ;
			}
			break ;
		default:
			VERBO3 strerr_warnw3x("unknown ", get_keybyid(exec->build)," build type") ;
			break ;
	}
	/** close uid */
	if (!stralloc_0(&ui)) retstralloc(0,"write_exec") ;
	/** close env*/
	if (!stralloc_0(&env)) retstralloc(0,"write_exec") ;
	/** close shebang */
	if (!stralloc_0(&shebang)) retstralloc(0,"write_exec") ;
	/** close command */
	if (!stralloc_cats(&runuser, keep.s+exec->exec)) retstralloc(0,"write_exec") ;
	if ((type == ONESHOT) && (exec->build == CUSTOM))
	{
		if (!stralloc_cats(&runuser," \"")) retstralloc(0,"write_exec") ;
	}
	if (!stralloc_cats(&runuser,"\n")) retstralloc(0,"write_exec") ;
	if (!stralloc_0(&runuser)) retstralloc(0,"write_exec") ;
	
	/** build the file*/	
	if (!stralloc_cats(&execute,shebang.s)) retstralloc(0,"write_exec") ;
	if (exec->build == AUTO)
	{
		if (!stralloc_cats(&execute,EXECLINE_BINPREFIX "fdmove -c 2 1\n")) retstralloc(0,"write_exec") ;
	}
	if (!stralloc_cats(&execute,env.s)) retstralloc(0,"write_exec") ;
	if (!stralloc_cats(&execute,ui.s)) retstralloc(0,"write_exec") ;
	if (!stralloc_cats(&execute,runuser.s)) retstralloc(0,"write_exec") ;

	memcpy(write,dst,dstlen) ;
	write[dstlen] = '/' ;
	memcpy(write + dstlen + 1, file, filelen) ;
	write[dstlen + 1 + filelen] = 0 ;
	
	if (!file_write_unsafe(dst,file,execute.s,execute.len))
	{
		VERBO3 strerr_warnwu4sys("write: ",dst,"/",file) ;
		return 0 ;
	}
	
	if (chmod(write, mode) < 0)
	{
		VERBO3 strerr_warnwu2sys("chmod ", write) ;
		return 0 ;
	}
	
	stralloc_free(&home) ;
	stralloc_free(&shebang) ;
	stralloc_free(&ui) ;
	stralloc_free(&execute) ;
	stralloc_free(&env) ;
	stralloc_free(&runuser) ;
	stralloc_free(&execute) ;
	return 1 ;	
}

int write_dependencies(unsigned int nga,unsigned int idga,char const *dst,char const *filename, genalloc *ga, unsigned int force)
{
	int r ;
		
	stralloc contents = STRALLOC_ZERO ;
	stralloc namedeps = STRALLOC_ZERO ;
	
	for (unsigned int i = 0; i < nga; i++)
	{
		if (!stralloc_obreplace(&namedeps,deps.s+genalloc_s(unsigned int,ga)[idga+i])) return 0 ;
		r = insta_check(namedeps.s) ;
		if (!r) 
		{
			VERBO3 strerr_warnw2x(" invalid instance name: ",namedeps.s) ;
			return 0 ;
		}
		if (r > 0)
		{
			if (!insta_splitname(&namedeps,namedeps.s,r,1))
			{
				VERBO3 strerr_warnwu2x("split copy name of instance: ",namedeps.s) ;
				return 0 ;
			}
		}
		if (!stralloc_cats(&contents,namedeps.s)) retstralloc(0,"write_dependencies") ;
		if (!stralloc_cats(&contents,"\n")) retstralloc(0,"write_dependencies") ;
	}
		
	if (contents.len)
	{
		if (!file_write_unsafe(dst,filename,contents.s,contents.len))
		{
			VERBO3 strerr_warnwu3sys("create file: ",dst,filename) ;
			goto err ;
		}
	}
	
	stralloc_free(&contents) ;
	stralloc_free(&namedeps) ;
	return 1 ;
	err:
		stralloc_free(&contents) ;
		stralloc_free(&namedeps) ;
		return 0 ;
}

int write_uint(char const *dst, char const *name, uint32_t ui)
{
	char number[UINT32_FMT] ; 
	
	if (!file_write_unsafe(dst,name,number,uint32_fmt(number,ui)))
	{
		VERBO3 strerr_warnwu4sys("write: ",dst,"/",name) ;
		return 0 ;
	}

	return 1 ;
}

int write_env(char const *name, stralloc *sa,char const *dst)
{
	int r ;
			
	r = scan_mode(dst,S_IFDIR) ;
	if (r < 0)
	{
		VERBO3 strerr_warnw2sys(" invalid environment directory: ",dst) ;
		return 0 ;
	}
	else if (!r)
	{
		VERBO3 strerr_warnw2sys(dst," service environment directory doesn't exist") ;
		return 0 ;
	}
	if (!file_write_unsafe(dst,name,sa->s,sa->len))
	{
		VERBO3 strerr_warnwu4sys("create file: ",dst,"/",name) ;
		return 0 ;
	}
		
	return 1 ;
}

