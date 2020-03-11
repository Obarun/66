/* 
 * parser_write.c
 * 
 * Copyright (c) 2018-2020 Eric Vidal <eric@obarun.org>
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
#include <stdint.h>
//#include <stdio.h>

#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>

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
#include <66/environ.h>

#include <s6/config.h>//S6_BINPREFIX
#include <execline/config.h>//EXECLINE_BINPREFIX


/** @Return 0 on fail
 * @Return 1 on success
 * @Return 2 if the service is ignored */
int write_services(ssexec_t *info,sv_alltype *sv, char const *workdir, uint8_t force, uint8_t conf)
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
			if (!ss_resolve_read(&res,workdir,name)) log_dieusys(LOG_EXIT_SYS,"read resolve file of: ",name) ;
			if (res.type != type && res.disen) log_die(LOG_EXIT_SYS,"Detection of incompatible type format for: ",name," -- current: ",get_keybyid(type)," previous: ",get_keybyid(res.type)) ;
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
		log_warn_return(LOG_EXIT_ZERO,"unvalide source: ",wname) ;

	if ((r && force) || !r)
	{
		if (rm_rf(wname) < 0)
			log_warnusys_return(LOG_EXIT_ZERO,"remove: ",wname) ;
		r = dir_create(wname, 0755) ;
		if (!r)
			log_warnusys_return(LOG_EXIT_ZERO,"create ",wname) ;
	}
	else if (r && !force)
	{
		log_info("Ignoring: ",name," service: already enabled") ;
		return 2 ;
	}
	
	log_trace("Write service ", name," ...") ;
	
	switch(type)
	{
		case CLASSIC:
			if (!write_classic(sv, wname, force, conf))
				log_warnu_return(LOG_EXIT_ZERO,"write: ",wname) ;
			
			break ;
		case LONGRUN:
			if (!write_longrun(sv, wname, force, conf))
				log_warnu_return(LOG_EXIT_ZERO,"write: ",wname) ;

			break ;
		case ONESHOT:
			if (!write_oneshot(sv, wname, conf))
				log_warnu_return(LOG_EXIT_ZERO,"write: ",wname) ;

			break ;
		case BUNDLE:
			if (!write_bundle(sv, wname))
				log_warnu_return(LOG_EXIT_ZERO,"write: ",wname) ;

			break ;
		default: log_warn_return(LOG_EXIT_ZERO,"unkown type: ", get_keybyid(sv->cname.itype)) ;
	}
		
	return 1 ;
}

int write_classic(sv_alltype *sv, char const *dst, uint8_t force,uint8_t conf)
{	
	/**notification,timeout, ...*/
	if (!write_common(sv, dst, conf))
		log_warnu_return(LOG_EXIT_ZERO,"write common files") ;

	/** run file*/
	if (!write_exec(sv, &sv->type.classic_longrun.run,"run",dst,0755))
		log_warnu_return(LOG_EXIT_ZERO,"write: ",dst,"/run") ;

	/** finish file*/
	if (sv->type.classic_longrun.finish.exec) 
	{	
		if (!write_exec(sv, &sv->type.classic_longrun.finish,"finish",dst,0755))
			log_warnu_return(LOG_EXIT_ZERO,"write: ",dst,"/finish") ;
	}
	/**logger */
	if (sv->opts[0])
	{
		if (!write_logger(sv, &sv->type.classic_longrun.log,"log",dst,0755, force))
			log_warnu_return(LOG_EXIT_ZERO,"write: ",dst,"/log") ;
	}

	return 1 ;
}

int write_longrun(sv_alltype *sv,char const *dst, uint8_t force, uint8_t conf)
{	
	size_t r ;
	char *name = keep.s+sv->cname.name ;
	size_t namelen = strlen(name) ;
	size_t dstlen = strlen(dst) ;
	char logname[namelen + SS_LOG_SUFFIX_LEN + 1] ;
	char dstlog[dstlen + 1] ;
	
	/**notification,timeout ...*/
	if (!write_common(sv, dst,conf))
		log_warnu_return(LOG_EXIT_ZERO,"write common files") ;

	/**run file*/
	if (!write_exec(sv, &sv->type.classic_longrun.run,"run",dst,0644))
		log_warnu_return(LOG_EXIT_ZERO,"write: ",dst,"/run") ;

	/**finish file*/
	if (sv->type.classic_longrun.finish.exec) 
	{
		
		if (!write_exec(sv, &sv->type.classic_longrun.finish,"finish",dst,0644))
			log_warnu_return(LOG_EXIT_ZERO,"write: ",dst,"/finish") ;
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
			log_warnu_return(LOG_EXIT_ZERO,"write: ",dstlog,"/",logname) ;

		if (!write_consprod(sv,name,logname,dst,dstlog))
			log_warnu_return(LOG_EXIT_ZERO,"write consumer/producer files") ;
	}
	/** dependencies */
	if (!write_dependencies(sv->cname.nga,sv->cname.idga, dst, "dependencies"))
		log_warnu_return(LOG_EXIT_ZERO,"write: ",dst,"/dependencies") ;

	return 1 ;
}

int write_oneshot(sv_alltype *sv,char const *dst,uint8_t conf)
{
	
	if (!write_common(sv, dst,conf))
		log_warnu_return(LOG_EXIT_ZERO,"write common files") ;

	/** up file*/
	if (!write_exec(sv, &sv->type.oneshot.up,"up",dst,0644))
		log_warnu_return(LOG_EXIT_ZERO,"write: ",dst,"/up") ;

	/** down file*/
	if (sv->type.oneshot.down.exec) 
	{	
		if (!write_exec(sv, &sv->type.oneshot.down,"down",dst,0644))
			log_warnu_return(LOG_EXIT_ZERO,"write: ",dst,"/down") ;
	}
	
	if (!write_dependencies(sv->cname.nga,sv->cname.idga, dst, "dependencies"))
		log_warnu_return(LOG_EXIT_ZERO,"write: ",dst,"/dependencies") ;
		
	return 1 ;
}

int write_bundle(sv_alltype *sv, char const *dst)
{
	/** type file*/
	if (!file_write_unsafe(dst,"type","bundle",6))
		log_warnu_return(LOG_EXIT_ZERO,"write: ",dst,"/type") ;

	/** contents file*/
	if (!write_dependencies(sv->cname.nga,sv->cname.idga, dst, "contents"))
		log_warnu_return(LOG_EXIT_ZERO,"write: ",dst,"/contents") ;
		
	return 1 ;
}

int write_logger(sv_alltype *sv, sv_execlog *log,char const *name, char const *dst, mode_t mode, uint8_t force)
{
	int r ;
	int logbuild = log->run.build ;
	
	uid_t log_uid ;
	gid_t log_gid ;
	char *time = 0 ;
	char *pmax = 0 ;
	char *pback = 0 ;
	char *timestamp = "t" ;
	char *logrunner = log->run.runas ? keep.s + log->run.runas : SS_LOGGER_RUNNER ;
	char max[UINT32_FMT] ;
	char back[UINT32_FMT] ;
	char const *userhome ;
	char *svname = keep.s + sv->cname.name ;
		
	stralloc ddst = STRALLOC_ZERO ;
	stralloc shebang = STRALLOC_ZERO ;
	stralloc ui = STRALLOC_ZERO ;
	stralloc exec = STRALLOC_ZERO ;
	stralloc destlog = STRALLOC_ZERO ;
		
	if(!stralloc_cats(&ddst,dst) ||
	!stralloc_cats(&ddst,"/") || 
	!stralloc_cats(&ddst,name) || 
	!stralloc_0(&ddst)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;

	r = scan_mode(ddst.s,S_IFDIR) ;
	if (r && force)
	{
		if (rm_rf(ddst.s) < 0)
			log_warnusys_return(LOG_EXIT_ZERO,"remove: ",ddst.s) ;
	
		r = dir_create(ddst.s, 0755) ;
		if (!r)
			log_warnusys_return(LOG_EXIT_ZERO,"create ",ddst.s," directory") ;
	}
	else if (r)
	{
		log_warnu_return(LOG_EXIT_ZERO,"ignoring ",name,": already enabled") ;
	}
	else
	{
		r = dir_create(ddst.s, 0755) ;
		if (!r)
			log_warnusys_return(LOG_EXIT_ZERO,"create ",ddst.s," directory") ;
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
		if (!write_dependencies(log->nga,log->idga,ddst.s,"dependencies"))
			log_warnu_return(LOG_EXIT_ZERO,"write: ",ddst.s,"/dependencies") ;
	}
	
	if (sv->cname.itype > CLASSIC)
	{
		if (!file_write_unsafe(ddst.s,"type","longrun",7))
			log_warnusys_return(LOG_EXIT_ZERO,"write: ",ddst.s,"/type") ;
	}
	
	/**logger section may not be set
	 * pick auto by default*/
	if (!logbuild)
		logbuild=AUTO ; 
	
	switch(logbuild)
	{
		case AUTO:
			/** uid */
			if (!stralloc_cats(&shebang, "#!" EXECLINE_SHEBANGPREFIX "execlineb -P\n") || 
			!stralloc_0(&shebang)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
			if ((!MYUID))
			{
				if (!stralloc_cats(&ui,S6_BINPREFIX "s6-setuidgid ") ||
				!stralloc_cats(&ui,logrunner)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
			}
			if (!stralloc_cats(&ui,"\n") ||
			!stralloc_0(&ui)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
			/** destination */		
			if (!log->destination)
			{	
				if(MYUID > 0)
				{	
				
					if (!stralloc_cats(&destlog,userhome) ||
					!stralloc_cats(&destlog,"/") ||
					!stralloc_cats(&destlog,SS_LOGGER_USERDIR) ||
					!stralloc_cats(&destlog,svname)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
				}
				else
				{
					if (!stralloc_cats(&destlog,SS_LOGGER_SYSDIR) ||
					!stralloc_cats(&destlog,svname)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
				}
			}
			else
			{
				if (!stralloc_cats(&destlog,keep.s+log->destination)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
			}
			if (!stralloc_0(&destlog)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
			
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
			
			if (!stralloc_cats(&exec,shebang.s) || 
			!stralloc_cats(&exec,EXECLINE_BINPREFIX "fdmove -c 2 1\n") ||
			!stralloc_cats(&exec,ui.s) ||
			!stralloc_cats(&exec,S6_BINPREFIX "s6-log -d3 " "n") ||
			!stralloc_cats(&exec,pback) ||
			!stralloc_cats(&exec," ")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
			if (log->timestamp < NONE) 
			{
				if (!stralloc_cats(&exec,timestamp) ||
				!stralloc_cats(&exec," ")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
			}
			if (!stralloc_cats(&exec,"s") ||
			!stralloc_cats(&exec,pmax) || 
			!stralloc_cats(&exec," ") ||
			!stralloc_cats(&exec,destlog.s) ||
			!stralloc_cats(&exec,"\n")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
			
			/**write it*/
			if (!file_write_unsafe(ddst.s,"run",exec.s,exec.len))
				log_warnusys_return(LOG_EXIT_ZERO,"write: ",ddst.s,"/run") ;
				
			/** notification fd */
			if (!file_write_unsafe(ddst.s,SS_NOTIFICATION,"3\n",2))
				log_warnusys_return(LOG_EXIT_ZERO,"write: ",ddst.s,"/" SS_NOTIFICATION) ;
				
			if (sv->cname.itype == CLASSIC)
			{
				ddst.len-- ;
				if (!stralloc_cats(&ddst,"/run") ||
				!stralloc_0(&ddst)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
				
				if (chmod(ddst.s, mode) < 0)
					log_warnusys_return(LOG_EXIT_ZERO,"chmod ", ddst.s) ;
			}
			break;
		case CUSTOM:
			if (!write_exec(sv, &log->run,"run",ddst.s,mode))
				log_warnu_return(LOG_EXIT_ZERO,"write: ",ddst.s,"/run") ;
			break;
		default: log_warn_return(LOG_EXIT_ZERO,"unknown build value: ",get_keybyid(logbuild)) ;	
	}
		
	r = scan_mode(destlog.s,S_IFDIR) ;
	if (r == -1)
		log_warn_return(LOG_EXIT_ZERO,"log directory: ", destlog.s,": already exist with a different mode") ;

	
	if (!dir_create_parent(destlog.s,0755))
		log_warnusys_return(LOG_EXIT_ZERO,"create log directory: ",destlog.s) ;

	if ((!MYUID))
	{
		if (!youruid(&log_uid,logrunner) ||
		!yourgid(&log_gid,log_uid))
			log_warnusys_return(LOG_EXIT_ZERO,"get uid and gid of: ",logrunner) ;
	
		if (chown(destlog.s,log_uid,log_gid) == -1)
			log_warnusys_return(LOG_EXIT_ZERO,"chown: ",destlog.s) ;
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
	
	char pipefile[consdstlen + 1 + consnamelen + 1 + 1] ;
	
	/**producer-for*/
	if (!file_write_unsafe(consfile,get_keybyid(CONSUMER),prodname,strlen(prodname))) 
		log_warnu_return(LOG_EXIT_ZERO,"write: ",consfile,get_keybyid(CONSUMER)) ;
	
	/**consumer-for*/
	if (!file_write_unsafe(prodfile,get_keybyid(PRODUCER),consname,strlen(consname)))
		log_warnu_return(LOG_EXIT_ZERO,"write: ",prodfile,get_keybyid(PRODUCER)) ;

	/**pipeline**/
	if (sv->opts[1]) 
	{
		size_t len = strlen(deps.s+sv->pipeline) ;
		char pipename[len + 1] ;
		memcpy(pipefile,consdst,consdstlen) ;
		pipefile[consdstlen] = '/' ;
		memcpy(pipefile + consdstlen + 1, consname,consnamelen) ;
		pipefile[consdstlen + 1 + consnamelen] = '/' ;
		pipefile[consdstlen + 1 + consnamelen + 1] = 0  ;
		
		memcpy(pipename,deps.s+sv->pipeline,len) ;
		pipename[len] = 0 ;
		if (!file_write_unsafe(pipefile,PIPELINE_NAME,pipename,len))
			log_warnu_return(LOG_EXIT_ZERO,"write: ",pipefile,PIPELINE_NAME) ;
	}	
	
	return 1 ;
}

int write_common(sv_alltype *sv, char const *dst,uint8_t conf)
{
	int r ;
	char *time = NULL ;
	char *name = keep.s + sv->cname.name ;
	char *src = keep.s + sv->src ;
	size_t dstlen = strlen(dst) ;
	size_t srclen = strlen(src) ;
	size_t namelen = strlen(name) ;
	/**down file*/
	if (sv->flags[0])
	{
		if (!file_create_empty(dst,"down",0644))
			log_warnusys_return(LOG_EXIT_ZERO,"create down file") ;
	}
	/**nosetsid file*/
	if (sv->flags[1])
	{
		if (!file_create_empty(dst,"nosetsid",0644))
			log_warnusys_return(LOG_EXIT_ZERO,"create nosetsid file") ;
	}
	
	/**notification-fd*/
	if (sv->notification)
	{
		if (!write_uint(dst,"notification-fd", sv->notification))
			log_warnu_return(LOG_EXIT_ZERO,"write notification file") ;
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
				log_warnu_return(LOG_EXIT_ZERO,"write file: ",time) ;
		}
		
	}
	/** type file*/
	if (sv->cname.itype > CLASSIC)
	{
		if (!file_write_unsafe(dst,"type",get_keybyid(sv->cname.itype),strlen(get_keybyid(sv->cname.itype))))
			log_warnusys_return(LOG_EXIT_ZERO,"write type file") ;
	}
	/** max-death-tally */
	if (sv->death)
	{
		if (!write_uint(dst, "max-death-tally", sv->death))
			log_warnu_return(LOG_EXIT_ZERO,"write max-death-tally file") ;
	}
	/**down-signal*/
	if (sv->signal)
	{
		if (!write_uint(dst,"down-signal", sv->signal))
			log_warnu_return(LOG_EXIT_ZERO,"write down-signal file") ;
	}
	/** environment */
	if (sv->opts[2])
	{
		char *dst = keep.s + sv->srconf ;
		size_t dlen ;
		dlen = strlen(dst) ;
		char copy[dlen + namelen + 1] ;
		memcpy(copy,dst,dlen) ;
		memcpy(copy + dlen, name,namelen) ;
		copy[dlen + namelen] = 0 ;
		// copy config file from upstream in sysadmin
		r = scan_mode(copy,S_IFREG) ;
		if (!r || conf > 1)
		{
			copy[dlen] = 0 ;
			if (!write_env(name,&sv->saenv,copy))
				log_warnu_return(LOG_EXIT_ZERO,"write environment") ;
		}
		else if (conf == 1)
		{
			stralloc salist = STRALLOC_ZERO ;
			//merge config from upstream to sysadmin
			if (!file_readputsa(&salist,dst,name)) log_warnusys_return(LOG_EXIT_ZERO,"read: ",dst,name) ;
			if (!env_merge_conf(dst,name,&salist,&sv->saenv,conf))
				log_warnu_return(LOG_EXIT_ZERO,"merge environment file") ;

			stralloc_free(&salist) ;
		}
	}
	/** hierarchy copy */
	if (sv->hiercopy[0])
	{
		int r ;
		for (uint32_t i = 0 ; i < sv->hiercopy[0] ; i++)
		{
			char *what = keep.s + sv->hiercopy[i+1] ;
			size_t whatlen = strlen(what) ;
			char tmp[srclen + 1 + whatlen + 1] ;
			memcpy(tmp,src,srclen) ;
			tmp[srclen] = '/' ;
			memcpy(tmp + srclen + 1,what,whatlen) ;
			tmp[srclen + 1 + whatlen] = 0 ;
			char dtmp[dstlen + 1 + whatlen] ;
			memcpy(dtmp,dst,dstlen) ;
			dtmp[dstlen] = '/' ;
			memcpy(dtmp + dstlen + 1, what, whatlen) ;
			dtmp[dstlen + 1 + whatlen] = 0 ;	
			r = scan_mode(tmp,S_IFDIR) ;
			if (r <= 0)
			{
				r = scan_mode(tmp,S_IFREG) ;
				if (!r) log_warnusys_return(LOG_EXIT_ZERO,"find: ",tmp) ;
				if (r < 0) { errno = ENOTSUP ; log_warnsys_return(LOG_EXIT_ZERO,"invalid format of: ",tmp) ; }
			}
			if (!hiercopy(tmp,dtmp))
				log_warnusys_return(LOG_EXIT_ZERO,"copy: ",tmp," to: ",dtmp) ;
		}
	}
	return 1 ;
}

int write_exec(sv_alltype *sv, sv_exec *exec,char const *file,char const *dst,mode_t mode)
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
	
	switch (exec->build)
	{
		case AUTO:
			/** uid */
			if ((!owner && exec->runas))
			{
				if (!stralloc_cats(&ui,S6_BINPREFIX "s6-setuidgid ") ||
				!stralloc_cats(&ui,keep.s + exec->runas) || 
				!stralloc_cats(&ui,"\n")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
			}
			/** environment */
			if (sv->opts[2] && (exec->build == AUTO))
			{
				if (!stralloc_cats(&env,SS_BINPREFIX "execl-envfile ") ||
				!stralloc_cats(&env,keep.s + sv->srconf) || 
				!stralloc_cats(&env,name) || 
				!stralloc_cats(&env,"\n")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
			}
			/** shebang */
			if (type != ONESHOT)
			{
				if (!stralloc_cats(&shebang, "#!" EXECLINE_SHEBANGPREFIX "execlineb -P\n")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
			}
			break ;
		case CUSTOM:
			if (type != ONESHOT)
			{
				if (!stralloc_cats(&shebang, "#!") ||
				!stralloc_cats(&shebang, keep.s+exec->shebang) || 
				!stralloc_cats(&shebang,"\n")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
			}
			else
			{
				if (!stralloc_cats(&shebang, keep.s+exec->shebang) ||
				!stralloc_cats(&shebang," \"")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
			}
			break ;
		default: log_warn(LOG_EXIT_ZERO,"unknown ", get_keybyid(exec->build)," build type") ;
			break ;
	}
	/** close uid */
	if (!stralloc_0(&ui)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	/** close env*/
	if (!stralloc_0(&env)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	/** close shebang */
	if (!stralloc_0(&shebang)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	/** close command */
	if (!stralloc_cats(&runuser, keep.s+exec->exec)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	if ((type == ONESHOT) && (exec->build == CUSTOM))
	{
		if (!stralloc_cats(&runuser," \"")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	}
	if (!stralloc_cats(&runuser,"\n")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	if (!stralloc_0(&runuser)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	
	/** build the file*/	
	if (!stralloc_cats(&execute,shebang.s)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	if (exec->build == AUTO)
	{
		if (!stralloc_cats(&execute,EXECLINE_BINPREFIX "fdmove -c 2 1\n")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	}
	if (!stralloc_cats(&execute,env.s) ||
	!stralloc_cats(&execute,ui.s) || 
	!stralloc_cats(&execute,runuser.s)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;

	memcpy(write,dst,dstlen) ;
	write[dstlen] = '/' ;
	memcpy(write + dstlen + 1, file, filelen) ;
	write[dstlen + 1 + filelen] = 0 ;
	
	if (!file_write_unsafe(dst,file,execute.s,execute.len))
		log_warnusys_return(LOG_EXIT_ZERO,"write: ",dst,"/",file) ;
	
	if (chmod(write, mode) < 0)
		log_warnusys_return(LOG_EXIT_ZERO,"chmod ", write) ;
	
	stralloc_free(&home) ;
	stralloc_free(&shebang) ;
	stralloc_free(&ui) ;
	stralloc_free(&execute) ;
	stralloc_free(&env) ;
	stralloc_free(&runuser) ;
	stralloc_free(&execute) ;
	return 1 ;	
}

int write_dependencies(unsigned int nga,unsigned int idga,char const *dst,char const *filename)
{
	stralloc contents = STRALLOC_ZERO ;
	size_t id = idga, nid = nga ;
	for (;nid; id += strlen(deps.s + id) + 1, nid--)
	{
		if (!stralloc_cats(&contents,deps.s + id) ||
		!stralloc_cats(&contents,"\n")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	}
	
	//if (contents.len)
	{
		if (!file_write_unsafe(dst,filename,contents.s,contents.len))
		{
			log_warnusys("create file: ",dst,filename) ;
			goto err ;
		}
	}
	
	stralloc_free(&contents) ;
	return 1 ;
	err:
		stralloc_free(&contents) ;
		return 0 ;
}

int write_uint(char const *dst, char const *name, uint32_t ui)
{
	char number[UINT32_FMT] ; 
	
	if (!file_write_unsafe(dst,name,number,uint32_fmt(number,ui)))
		log_warnusys_return(LOG_EXIT_ZERO,"write: ",dst,"/",name) ;

	return 1 ;
}

int write_env(char const *name, stralloc *sa,char const *dst)
{
	int r ;
			
	r = scan_mode(dst,S_IFDIR) ;
	if (r < 0)
		log_warnsys_return(LOG_EXIT_ZERO," invalid environment directory: ",dst) ;
	else if (!r)
	{
		log_warnsys_return(LOG_EXIT_ZERO,dst," service environment directory doesn't exist") ;
	}
	if (!file_write_unsafe(dst,name,sa->s,sa->len))
		log_warnusys_return(LOG_EXIT_ZERO,"create file: ",dst,"/",name) ;

	return 1 ;
}

