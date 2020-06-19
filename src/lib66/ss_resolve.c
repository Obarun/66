/* 
 * ss_resolve.c
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

#include <string.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdlib.h>//realpath
#include <sys/types.h>
#include <stdio.h> //rename

#include <oblibs/types.h>
#include <oblibs/log.h>
#include <oblibs/directory.h>
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/direntry.h>
#include <skalibs/unix-transactional.h>
#include <skalibs/diuint32.h>
#include <skalibs/cdb_make.h>
#include <skalibs/cdb.h>

#include <66/constants.h>
#include <66/utils.h>
#include <66/enum.h>
#include <66/parser.h>//resolve need to find stralloc keep
#include <66/resolve.h>
#include <66/ssexec.h>
#include <66/state.h>

#include <s6/s6-supervise.h>

void ss_resolve_free(ss_resolve_t *res)
{
	stralloc_free(&res->sa) ;
}

void ss_resolve_init(ss_resolve_t *res)
{
	res->sa.len = 0 ;
	ss_resolve_add_string(res,"") ;
}

int ss_resolve_pointo(stralloc *sa,ssexec_t *info,int type, unsigned int where)
{
	stralloc tmp = STRALLOC_ZERO ;
	
	char ownerstr[UID_FMT] ;
	size_t ownerlen = uid_fmt(ownerstr,info->owner) ;
	ownerstr[ownerlen] = 0 ;

	if (where == SS_RESOLVE_STATE)
	{
		if (!stralloc_catb(&tmp,info->live.s,info->live.len - 1) ||
		!stralloc_cats(&tmp,SS_STATE) ||
		!stralloc_cats(&tmp,"/") ||
		!stralloc_cats(&tmp,ownerstr) ||
		!stralloc_cats(&tmp,"/") ||
		!stralloc_cats(&tmp,info->treename.s)) goto err ;
	}
	else if (where == SS_RESOLVE_SRC)
	{
		if (!stralloc_cats(&tmp,info->tree.s) ||
		!stralloc_cats(&tmp,SS_SVDIRS)) goto err ;
	}
	else if (where == SS_RESOLVE_BACK)		
	{
		if (!stralloc_cats(&tmp,info->base.s) ||
		!stralloc_cats(&tmp,SS_SYSTEM) ||
		!stralloc_cats(&tmp,SS_BACKUP) ||
		!stralloc_cats(&tmp,"/") ||
		!stralloc_cats(&tmp,info->treename.s)) goto err ;
	}
	else if (where == SS_RESOLVE_LIVE)
	{
		if (!stralloc_catb(&tmp,info->live.s,info->live.len - 1) ||
		!stralloc_cats(&tmp,SS_STATE) ||
		!stralloc_cats(&tmp,"/") ||
		!stralloc_cats(&tmp,ownerstr) ||
		!stralloc_cats(&tmp,"/") ||
		!stralloc_cats(&tmp,info->treename.s) || 
		!stralloc_cats(&tmp,SS_SVDIRS)) goto err ;
	}
	
	if (type >= 0 && where)
	{
		if (type == TYPE_CLASSIC)
		{
			if (!stralloc_cats(&tmp,SS_SVC)) goto err ;
		}
		else if (!stralloc_cats(&tmp,SS_DB)) goto err ;
	}
	if (!stralloc_0(&tmp) ||
	!stralloc_copy(sa,&tmp)) goto err ;
	
	stralloc_free(&tmp) ;
	return 1 ;
	
	err:
		stralloc_free(&tmp) ;
		return 0 ;
}

/* @sdir -> service dir
 * @mdir -> module dir */
int ss_resolve_module_path(stralloc *sdir, stralloc *mdir, char const *sv, uid_t owner)
{
	int r, insta ;
	stralloc sainsta = STRALLOC_ZERO ;
	stralloc mhome = STRALLOC_ZERO ; // module user dir
	stralloc shome = STRALLOC_ZERO ; // service user dir
	char const *src = 0 ;
	char const *dest = 0 ;
	
	insta = instance_check(sv) ;
	instance_splitname(&sainsta,sv,insta,SS_INSTANCE_TEMPLATE) ;

	if (!owner)
	{
		src = SS_MODULE_ADMDIR ;
		dest = SS_SERVICE_ADMDIR ;
	}
	else
	{	
		if (!set_ownerhome(&mhome,owner)) log_warnusys_return(LOG_EXIT_ZERO,"set home directory") ;
		if (!stralloc_cats(&mhome,SS_MODULE_USERDIR)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		if (!stralloc_0(&mhome)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		mhome.len-- ;
		src = mhome.s ;
		
		if (!set_ownerhome(&shome,owner)) log_warnusys_return(LOG_EXIT_ZERO,"set home directory") ;
		if (!stralloc_cats(&shome,SS_SERVICE_USERDIR)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		if (!stralloc_0(&shome)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		shome.len-- ;
		dest = shome.s ;
	
	}
	if (!auto_stra(mdir,src,sainsta.s)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	r = scan_mode(mdir->s,S_IFDIR) ;
	if (!r || r == -1)
	{
		mdir->len = 0 ;
		src = SS_MODULE_ADMDIR ;
		dest = SS_SERVICE_ADMDIR ;
		if (!auto_stra(mdir,src,sainsta.s)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		r = scan_mode(mdir->s,S_IFDIR) ;
		if (!r || r == -1)
		{
			mdir->len = 0 ;
			src = SS_MODULE_SYSDIR ;
			dest = SS_SERVICE_SYSDIR ;
			if (!auto_stra(mdir,src,sainsta.s)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
			r = scan_mode(mdir->s,S_IFDIR) ;
			if (!r || r == -1) log_warnu_return(LOG_EXIT_ZERO,"find module: ",sv) ;
		}
		
	}
	if (!auto_stra(sdir,dest,sv)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	
	stralloc_free(&sainsta) ;
	stralloc_free(&mhome) ;
	stralloc_free(&shome) ;
	return 1 ;
}

int ss_resolve_src_path(stralloc *sasrc,char const *sv, uid_t owner,char const *directory_forced)
{
	int r ;
	char const *src = 0 ;
	int found = 0, err = -1 ;
	stralloc home = STRALLOC_ZERO ;
	if (directory_forced)
	{
		if (!ss_resolve_cmp_service_basedir(directory_forced)) { log_warn("invalid base service directory: ",directory_forced) ; goto err ; }
		src = directory_forced ;
		r = ss_resolve_src(sasrc,sv,src,&found) ;
		if (r == -1){ log_warnusys("parse source directory: ",src) ; goto err ; }
		if (!r) { log_warnu("find service: ",sv) ; err = 0 ; goto err ; }
	}
	else
	{
		if (!owner) src = SS_SERVICE_ADMDIR ;
		else
		{
			if (!set_ownerhome(&home,owner)) { log_warnusys("set home directory") ; goto err ; }
			if (!stralloc_cats(&home,SS_SERVICE_USERDIR)) { log_warnsys("stralloc") ; goto err ; }
			if (!stralloc_0(&home)) { log_warnsys("stralloc") ; goto err ; }
			home.len-- ;
			src = home.s ;
		}

		r = ss_resolve_src(sasrc,sv,src,&found) ;
		if (r == -1){ log_warnusys("parse source directory: ",src) ; goto err ; }
		if (!r)
		{
			found = 0 ;
			src = SS_SERVICE_ADMDIR ;
			r = ss_resolve_src(sasrc,sv,src,&found) ;
			if (r == -1) { log_warnusys("parse source directory: ",src) ; goto err ; }
			if (!r)
			{
				found = 0 ;
				src = SS_SERVICE_SYSDIR ;
				r = ss_resolve_src(sasrc,sv,src,&found) ;
				if (r == -1) { log_warnusys("parse source directory: ",src) ; goto err ; }
				if (!r) { log_warnu("find service: ",sv) ; err = 0 ; goto err ; }
			}
		}
	}
	stralloc_free(&home) ;
	return 1 ;
	err:
		stralloc_free(&home) ;
		return err ;
}

int ss_resolve_src(stralloc *sasrc, char const *name, char const *src,int *found)
{
	int r, obr, insta ;
	size_t i, len, namelen = strlen(name), srclen = strlen(src), pos = 0 ;
	stralloc sainsta = STRALLOC_ZERO ;
	stralloc satmp = STRALLOC_ZERO ;
	stralloc sort = STRALLOC_ZERO ;

	if (!ss_resolve_sort_byfile_first(&sort,src))
	{
		log_warnu("sort directory: ",src) ;
		goto err ;
	}

	for (; pos < sort.len ; pos += strlen(sort.s + pos) + 1)
    {
		char *dname = sort.s + pos ;
		size_t dnamelen = strlen(dname) ;
		char bname[dnamelen + 1] ;

		if (!ob_basename(bname,dname)) goto err ;

		if (scan_mode(dname,S_IFDIR) > 0)
		{
			*found = ss_resolve_src(sasrc,name,dname,found) ;
			if (*found < 0) goto err ;
		}
		obr = 0 ;
		insta = 0 ;
		obr = obstr_equal(name,bname) ;
		insta = instance_check(name) ;

		if (insta > 0)
		{	
			if (!instance_splitname(&sainsta,name,insta,SS_INSTANCE_TEMPLATE)) goto err ;
			obr = obstr_equal(sainsta.s,bname) ;
		}

		if (obr)
		{
			*found = 1 ;
			if (scan_mode(dname,S_IFDIR) > 0)
			{
				int rd = ss_resolve_service_isdir(dname,bname) ;
				if (rd == -1) goto err ;
				if (!rd)
					r = sastr_dir_get(&satmp,dname,"",S_IFREG|S_IFDIR) ;
				else r = sastr_dir_get(&satmp,dname,"",S_IFREG) ;
				if (!r)
				{
					log_warnusys("get services from directory: ",dname) ;
					goto err ;
				}
				i = 0, len = satmp.len ;

				for (;i < len; i += strlen(satmp.s + i) + 1)
				{
					size_t tlen = strlen(satmp.s+i) ;
					char t[dnamelen + 1 + tlen + 2];

					auto_strings(t,dname,"/",satmp.s + i,"/") ;

					r = scan_mode(t,S_IFDIR) ;
					if (r == 1)
					{
						t[dnamelen + 1] = 0 ;
						*found = ss_resolve_src(sasrc,satmp.s+i,t,found) ;
						if (*found < 0) goto err ;
					}
					else
					{
						char t[dnamelen + tlen + 1] ;
						auto_strings(t,dname,satmp.s + i) ;
						if (sastr_cmp(sasrc,t) == -1)
						{
							if (!stralloc_cats(sasrc,dname)) goto err ;
							if (!stralloc_catb(sasrc,satmp.s+i,tlen+1)) goto err ;
						}
					}
				}
				break ;
			}
			else if (scan_mode(dname,S_IFREG) > 0)
			{
				char t[srclen + namelen + 1] ;
				auto_strings(t,src,name) ;
				if (sastr_cmp(sasrc,t) == -1)
				{
					if (!stralloc_cats(sasrc,src)) goto err ;
					if (!stralloc_catb(sasrc,name,namelen+1)) goto err ;
				}
				break ;
			}
			else goto err ;
		}
	}

	stralloc_free(&sainsta) ;
	stralloc_free(&satmp) ;
	stralloc_free(&sort) ;

	return (*found) ;

	err:
		stralloc_free(&sainsta) ;
		stralloc_free(&satmp) ;
		stralloc_free(&sort) ;
		return -1 ;
}

void ss_resolve_rmfile(char const *src,char const *name)
{
	size_t srclen = strlen(src) ;
	size_t namelen = strlen(name) ;

	char tmp[srclen + SS_RESOLVE_LEN + 1 + namelen +1] ;
	memcpy(tmp,src,srclen) ;
	memcpy(tmp + srclen, SS_RESOLVE,SS_RESOLVE_LEN) ;
	tmp[srclen + SS_RESOLVE_LEN] = '/' ;
	memcpy(tmp + srclen + SS_RESOLVE_LEN + 1, name, namelen) ;
	tmp[srclen + SS_RESOLVE_LEN + 1 + namelen] = 0 ;

	unlink_void(tmp) ;
}

int ss_resolve_add_uint32(stralloc *sa, uint32_t data)
{
	char pack[4] ;
	uint32_pack_big(pack,data) ;
	return stralloc_catb(sa,pack,4) ;
}

uint32_t ss_resolve_add_string(ss_resolve_t *res, char const *data)
{
	uint32_t baselen = res->sa.len ;
	if (!data)
	{
		if (!stralloc_catb(&res->sa,"",1)) log_warnusys_return(LOG_EXIT_LESSONE,"stralloc") ;
		return baselen ;
	}
	size_t datalen = strlen(data) ;
	if (!stralloc_catb(&res->sa,data,datalen + 1)) log_warnusys_return(LOG_EXIT_LESSONE,"stralloc") ;
	return baselen ;
}

int ss_resolve_pack(stralloc *sa, ss_resolve_t *res)
{
	if(!ss_resolve_add_uint32(sa,res->sa.len)) return 0 ;
	if (!stralloc_catb(sa,res->sa.s,res->sa.len)) return 0 ;
	if(!ss_resolve_add_uint32(sa,res->name) ||
	!ss_resolve_add_uint32(sa,res->description) ||
	!ss_resolve_add_uint32(sa,res->version) ||
	!ss_resolve_add_uint32(sa,res->logger) ||
	!ss_resolve_add_uint32(sa,res->logreal) ||
	!ss_resolve_add_uint32(sa,res->logassoc) ||
	!ss_resolve_add_uint32(sa,res->dstlog) ||
	!ss_resolve_add_uint32(sa,res->deps) ||
	!ss_resolve_add_uint32(sa,res->optsdeps) ||
	!ss_resolve_add_uint32(sa,res->extdeps) ||
	!ss_resolve_add_uint32(sa,res->contents) ||
	!ss_resolve_add_uint32(sa,res->src) ||
	!ss_resolve_add_uint32(sa,res->srconf) ||
	!ss_resolve_add_uint32(sa,res->live) ||
	!ss_resolve_add_uint32(sa,res->runat) ||
	!ss_resolve_add_uint32(sa,res->tree) ||
	!ss_resolve_add_uint32(sa,res->treename) ||
	!ss_resolve_add_uint32(sa,res->state) ||
	!ss_resolve_add_uint32(sa,res->exec_run) ||
	!ss_resolve_add_uint32(sa,res->exec_log_run) ||
	!ss_resolve_add_uint32(sa,res->real_exec_run) ||
	!ss_resolve_add_uint32(sa,res->real_exec_log_run) ||
	!ss_resolve_add_uint32(sa,res->exec_finish) ||
	!ss_resolve_add_uint32(sa,res->real_exec_finish) ||
	!ss_resolve_add_uint32(sa,res->type) ||
	!ss_resolve_add_uint32(sa,res->ndeps) ||
	!ss_resolve_add_uint32(sa,res->noptsdeps) ||
	!ss_resolve_add_uint32(sa,res->nextdeps) ||
	!ss_resolve_add_uint32(sa,res->ncontents) ||
	!ss_resolve_add_uint32(sa,res->down) || 
	!ss_resolve_add_uint32(sa,res->disen)) return 0 ;
	
	return 1 ;
}

int ss_resolve_write(ss_resolve_t *res, char const *dst, char const *name)
{
	
	//stralloc sa = STRALLOC_ZERO ;
	
	size_t dstlen = strlen(dst) ;
	size_t namelen = strlen(name) ;
	
	char tmp[dstlen + SS_RESOLVE_LEN + 1 + namelen + 1] ;
	auto_strings(tmp,dst,SS_RESOLVE,"/") ;

	if (!ss_resolve_write_cdb(res,tmp,name)) return 0 ;

	return 1 ;
}

int ss_resolve_read(ss_resolve_t *res, char const *src, char const *name)
{
	//stralloc sa = STRALLOC_ZERO ;
	size_t srclen = strlen(src) ;
	size_t namelen = strlen(name) ;
	
	char tmp[srclen + SS_RESOLVE_LEN + 1 + namelen + 1] ;
	auto_strings(tmp,src,SS_RESOLVE,"/",name) ;

	if (!ss_resolve_read_cdb(res,tmp)) return 0 ;

	return 1 ;
}

int ss_resolve_check_insrc(ssexec_t *info, char const *name)
{
	stralloc sares = STRALLOC_ZERO ;
	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC)) goto err ;
	if (!ss_resolve_check(sares.s,name)) goto err ;
	stralloc_free(&sares) ;
	return 1 ;
	err:
		stralloc_free(&sares) ;
		return 0 ;
}

int ss_resolve_check(char const *src, char const *name)
{
	int r ;
	size_t srclen = strlen(src) ;
	size_t namelen = strlen(name) ;
	char tmp[srclen + SS_RESOLVE_LEN + 1 + namelen + 1] ;
	memcpy(tmp,src,srclen) ;
	memcpy(tmp + srclen, SS_RESOLVE,SS_RESOLVE_LEN) ;
	tmp[srclen + SS_RESOLVE_LEN] = '/' ;
	memcpy(tmp + srclen + SS_RESOLVE_LEN + 1,name,namelen) ;
	tmp[srclen + SS_RESOLVE_LEN + 1 + namelen] = 0 ;
	r = scan_mode(tmp,S_IFREG) ;
	if (!r || r < 0) return 0 ;
	return 1 ;
}

int ss_resolve_setnwrite(sv_alltype *services, ssexec_t *info, char const *dst)
{
	char ownerstr[UID_FMT] ;
	size_t ownerlen = uid_fmt(ownerstr,info->owner), id, nid ;
	ownerstr[ownerlen] = 0 ;
	
	stralloc destlog = STRALLOC_ZERO ;
	stralloc ndeps = STRALLOC_ZERO ;
	stralloc other_deps = STRALLOC_ZERO ;
	
	ss_state_t sta = STATE_ZERO ;
	ss_resolve_t res = RESOLVE_ZERO ;
	ss_resolve_init(&res) ;
	
	char *name = keep.s + services->cname.name ;
	size_t namelen = strlen(name) ; 
	char logname[namelen + SS_LOG_SUFFIX_LEN + 1] ;
	char logreal[namelen + SS_LOG_SUFFIX_LEN + 1] ;
	char stmp[info->livetree.len + 1 + info->treename.len + SS_SVDIRS_LEN + 1 + namelen + SS_LOG_SUFFIX_LEN + 1] ;
	
	size_t livelen = info->live.len - 1 ; 
	char state[livelen + SS_STATE_LEN + 1 + ownerlen + 1 + info->treename.len + 1 + namelen + 1] ;
	memcpy(state,info->live.s,livelen) ;
	memcpy(state + livelen, SS_STATE,SS_STATE_LEN) ;
	state[livelen+ SS_STATE_LEN] = '/' ;
	memcpy(state + livelen + SS_STATE_LEN + 1,ownerstr,ownerlen) ;
	state[livelen + SS_STATE_LEN + 1 + ownerlen] = '/' ;
	memcpy(state + livelen + SS_STATE_LEN + 1 + ownerlen + 1,info->treename.s,info->treename.len) ;
	state[livelen + SS_STATE_LEN + 1 + ownerlen + 1 + info->treename.len] = 0 ;
	
	res.type = services->cname.itype ;
	res.name = ss_resolve_add_string(&res,name) ;
	res.description = ss_resolve_add_string(&res,keep.s + services->cname.description) ;
	/*** temporary check here, version is not mandatory yet */
	if (services->cname.version >= 0)
		res.version = ss_resolve_add_string(&res,keep.s + services->cname.version) ;
	res.tree = ss_resolve_add_string(&res,info->tree.s) ;
	res.treename = ss_resolve_add_string(&res,info->treename.s) ;
	res.live = ss_resolve_add_string(&res,info->live.s) ;
	res.state = ss_resolve_add_string(&res,state) ;
	res.src = ss_resolve_add_string(&res,keep.s + services->src) ;
	if (services->srconf > 0)
		res.srconf = ss_resolve_add_string(&res,keep.s + services->srconf) ;

	if (res.type == TYPE_ONESHOT)
	{
		if (services->type.oneshot.up.exec >= 0)
		{
			res.exec_run = ss_resolve_add_string(&res,keep.s + services->type.oneshot.up.exec) ;
			res.real_exec_run = ss_resolve_add_string(&res,keep.s + services->type.oneshot.up.real_exec) ;
		}
		if (services->type.oneshot.down.exec >= 0)
		{
			res.exec_finish = ss_resolve_add_string(&res,keep.s + services->type.oneshot.down.exec) ;
			res.real_exec_finish = ss_resolve_add_string(&res,keep.s + services->type.oneshot.down.real_exec) ;
		}
	}
	if (res.type == TYPE_CLASSIC || res.type == TYPE_LONGRUN)
	{
		if (services->type.classic_longrun.run.exec >= 0)
		{
			res.exec_run = ss_resolve_add_string(&res,keep.s + services->type.classic_longrun.run.exec) ;
			res.real_exec_run = ss_resolve_add_string(&res,keep.s + services->type.classic_longrun.run.real_exec) ;
		}
		if (services->type.classic_longrun.finish.exec >= 0)
		{
			res.exec_finish = ss_resolve_add_string(&res,keep.s + services->type.classic_longrun.finish.exec) ;
			res.real_exec_finish = ss_resolve_add_string(&res,keep.s + services->type.classic_longrun.finish.real_exec) ;
		}
	}
	
	res.ndeps = services->cname.nga ;
	res.noptsdeps = services->cname.nopts ;
	res.nextdeps = services->cname.next ;
	res.ncontents = services->cname.ncontents ;
	if (services->flags[0])	res.down = 1 ;
	res.disen = 1 ;
	if (res.type == TYPE_CLASSIC)
	{
		memcpy(stmp,info->scandir.s,info->scandir.len) ;
		stmp[info->scandir.len] = '/' ;
		memcpy(stmp + info->scandir.len + 1,name,namelen) ;
		stmp[info->scandir.len + 1 + namelen] = 0 ;
		res.runat = ss_resolve_add_string(&res,stmp) ;
	}
	else if (res.type >= TYPE_BUNDLE)
	{
		memcpy(stmp,info->livetree.s,info->livetree.len) ;
		stmp[info->livetree.len] = '/' ;
		memcpy(stmp + info->livetree.len + 1,info->treename.s,info->treename.len) ;
		memcpy(stmp + info->livetree.len + 1 + info->treename.len, SS_SVDIRS,SS_SVDIRS_LEN) ;
		stmp[info->livetree.len + 1 + info->treename.len + SS_SVDIRS_LEN] = '/' ;
		memcpy(stmp + info->livetree.len + 1 + info->treename.len + SS_SVDIRS_LEN + 1, name,namelen) ;
		stmp[info->livetree.len + 1 + info->treename.len + SS_SVDIRS_LEN + 1 + namelen] = 0 ;
		res.runat = ss_resolve_add_string(&res,stmp) ;
	}

	if (ss_state_check(state,name))
	{
		if (!ss_state_read(&sta,state,name)) { log_warnusys("read state file of: ",name) ; goto err ; }
		if (!sta.init)
			ss_state_setflag(&sta,SS_FLAGS_RELOAD,SS_FLAGS_TRUE) ;
		ss_state_setflag(&sta,SS_FLAGS_INIT,SS_FLAGS_FALSE) ;
		ss_state_setflag(&sta,SS_FLAGS_UNSUPERVISE,SS_FLAGS_FALSE) ;
		if (!ss_state_write(&sta,res.sa.s + res.state,name)){ log_warnusys("write state file of: ",name) ; goto err ; }
	}

	if (res.ndeps)
	{
		id = services->cname.idga, nid = res.ndeps ;
		for (;nid; id += strlen(deps.s + id) + 1, nid--)
		{
			if (!stralloc_catb(&ndeps,deps.s + id,strlen(deps.s + id)) ||
			!stralloc_catb(&ndeps," ",1)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		}
		ndeps.len-- ;
		if (!stralloc_0(&ndeps)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		res.deps = ss_resolve_add_string(&res,ndeps.s) ;
	}

	if (res.noptsdeps)
	{
		id = services->cname.idopts, nid = res.noptsdeps ;
		for (;nid; id += strlen(deps.s + id) + 1, nid--)
		{
			if (!stralloc_catb(&other_deps,deps.s + id,strlen(deps.s + id)) ||
			!stralloc_catb(&other_deps," ",1)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		}
		other_deps.len-- ;
		if (!stralloc_0(&other_deps)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		res.optsdeps = ss_resolve_add_string(&res,other_deps.s) ;
	}

	if (res.nextdeps)
	{
		other_deps.len = 0 ;
		id = services->cname.idext, nid = res.nextdeps ;
		for (;nid; id += strlen(deps.s + id) + 1, nid--)
		{
			if (!stralloc_catb(&other_deps,deps.s + id,strlen(deps.s + id)) ||
			!stralloc_catb(&other_deps," ",1)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		}
		other_deps.len-- ;
		if (!stralloc_0(&other_deps)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		res.extdeps = ss_resolve_add_string(&res,other_deps.s) ;
	}

	if (res.ncontents)
	{
		other_deps.len = 0 ;
		id = services->cname.idcontents, nid = res.ncontents ;
		for (;nid; id += strlen(deps.s + id) + 1, nid--)
		{
			if (!stralloc_catb(&other_deps,deps.s + id,strlen(deps.s + id)) ||
			!stralloc_catb(&other_deps," ",1)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		}
		other_deps.len-- ;
		if (!stralloc_0(&other_deps)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		res.contents = ss_resolve_add_string(&res,other_deps.s) ;
	}

	if (services->opts[0])
	{
		// destination of the logger
		if (services->type.classic_longrun.log.destination < 0)
		{	
			if(info->owner > 0)
			{	
				if (!stralloc_cats(&destlog,get_userhome(info->owner))) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
				if (!stralloc_cats(&destlog,"/")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
				if (!stralloc_cats(&destlog,SS_LOGGER_USERDIR)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
				if (!stralloc_cats(&destlog,name)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
			}
			else
			{
				if (!stralloc_cats(&destlog,SS_LOGGER_SYSDIR)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
				if (!stralloc_cats(&destlog,name)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
			}
		}
		else
		{
			if (!stralloc_cats(&destlog,keep.s+services->type.classic_longrun.log.destination)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		}
		if (!stralloc_0(&destlog)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		
		res.dstlog = ss_resolve_add_string(&res,destlog.s) ;
		
		if ((res.type == TYPE_CLASSIC) || (res.type == TYPE_LONGRUN))
		{
			memcpy(logname,name,namelen) ;
			memcpy(logname + namelen,SS_LOG_SUFFIX,SS_LOG_SUFFIX_LEN) ;
			logname[namelen + SS_LOG_SUFFIX_LEN] = 0 ;
		
			memcpy(logreal,name,namelen) ;
			if (res.type == TYPE_CLASSIC)
			{
				memcpy(logreal + namelen,"/log",SS_LOG_SUFFIX_LEN) ;
			}
			else memcpy(logreal + namelen,"-log",SS_LOG_SUFFIX_LEN) ;
			logreal[namelen + SS_LOG_SUFFIX_LEN] = 0 ;
			
			res.logger = ss_resolve_add_string(&res,logname) ;
			res.logreal = ss_resolve_add_string(&res,logreal) ;
			if (ndeps.len) ndeps.len--;
			if (!stralloc_catb(&ndeps," ",1)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
			if (!stralloc_cats(&ndeps,res.sa.s + res.logger)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
			if (!stralloc_0(&ndeps)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
			res.deps = ss_resolve_add_string(&res,ndeps.s) ;
			if (res.type == TYPE_CLASSIC) res.ndeps = 1 ;
			else if (res.type == TYPE_LONGRUN) res.ndeps += 1 ;

			if (services->type.classic_longrun.log.run.exec >= 0)
				res.exec_log_run = ss_resolve_add_string(&res,keep.s + services->type.classic_longrun.log.run.exec) ;

			if (services->type.classic_longrun.log.run.real_exec >= 0)
				res.real_exec_log_run = ss_resolve_add_string(&res,keep.s + services->type.classic_longrun.log.run.real_exec) ;

			if (!ss_resolve_setlognwrite(&res,dst,info)) goto err ;
		}
	}

	if (!ss_resolve_write(&res,dst,res.sa.s + res.name))
	{
		log_warnusys("write resolve file: ",dst,SS_RESOLVE,"/",res.sa.s + res.name) ;
		goto err ;
	}
	
	ss_resolve_free(&res) ;
	stralloc_free(&ndeps) ;
	stralloc_free(&other_deps) ;
	stralloc_free(&destlog) ;
	return 1 ;
	
	err:
		ss_resolve_free(&res) ;
		stralloc_free(&ndeps) ;
		stralloc_free(&other_deps) ;
		stralloc_free(&destlog) ;
		return 0 ;
}

int ss_resolve_setlognwrite(ss_resolve_t *sv, char const *dst,ssexec_t *info)
{
	if (!sv->logger) return 1 ;
	
	ss_state_t sta = STATE_ZERO ;
	ss_resolve_t res = RESOLVE_ZERO ;
	ss_resolve_init(&res) ;
	
	char *string = sv->sa.s ;
	size_t svlen = strlen(string + sv->name) ;
	char descrip[svlen + 7 + 1] ;
	memcpy(descrip,string + sv->name,svlen) ;
	memcpy(descrip + svlen," logger",7) ;
	descrip[svlen + 7] = 0 ;
	
	size_t runlen = strlen(string + sv->runat) ;
	char live[runlen + 4 + 1] ;
	memcpy(live,string + sv->runat,runlen) ;
	if (sv->type >= TYPE_BUNDLE)
	{
		memcpy(live + runlen,"-log",4)  ;
	}else memcpy(live + runlen,"/log",4)  ;
	live[runlen + 4] = 0 ;
	
	res.type = sv->type ;
	res.name = ss_resolve_add_string(&res,string + sv->logger) ;
	res.description = ss_resolve_add_string(&res,descrip) ;
	/*** temporary check here, version is not mandatory yet */
	if (sv->version > 0)
		res.version = ss_resolve_add_string(&res,string + sv->version) ;
	res.logreal = ss_resolve_add_string(&res,string + sv->logreal) ;
	res.logassoc = ss_resolve_add_string(&res,string + sv->name) ;
	res.dstlog = ss_resolve_add_string(&res,string + sv->dstlog) ;
	res.live = ss_resolve_add_string(&res,string + sv->live) ;
	res.runat = ss_resolve_add_string(&res,live) ;
	res.tree = ss_resolve_add_string(&res,string + sv->tree) ;
	res.treename = ss_resolve_add_string(&res,string + sv->treename) ;
	res.state = ss_resolve_add_string(&res,string + sv->state) ;
	res.src = ss_resolve_add_string(&res,string + sv->src) ;
	res.down = sv->down ;
	res.disen = sv->disen ;
	if (sv->exec_log_run >= 0)
		res.exec_log_run = ss_resolve_add_string(&res,string + sv->exec_log_run) ;
	if (sv->real_exec_log_run >= 0)
		res.real_exec_log_run = ss_resolve_add_string(&res,string + sv->real_exec_log_run) ;

	if (ss_state_check(string + sv->state,string + sv->logger))
	{
		if (!ss_state_read(&sta,string + sv->state,string + sv->logger)) { log_warnusys("read state file of: ",string + sv->logger) ; goto err ; }
		if (!sta.init)
			ss_state_setflag(&sta,SS_FLAGS_RELOAD,SS_FLAGS_TRUE) ;
		ss_state_setflag(&sta,SS_FLAGS_INIT,SS_FLAGS_FALSE) ;
		ss_state_setflag(&sta,SS_FLAGS_UNSUPERVISE,SS_FLAGS_FALSE) ;
		if (!ss_state_write(&sta,string + sv->state,string + sv->logger)){ log_warnusys("write state file of: ",string + sv->logger) ; goto err ; }
	}
	
	if (!ss_resolve_write(&res,dst,res.sa.s + res.name))
	{
		log_warnusys("write resolve file: ",dst,SS_RESOLVE,"/",res.sa.s + res.name) ;
		goto err ;
	}
	ss_resolve_free(&res) ;
	return 1 ;
	err:
		ss_resolve_free(&res) ;
		return 0 ;
}

int ss_resolve_cmp(genalloc *ga,char const *name)
{
	unsigned int i = 0 ;
	for (;i < genalloc_len(ss_resolve_t,ga) ; i++)
	{
		char *string = genalloc_s(ss_resolve_t,ga)[i].sa.s ;
		char *s = string + genalloc_s(ss_resolve_t,ga)[i].name ;
		if (obstr_equal(name,s)) return 1 ;
	}
	return 0 ;
}
	
int ss_resolve_copy(ss_resolve_t *dst,ss_resolve_t *res)
{
	ss_resolve_free(dst) ;
	size_t len = res->sa.len - 1 ;
	dst->salen = res->salen ;
	if (!stralloc_catb(&dst->sa,res->sa.s,len)) return 0 ;
	dst->name = res->name ;
	dst->description = res->description ;
	dst->version = res->version ;
	dst->logger = res->logger ;
	dst->logreal = res->logreal ;
	dst->logassoc = res->logassoc ;
	dst->dstlog = res->dstlog ;
	dst->deps = res->deps ;
	dst->optsdeps = res->optsdeps ;
	dst->extdeps = res->extdeps ;
	dst->contents = res->contents ;
	dst->src = res->src ;
	dst->srconf = res->srconf ;
	dst->live = res->live ;
	dst->runat = res->runat ;
	dst->tree = res->tree ;
	dst->treename = res->treename ;
	dst->state = res->state ;
	dst->exec_run = res->exec_run ;
	dst->exec_log_run = res->exec_log_run ;
	dst->real_exec_run = res->real_exec_run ;
	dst->real_exec_log_run = res->real_exec_log_run ;
	dst->exec_finish = res->exec_finish ;
	dst->real_exec_finish = res->real_exec_finish ;
	dst->type = res->type ;
	dst->ndeps = res->ndeps ;
	dst->noptsdeps = res->noptsdeps ;
	dst->nextdeps = res->nextdeps ;
	dst->ncontents = res->ncontents ;
	dst->down = res->down ;
	dst->disen = res->disen ;
	
	if (!stralloc_0(&dst->sa)) return 0 ;
	return 1 ;
}

int ss_resolve_append(genalloc *ga,ss_resolve_t *res)
{
	ss_resolve_t cp = RESOLVE_ZERO ;
	if (!ss_resolve_copy(&cp,res)) goto err ;
	if (!genalloc_append(ss_resolve_t,ga,&cp)) goto err ;
	return 1 ;
	err:
		ss_resolve_free(&cp) ;
		return 0 ;
}

int ss_resolve_add_deps(genalloc *tokeep,ss_resolve_t *res, char const *src)
{
	size_t pos = 0 ;
	stralloc tmp = STRALLOC_ZERO ;
	
	char *name = res->sa.s + res->name ;
	char *deps = res->sa.s + res->deps ;
	if (!ss_resolve_cmp(tokeep,name) && (!obstr_equal(name,SS_MASTER+1)))
		if (!ss_resolve_append(tokeep,res)) goto err ;

	if (res->ndeps)
	{
		if (!sastr_clean_string(&tmp,deps)) return 0 ;
		for (;pos < tmp.len ; pos += strlen(tmp.s + pos) + 1)
		{
			ss_resolve_t dres = RESOLVE_ZERO ;
			char *dname = tmp.s + pos ;
			if (!ss_resolve_check(src,dname)) goto err ;
			if (!ss_resolve_read(&dres,src,dname)) goto err ;
			if (dres.ndeps && !ss_resolve_cmp(tokeep,dname))
			{
				if (!ss_resolve_add_deps(tokeep,&dres,src)) goto err ;
			}
			if (!ss_resolve_cmp(tokeep,dname))
			{
				if (!ss_resolve_append(tokeep,&dres)) goto err ;
			}			
			ss_resolve_free(&dres) ;
		}
	}
	stralloc_free(&tmp) ;
	return 1 ;
	err:
		stralloc_free(&tmp) ;
		return 0 ;
}

int ss_resolve_add_rdeps(genalloc *tokeep, ss_resolve_t *res, char const *src)
{
	int type ;
	stralloc tmp = STRALLOC_ZERO ;
	stralloc nsv = STRALLOC_ZERO ;
	ss_state_t sta = STATE_ZERO ;
	
	char *name = res->sa.s + res->name ;
	size_t srclen = strlen(src), a = 0, b = 0, c = 0 ;
	char s[srclen + SS_RESOLVE_LEN + 1] ;
	memcpy(s,src,srclen) ;
	memcpy(s + srclen,SS_RESOLVE,SS_RESOLVE_LEN) ;
	s[srclen + SS_RESOLVE_LEN] = 0 ;

	if (res->type == TYPE_CLASSIC) type = 0 ;
	else type = 1 ;
	
	if (!sastr_dir_get(&nsv,s,SS_MASTER+1,S_IFREG)) goto err ;
	
	if (!ss_resolve_cmp(tokeep,name) && (!obstr_equal(name,SS_MASTER+1)))
	{
		if (!ss_resolve_append(tokeep,res)) goto err ;
	}
	if ((res->type == TYPE_BUNDLE || res->type == TYPE_MODULE) && res->ndeps)
	{
		uint32_t deps = res->type == TYPE_MODULE ? res->contents : res->deps ;
		if (!sastr_clean_string(&tmp,res->sa.s + deps)) goto err ;
		ss_resolve_t dres = RESOLVE_ZERO ;
		for (; a < tmp.len ; a += strlen(tmp.s + a) + 1)
		{	
			char *name = tmp.s + a ;
			if (!ss_resolve_check(src,name)) goto err ; 
			if (!ss_resolve_read(&dres,src,name)) goto err ;
			if (dres.type == TYPE_CLASSIC) continue ;
			if (!ss_resolve_cmp(tokeep,name))
			{
				if (!ss_resolve_append(tokeep,&dres)) goto err ;
				if (!ss_resolve_add_rdeps(tokeep,&dres,src)) goto err ;
			}
			ss_resolve_free(&dres) ;
		}
	}
	for (; b < nsv.len ; b += strlen(nsv.s + b) +1)
	{
		int dtype = 0 ;
		tmp.len = 0 ;
		ss_resolve_t dres = RESOLVE_ZERO ;
		char *dname = nsv.s + b ;
		if (obstr_equal(name,dname)) { ss_resolve_free(&dres) ; continue ; }
		if (!ss_resolve_check(src,dname)) goto err ;
		if (!ss_resolve_read(&dres,src,dname)) goto err ;
		
		if (dres.type == TYPE_CLASSIC) dtype = 0 ;
		else dtype = 1 ;
				
		if (ss_state_check(dres.sa.s + dres.state,dname))
		{ 
			if (!ss_state_read(&sta,dres.sa.s + dres.state,dname)) goto err ;
			if (dtype != type || (!dres.disen && !sta.unsupervise)){ ss_resolve_free(&dres) ; continue ; }
		}
		else if (dtype != type || (!dres.disen)){ ss_resolve_free(&dres) ; continue ; }
		if (dres.type == TYPE_BUNDLE && !dres.ndeps){ ss_resolve_free(&dres) ; continue ; }

		if (!ss_resolve_cmp(tokeep,dname))
		{
			if (dres.ndeps)// || (dres.type == TYPE_BUNDLE && dres.ndeps) || )
			{
				if (!sastr_clean_string(&tmp,dres.sa.s + dres.deps)) goto err ;
				/** we must check every service inside the module to not add as
				 * rdeps a service declared inside the module.
				 * eg. 
				 * module boot@system declare tty-rc@tty1
				 * we don't want boot@system as rdeps of tty-rc@tty1 but only
				 * service inside the module as rdeps of tty-rc@tty1 */
				if (dres.type == TYPE_MODULE)
					for (c = 0 ; c < tmp.len ; c += strlen(tmp.s + c) + 1)
						if (obstr_equal(name,tmp.s + c)) goto skip ;

				for (c = 0 ; c < tmp.len ; c += strlen(tmp.s + c) + 1)
				{
					if (obstr_equal(name,tmp.s + c))
					{
						if (!ss_resolve_append(tokeep,&dres)) goto err ;
						if (!ss_resolve_add_rdeps(tokeep,&dres,src)) goto err ;
						ss_resolve_free(&dres) ;
						break ;
					}
				}
			}
		}
		skip:
		ss_resolve_free(&dres) ;
	}
	
	stralloc_free(&nsv) ;
	stralloc_free(&tmp) ;
	return 1 ;
	err:
		stralloc_free(&nsv) ;
		stralloc_free(&tmp) ;
		return 0 ;
}

int ss_resolve_add_logger(genalloc *ga,char const *src)
{
	size_t i = 0 ;
	genalloc gatmp = GENALLOC_ZERO ;
	
	for (; i < genalloc_len(ss_resolve_t,ga) ; i++) 
	{
		ss_resolve_t res = RESOLVE_ZERO ;
		ss_resolve_t dres = RESOLVE_ZERO ;
		if (!ss_resolve_copy(&res,&genalloc_s(ss_resolve_t,ga)[i]))	goto err ;
		
		char *string = res.sa.s ;
		char *name = string + res.name ;
		if (!ss_resolve_cmp(&gatmp,name))
		{
			if (!ss_resolve_append(&gatmp,&res)) 
				goto err ;
		
			if (res.logger)
			{
				if (!ss_resolve_check(src,string + res.logger)) goto err ;
				if (!ss_resolve_read(&dres,src,string + res.logger))
					goto err ;
				if (!ss_resolve_cmp(&gatmp,string + res.logger))
					if (!ss_resolve_append(&gatmp,&dres)) goto err ;
			}
		}		
		ss_resolve_free(&res) ;
		ss_resolve_free(&dres) ;
	}
	genalloc_deepfree(ss_resolve_t,ga,ss_resolve_free) ;
	if (!genalloc_copy(ss_resolve_t,ga,&gatmp)) goto err ;

	genalloc_free(ss_resolve_t,&gatmp) ;
	return 1 ;
	err:
		genalloc_deepfree(ss_resolve_t,&gatmp,ss_resolve_free) ;
		return 0 ;
}

int ss_resolve_create_live(ssexec_t *info)
{
	int r ;
	gid_t gidowner ;
	if (!yourgid(&gidowner,info->owner)) return 0 ;
	stralloc sares = STRALLOC_ZERO ;
	stralloc ressrc = STRALLOC_ZERO ;

	if (!ss_resolve_pointo(&ressrc,info,SS_NOTYPE,SS_RESOLVE_SRC)) goto err ;

	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_STATE)) goto err ;
	r = scan_mode(sares.s,S_IFDIR) ;
	if (r < 0) goto err ;
	if (!r)
	{
		ssize_t len = get_rlen_until(sares.s,'/',sares.len) ;
		sares.len-- ;
		char sym[sares.len + SS_SVDIRS_LEN + 1] ;
		memcpy(sym,sares.s,sares.len) ;
		sym[sares.len] = 0 ;
		
		r = dir_create_parent(sym,0700) ;
		if (!r) goto err ;
		sym[len] = 0 ;
		if (chown(sym,info->owner,gidowner) < 0) goto err ;
		memcpy(sym,sares.s,sares.len) ;
		memcpy(sym + sares.len, SS_SVDIRS, SS_SVDIRS_LEN) ;
		sym[sares.len + SS_SVDIRS_LEN] = 0 ;
		
		log_trace("point symlink: ",sym," to ",ressrc.s) ;
		if (symlink(ressrc.s,sym) < 0)
		{
			log_warnusys("symlink: ", sym) ;
			goto err ;
		}
	}
	/** live/state/uid/treename/init file */
	if (!file_write_unsafe(sares.s,"init","",0)) goto err ;

	stralloc_free(&ressrc) ;
	stralloc_free(&sares) ;
	
	return 1 ;
	err:
		stralloc_free(&ressrc) ;
		stralloc_free(&sares) ;
		return 0 ;
}

int ss_resolve_search(genalloc *ga,char const *name)
{
	unsigned int i = 0 ;
	for (; i < genalloc_len(ss_resolve_t,ga) ; i++)
	{
		char *s = genalloc_s(ss_resolve_t,ga)[i].sa.s + genalloc_s(ss_resolve_t,ga)[i].name ;
		if (obstr_equal(name,s)) return i ;
	}
	return -1 ;
}

int ss_resolve_write_master(ssexec_t *info,ss_resolve_graph_t *graph,char const *dir, unsigned int reverse)
{
	int r ;
	size_t i = 0 ;
	char ownerstr[UID_FMT] ;
	size_t ownerlen = uid_fmt(ownerstr,info->owner) ;
	ownerstr[ownerlen] = 0 ;
	
	stralloc in = STRALLOC_ZERO ;
	stralloc inres = STRALLOC_ZERO ;
	stralloc gain = STRALLOC_ZERO ;
	ss_resolve_t res = RESOLVE_ZERO ;
	
	size_t dirlen = strlen(dir) ;
	
	char runat[info->livetree.len + 1 + info->treename.len + SS_SVDIRS_LEN + SS_MASTER_LEN + 1] ;
	memcpy(runat,info->livetree.s,info->livetree.len) ;
	runat[info->livetree.len] = '/' ;
	memcpy(runat + info->livetree.len + 1,info->treename.s,info->treename.len) ;
	memcpy(runat + info->livetree.len + 1 + info->treename.len, SS_SVDIRS,SS_SVDIRS_LEN) ;
	memcpy(runat + info->livetree.len + 1 + info->treename.len + SS_SVDIRS_LEN, SS_MASTER, SS_MASTER_LEN) ;
	runat[info->livetree.len + 1 + info->treename.len + SS_SVDIRS_LEN + SS_MASTER_LEN] = 0 ;
	
	char dst[dirlen + SS_DB_LEN + SS_SRC_LEN + SS_MASTER_LEN + 1] ;
	memcpy(dst, dir, dirlen) ;
	memcpy(dst + dirlen, SS_DB, SS_DB_LEN) ;
	memcpy(dst + dirlen + SS_DB_LEN, SS_SRC, SS_SRC_LEN) ;
	memcpy(dst + dirlen + SS_DB_LEN + SS_SRC_LEN, SS_MASTER, SS_MASTER_LEN) ;
	dst[dirlen + SS_DB_LEN + SS_SRC_LEN + SS_MASTER_LEN] = 0 ;
	
	size_t livelen = info->live.len - 1 ; 
	char state[livelen + SS_STATE_LEN + 1 + ownerlen + 1 + info->treename.len + 1] ;
	memcpy(state,info->live.s,livelen) ;
	memcpy(state + livelen, SS_STATE,SS_STATE_LEN) ;
	state[livelen+ SS_STATE_LEN] = '/' ;
	memcpy(state + livelen + SS_STATE_LEN + 1,ownerstr,ownerlen) ;
	state[livelen + SS_STATE_LEN + 1 + ownerlen] = '/' ;
	memcpy(state + livelen + SS_STATE_LEN + 1 + ownerlen + 1,info->treename.s,info->treename.len) ;
	state[livelen + SS_STATE_LEN + 1 + ownerlen + 1 + info->treename.len] = 0 ;
	
	if (reverse)
	{
		size_t dstlen = strlen(dst) ;
		char file[dstlen + 1 + SS_CONTENTS_LEN + 1] ;
		memcpy(file,dst,dstlen) ;
		file[dstlen] = '/' ;
		memcpy(file + dstlen + 1, SS_CONTENTS,SS_CONTENTS_LEN) ;
		file[dstlen + 1 + SS_CONTENTS_LEN] = 0 ;
		size_t filesize=file_get_size(file) ;
		
		r = openreadfileclose(file,&gain,filesize) ;
		if(!r) goto err ;
		/** ensure that we have an empty line at the end of the string*/
		if (!stralloc_cats(&gain,"\n")) goto err ;
		if (!stralloc_0(&gain)) goto err ;
		if (!sastr_clean_element(&gain)) goto err ;
	}
	
	for (; i < genalloc_len(ss_resolve_t,&graph->sorted); i++)
	{
		char *string = genalloc_s(ss_resolve_t,&graph->sorted)[i].sa.s ;
		char *name = string + genalloc_s(ss_resolve_t,&graph->sorted)[i].name ;
		if (reverse)
			if (sastr_cmp(&gain,name) == -1) continue ;
		
		if (!stralloc_cats(&in,name)) goto err ;
		if (!stralloc_cats(&in,"\n")) goto err ;
	
		if (!stralloc_cats(&inres,name)) goto err ;
		if (!stralloc_cats(&inres," ")) goto err ;
	}
		
	if (inres.len) inres.len--;
	if (!stralloc_0(&inres)) goto err ;
	
	r = file_write_unsafe(dst,SS_CONTENTS,in.s,in.len) ;
	if (!r) 
	{ 
		log_warnusys("write: ",dst,"contents") ;
		goto err ;
	}
	
	ss_resolve_init(&res) ;
	res.name = ss_resolve_add_string(&res,SS_MASTER+1) ;
	res.description = ss_resolve_add_string(&res,"inner bundle - do not use it") ;
	res.treename = ss_resolve_add_string(&res,info->treename.s) ;
	res.tree = ss_resolve_add_string(&res,info->tree.s) ;
	res.live = ss_resolve_add_string(&res,info->live.s) ;
	res.type = TYPE_BUNDLE ;
	res.deps = ss_resolve_add_string(&res,inres.s) ;
	res.ndeps = genalloc_len(ss_resolve_t,&graph->sorted) ;
	res.runat = ss_resolve_add_string(&res,runat) ;
	res.state = ss_resolve_add_string(&res,state) ;
		
	if (!ss_resolve_write(&res,dir,SS_MASTER+1)) goto err ;
	
	stralloc_free(&in) ;
	stralloc_free(&inres) ;
	ss_resolve_free(&res) ;
	stralloc_free(&gain) ;
	return 1 ;
	
	err:
		ss_resolve_free(&res) ;
		stralloc_free(&in) ;
		stralloc_free(&inres) ;
		stralloc_free(&gain) ;
		return 0 ;
}

int ss_resolve_sort_bytype(genalloc *gares,stralloc *list,char const *src)
{
	size_t pos = 0 ;
	ss_resolve_t res = RESOLVE_ZERO ;
	
	/** search for classic first */
	for (pos = 0 ;pos < list->len; pos += strlen(list->s + pos) + 1)
	{
		char *name = list->s + pos ;
		if (!ss_resolve_read(&res,src,name))
			log_warnu_return(LOG_EXIT_ZERO,"read resolve file of: ",name) ;
		if (res.type == TYPE_CLASSIC)
			if (ss_resolve_search(gares,name) == -1)
				if (!ss_resolve_append(gares,&res))
					log_warnu_return(LOG_EXIT_ZERO,"append genalloc") ;
	}
	
	/** second pass for module */
	for (pos = 0 ;pos < list->len; pos += strlen(list->s + pos) + 1)
	{
		char *name = list->s + pos ;
		if (!ss_resolve_read(&res,src,name))
			log_warnu_return(LOG_EXIT_ZERO,"read resolve file of: ",name) ;
		if (res.type == TYPE_MODULE)
			if (ss_resolve_search(gares,name) == -1)
				if (!ss_resolve_append(gares,&res))
					log_warnu_return(LOG_EXIT_ZERO,"append genalloc") ;
	}

	/** finally add s6-rc services */
	for (pos = 0 ;pos < list->len; pos += strlen(list->s + pos) + 1)
	{
		char *name = list->s + pos ;
		if (!ss_resolve_read(&res,src,name))
			log_warnu_return(LOG_EXIT_ZERO,"read resolve file of: ",name) ;
		if (res.type != TYPE_CLASSIC && res.type != TYPE_MODULE)
			if (ss_resolve_search(gares,name) == -1)
				if (!ss_resolve_append(gares,&res))
					log_warnu_return(LOG_EXIT_ZERO,"append genalloc") ;
	}
	
	ss_resolve_free(&res) ;
	return 1 ;
}

int ss_resolve_cmp_service_basedir(char const *dir)
{
	/** directory_forced can be 0, so nothing to do */
	if (!dir) return 1 ;
	size_t len = strlen(dir) ;
	uid_t owner = MYUID ;
	stralloc home = STRALLOC_ZERO ;

	char system[len + 1] ;
	char adm[len + 1] ;
	char user[len + 1] ;

	if (owner)
	{
		if (!set_ownerhome(&home,owner)) { log_warnusys("set home directory") ; goto err ; }
		if (!auto_stra(&home,SS_SERVICE_USERDIR)) { log_warnsys("stralloc") ; goto err ; }
		auto_strings(user,dir) ;
		user[strlen(home.s)] = 0 ;
	}

	if (len < strlen(SS_SERVICE_SYSDIR))
		if (len < strlen(SS_SERVICE_ADMDIR))
			if (owner) {
				if (len < strlen(home.s))
					goto err ;
			} else goto err ;

	auto_strings(system,dir) ;
	auto_strings(adm,dir) ;

	system[strlen(SS_SERVICE_SYSDIR)] = 0 ;
	adm[strlen(SS_SERVICE_ADMDIR)] = 0 ;

	if (strcmp(SS_SERVICE_SYSDIR,system))
		if (strcmp(SS_SERVICE_ADMDIR,adm))
			if (owner) {
				if (strcmp(home.s,user))
					goto err ;
			} else goto err ;

	stralloc_free(&home) ;
	return 1 ;
	err:
		stralloc_free(&home) ;
		return 0 ;
}

int ss_resolve_service_isdir(char const *dir, char const *name)
{
	size_t dirlen = strlen(dir) ;
	size_t namelen = strlen(name) ;
	char t[dirlen + 1 + namelen + 1] ;
	memcpy(t,dir,dirlen) ;
	t[dirlen] = '/' ;
	memcpy(t + dirlen + 1, name, namelen) ;
	t[dirlen + 1 + namelen] = 0 ;
	int r = scan_mode(t,S_IFREG) ;
	if (!ob_basename(t,dir)) return -1 ;
	if (!strcmp(t,name) && r) return 1 ;
	return 0 ;
}

int ss_resolve_sort_byfile_first(stralloc *sort, char const *src)
{
	int fdsrc ;
	stralloc tmp = STRALLOC_ZERO ;

	DIR *dir = opendir(src) ;
	if (!dir)
	{
		log_warnusys("open : ", src) ;
		goto errstra ;
	}
	fdsrc = dir_fd(dir) ;

	for (;;)
    {
		tmp.len = 0 ;
		struct stat st ;
		direntry *d ;
		d = readdir(dir) ;
		if (!d) break ;
		if (d->d_name[0] == '.')
		if (((d->d_name[1] == '.') && !d->d_name[2]) || !d->d_name[1])
			continue ;

		if (stat_at(fdsrc, d->d_name, &st) < 0)
		{
			log_warnusys("stat ", src, d->d_name) ;
			goto errdir ;
		}

		if (S_ISREG(st.st_mode))
		{
			if (!auto_stra(&tmp,src,d->d_name)) goto errdir ;
			if (!stralloc_insertb(sort,0,tmp.s,strlen(tmp.s) + 1)) goto errdir ;
		}
		else if(S_ISDIR(st.st_mode))
		{
			if (!auto_stra(&tmp,src,d->d_name,"/")) goto errdir ;
			if (!stralloc_insertb(sort,sort->len,tmp.s,strlen(tmp.s) + 1)) goto errdir ;
		}
	}

	dir_close(dir) ;
	stralloc_free(&tmp) ;

	return 1 ;

	errdir:
		dir_close(dir) ;
	errstra:
		stralloc_free(&tmp) ;
		return 0 ;
}

/** @Return -1 system error
 * @Return 0 no tree exist yet
 * @Return 1 svname doesn't exist
 * @Return > 2, service exist on different tree */
int ss_resolve_svtree(stralloc *svtree,char const *svname,char const *tree)
{
	uint8_t found = 1, copied = 0 ;
	stralloc satree = STRALLOC_ZERO ;
	stralloc tmp = STRALLOC_ZERO ;
	char ownerstr[UID_FMT] ;
	uid_t owner = getuid() ;
	size_t ownerlen = uid_fmt(ownerstr,owner), pos, newlen ;

	ownerstr[ownerlen] = 0 ;

	if (!set_ownersysdir(svtree,owner)) { log_warnusys("set owner directory") ; goto err ; }
	if (!auto_stra(svtree,SS_SYSTEM)) goto err ;

	if (!scan_mode(svtree->s,S_IFDIR))
	{
		found = 0 ;
		goto freed ;
	}

	if (!auto_stra(svtree,"/")) goto err ;
	newlen = svtree->len ;

	if (!stralloc_copy(&tmp,svtree)) goto err ;

	if (!sastr_dir_get(&satree, svtree->s,SS_BACKUP + 1, S_IFDIR)) {
		log_warnu("get list of trees from directory: ",svtree->s) ;
		goto err ;
	}

	if (satree.len)
	{
		for(pos = 0 ; pos < satree.len ; pos += strlen(satree.s + pos) + 1)
		{
			tmp.len = newlen ;
			char *name = satree.s + pos ;

			if (!auto_stra(&tmp,name,SS_SVDIRS)) goto err ;
			if (ss_resolve_check(tmp.s,svname))
			{
				if (!tree || (tree && !strcmp(name,tree))){
					svtree->len = 0 ;
					if (!stralloc_copy(svtree,&tmp)) goto err ;
					copied = 1 ;
				}
				found++ ;
			}
		}
	}
	else
	{
		found = 0 ;
		goto freed ;
	}

	if (found > 2 && tree) found = 2 ;
	if (!copied) found = 1 ;
	if (!stralloc_0(svtree)) goto err ;
	freed:
	stralloc_free(&satree) ;
	stralloc_free(&tmp) ;
	return found ;
	err:
		stralloc_free(&satree) ;
		stralloc_free(&tmp) ;
		return -1 ;
}

int ss_resolve_add_cdb_uint(struct cdb_make *c, char const *key,uint32_t data)
{
	char pack[4] ;
	size_t klen = strlen(key) ;

	uint32_pack_big(pack, data) ;
	if (cdb_make_add(c,key,klen,pack,4) < 0)
		log_warnsys_return(LOG_EXIT_ZERO,"cdb_make_add: ",key) ;

	return 1 ;
}

int ss_resolve_add_cdb(struct cdb_make *c,char const *key,char const *data)
{
	size_t klen = strlen(key) ;
	size_t dlen = strlen(data) ;

	if (cdb_make_add(c,key,klen,dlen ? data : 0,dlen) < 0)
		log_warnsys_return(LOG_EXIT_ZERO,"cdb_make_add: ",key) ;

	return 1 ;
}

int ss_resolve_write_cdb(ss_resolve_t *res, char const *dst, char const *name)
{
	int fd ;
	size_t dstlen = strlen(dst), namelen = strlen(name);
	struct cdb_make c = CDB_MAKE_ZERO ;
	char tfile[dstlen + 1 + namelen + namelen + 9] ;
	char dfile[dstlen + 1 + namelen + 1] ;

	char *str = res->sa.s ;

	auto_strings(dfile,dst,"/",name) ;

	auto_strings(tfile,dst,"/",name,":",name,":","XXXXXX") ;

	fd = mkstemp(tfile) ;
	if (fd < 0 || ndelay_off(fd)) {
		log_warnusys("mkstemp: ", tfile) ;
		goto err_fd ;
	}

	if (cdb_make_start(&c, fd) < 0) {
		log_warnusys("cdb_make_start") ;
		goto err ;
	}

	/* name */
	if (!ss_resolve_add_cdb(&c,"name",str + res->name) ||

	/* description */
	!ss_resolve_add_cdb(&c,"description",str + res->description) ||

	/* version */
	!ss_resolve_add_cdb(&c,"version",str + res->version) ||

	/* logger */
	!ss_resolve_add_cdb(&c,"logger",str + res->logger) ||

	/* logreal */
	!ss_resolve_add_cdb(&c,"logreal",str + res->logreal) ||

	/* logassoc */
	!ss_resolve_add_cdb(&c,"logassoc",str + res->logassoc) ||

	/* dstlog */
	!ss_resolve_add_cdb(&c,"dstlog",str + res->dstlog) ||

	/* deps */
	!ss_resolve_add_cdb(&c,"deps",str + res->deps) ||

	/* optsdeps */
	!ss_resolve_add_cdb(&c,"optsdeps",str + res->optsdeps) ||

	/* extdeps */
	!ss_resolve_add_cdb(&c,"extdeps",str + res->extdeps) ||

	/* contents */
	!ss_resolve_add_cdb(&c,"contents",str + res->contents) ||

	/* src */
	!ss_resolve_add_cdb(&c,"src",str + res->src) ||

	/* srconf */
	!ss_resolve_add_cdb(&c,"srconf",str + res->srconf) ||

	/* live */
	!ss_resolve_add_cdb(&c,"live",str + res->live) ||

	/* runat */
	!ss_resolve_add_cdb(&c,"runat",str + res->runat) ||

	/* tree */
	!ss_resolve_add_cdb(&c,"tree",str + res->tree) ||

	/* treename */
	!ss_resolve_add_cdb(&c,"treename",str + res->treename) ||

	/* dstlog */
	!ss_resolve_add_cdb(&c,"dstlog",str + res->dstlog) ||

	/* state */
	!ss_resolve_add_cdb(&c,"state",str + res->state) ||

	/* exec_run */
	!ss_resolve_add_cdb(&c,"exec_run",str + res->exec_run) ||

	/* exec_log_run */
	!ss_resolve_add_cdb(&c,"exec_log_run",str + res->exec_log_run) ||

	/* real_exec_run */
	!ss_resolve_add_cdb(&c,"real_exec_run",str + res->real_exec_run) ||

	/* real_exec_log_run */
	!ss_resolve_add_cdb(&c,"real_exec_log_run",str + res->real_exec_log_run) ||

	/* exec_finish */
	!ss_resolve_add_cdb(&c,"exec_finish",str + res->exec_finish) ||

	/* real_exec_finish */
	!ss_resolve_add_cdb(&c,"real_exec_finish",str + res->real_exec_finish) ||

	/* type */
	!ss_resolve_add_cdb_uint(&c,"type",res->type) ||

	/* ndeps */
	!ss_resolve_add_cdb_uint(&c,"ndeps",res->ndeps) ||

	/* noptsdeps */
	!ss_resolve_add_cdb_uint(&c,"noptsdeps",res->noptsdeps) ||

	/* nextdeps */
	!ss_resolve_add_cdb_uint(&c,"nextdeps",res->nextdeps) ||

	/* ncontents */
	!ss_resolve_add_cdb_uint(&c,"ncontents",res->ncontents) ||

	/* down */
	!ss_resolve_add_cdb_uint(&c,"down",res->down) ||

	/* disen */
	!ss_resolve_add_cdb_uint(&c,"disen",res->disen)) goto err ;

	if (cdb_make_finish(&c) < 0 || fsync(fd) < 0) {
		log_warnusys("write to: ", tfile) ;
		goto err ;
	}

	close(fd) ;

	if (rename(tfile, dfile) < 0) {
		log_warnusys("rename ", tfile, " to ", dfile) ;
		goto err_fd ;
	}

	return 1 ;

	err:
		close(fd) ;
	err_fd:
		unlink_void(tfile) ;
		return 0 ;
}

int ss_resolve_find_cdb(stralloc *result, cdb_t *c,char const *key)
{
	uint32_t x = 0 ;
	size_t klen = strlen(key) ;

	result->len = 0 ;

	int r = cdb_find(c, key, klen) ;
	if (r == -1) log_warnusys_return(LOG_EXIT_LESSONE,"search on cdb key: ",key) ;
	if (!r) log_warnusys_return(LOG_EXIT_ZERO,"unknown key on cdb: ",key) ;
	{
		char pack[cdb_datalen(c) + 1] ;

		if (cdb_read(c, pack, cdb_datalen(c),cdb_datapos(c)) < 0)
			log_warnusys_return(LOG_EXIT_LESSONE,"read on cdb value of key: ",key) ;

		pack[cdb_datalen(c)] = 0 ;
		uint32_unpack_big(pack, &x) ;

		if (!auto_stra(result,pack)) log_warnusys_return(LOG_EXIT_LESSONE,"stralloc") ;
	}

	return x ;
}

int ss_resolve_read_cdb(ss_resolve_t *dres, char const *name)
{
	int fd ;
	uint32_t x ;

	cdb_t c = CDB_ZERO ;
	stralloc tmp = STRALLOC_ZERO ;
	ss_resolve_t res = RESOLVE_ZERO ;

	fd = open_readb(name) ;
	if (fd < 0) {
		log_warnusys("open: ",name) ;
		goto err_fd ;
	}
	if (!cdb_init_map(&c, fd, 1)) {
		log_warnusys("cdb_init: ", name) ;
		goto err ;
	}

	ss_resolve_init(&res) ;

	/* name */
	ss_resolve_find_cdb(&tmp,&c,"name") ;
	res.name = tmp.len ? ss_resolve_add_string(&res,tmp.s) : 0 ;

	/* description */
	ss_resolve_find_cdb(&tmp,&c,"description") ;
	res.description = tmp.len ? ss_resolve_add_string(&res,tmp.s) : 0 ;

	/* version */
	ss_resolve_find_cdb(&tmp,&c,"version") ;
	res.version = tmp.len ? ss_resolve_add_string(&res,tmp.s) : 0 ;

	/* logger */
	ss_resolve_find_cdb(&tmp,&c,"logger") ;
	res.logger = tmp.len ? ss_resolve_add_string(&res,tmp.s) : 0 ;

	/* logreal */
	ss_resolve_find_cdb(&tmp,&c,"logreal") ;
	res.logreal = tmp.len ? ss_resolve_add_string(&res,tmp.s) : 0 ;

	/* logassoc */
	ss_resolve_find_cdb(&tmp,&c,"logassoc") ;
	res.logassoc = tmp.len ? ss_resolve_add_string(&res,tmp.s) : 0 ;

	/* dstlog */
	ss_resolve_find_cdb(&tmp,&c,"dstlog") ;
	res.dstlog = tmp.len ? ss_resolve_add_string(&res,tmp.s) : 0 ;

	/* deps */
	ss_resolve_find_cdb(&tmp,&c,"deps") ;
	res.deps = tmp.len ? ss_resolve_add_string(&res,tmp.s) : 0 ;

	/* optsdeps */
	ss_resolve_find_cdb(&tmp,&c,"optsdeps") ;
	res.optsdeps = tmp.len ? ss_resolve_add_string(&res,tmp.s) : 0 ;

	/* extdeps */
	ss_resolve_find_cdb(&tmp,&c,"extdeps") ;
	res.extdeps = tmp.len ? ss_resolve_add_string(&res,tmp.s) : 0 ;

	/* contents */
	ss_resolve_find_cdb(&tmp,&c,"contents") ;
	res.contents = tmp.len ? ss_resolve_add_string(&res,tmp.s) : 0 ;

	/* src */
	ss_resolve_find_cdb(&tmp,&c,"src") ;
	res.src = tmp.len ? ss_resolve_add_string(&res,tmp.s) : 0 ;

	/* srconf */
	ss_resolve_find_cdb(&tmp,&c,"srconf") ;
	res.srconf = tmp.len ? ss_resolve_add_string(&res,tmp.s) : 0 ;

	/* live */
	ss_resolve_find_cdb(&tmp,&c,"live") ;
	res.live = tmp.len ? ss_resolve_add_string(&res,tmp.s) : 0 ;

	/* runat */
	ss_resolve_find_cdb(&tmp,&c,"runat") ;
	res.runat = tmp.len ? ss_resolve_add_string(&res,tmp.s) : 0 ;

	/* tree */
	ss_resolve_find_cdb(&tmp,&c,"tree") ;
	res.tree = tmp.len ? ss_resolve_add_string(&res,tmp.s) : 0 ;

	/* treename */
	ss_resolve_find_cdb(&tmp,&c,"treename") ;
	res.treename = tmp.len ? ss_resolve_add_string(&res,tmp.s) : 0 ;

	/* state */
	ss_resolve_find_cdb(&tmp,&c,"state") ;
	res.state = tmp.len ? ss_resolve_add_string(&res,tmp.s) : 0 ;

	/* exec_run */
	ss_resolve_find_cdb(&tmp,&c,"exec_run") ;
	res.exec_run = tmp.len ? ss_resolve_add_string(&res,tmp.s) : 0 ;

	/* exec_log_run */
	ss_resolve_find_cdb(&tmp,&c,"exec_log_run") ;
	res.exec_log_run = tmp.len ? ss_resolve_add_string(&res,tmp.s) : 0 ;

	/* real_exec_run */
	ss_resolve_find_cdb(&tmp,&c,"real_exec_run") ;
	res.real_exec_run = tmp.len ? ss_resolve_add_string(&res,tmp.s) : 0 ;

	/* real_exec_log_run */
	ss_resolve_find_cdb(&tmp,&c,"real_exec_log_run") ;
	res.real_exec_log_run = tmp.len ? ss_resolve_add_string(&res,tmp.s) : 0 ;

	/* exec_finish */
	ss_resolve_find_cdb(&tmp,&c,"exec_finish") ;
	res.exec_finish = tmp.len ? ss_resolve_add_string(&res,tmp.s) : 0 ;

	/* real_exec_finish */
	ss_resolve_find_cdb(&tmp,&c,"real_exec_finish") ;
	res.real_exec_finish = tmp.len ? ss_resolve_add_string(&res,tmp.s) : 0 ;

	/* type */
	x = ss_resolve_find_cdb(&tmp,&c,"type") ;
	res.type = x ;

	/* ndeps */
	x = ss_resolve_find_cdb(&tmp,&c,"ndeps") ;
	res.ndeps = x ;

	/* noptsdeps */
	x =ss_resolve_find_cdb(&tmp,&c,"noptsdeps") ;
	res.noptsdeps = x ;

	/* nextdeps */
	x = ss_resolve_find_cdb(&tmp,&c,"nextdeps") ;
	res.nextdeps = x ;

	/* ncontents */
	x = ss_resolve_find_cdb(&tmp,&c,"ncontents") ;
	res.ncontents = x ;

	/* down */
	x = ss_resolve_find_cdb(&tmp,&c,"down") ;
	res.down = x ;

	/* disen */
	x = ss_resolve_find_cdb(&tmp,&c,"disen") ;
	res.disen = x ;

	if (!ss_resolve_copy(dres,&res)) goto err ;

	close(fd) ;
	cdb_free(&c) ;
	ss_resolve_free(&res) ;
	stralloc_free(&tmp) ;

	return 1 ;

	err:
		close(fd) ;
	err_fd:
		cdb_free(&c) ;
		ss_resolve_free(&res) ;
		stralloc_free(&tmp) ;
		return 0 ;
}
