/* 
 * resolve.c
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
#include <sys/stat.h>
#include <stdlib.h>//realpath

#include <oblibs/types.h>
#include <oblibs/error2.h>
#include <oblibs/directory.h>
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/stralist.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/direntry.h>
#include <skalibs/unix-transactional.h>
#include <skalibs/diuint32.h>

#include <66/constants.h>
#include <66/utils.h>
#include <66/enum.h>
#include <66/parser.h>//resolve need to find stralloc keep
#include <66/resolve.h>
#include <66/ssexec.h>
#include <66/graph.h>

#include <s6/s6-supervise.h>
#include <stdio.h>



/*
int resolve_symlive(char const *live, char const *tree, char const *treename)
{
	int r ;
	
	size_t livelen = strlen(live) ;
	size_t treelen = strlen(tree) ;
	size_t treenamelen = strlen(treename) ;
	size_t newlen ;
	
	uid_t owner = MYUID ;
	char ownerstr[256] ;
	size_t ownerlen = uid_fmt(ownerstr,owner) ;
	ownerstr[ownerlen] = 0 ;
	struct stat st ;
	
	char dst[treelen + SS_SVDIRS_LEN + 1] ;
	memcpy(dst,tree,treelen) ;
	memcpy(dst + treelen, SS_SVDIRS, SS_SVDIRS_LEN) ;
	dst[treelen + SS_SVDIRS_LEN] = 0 ;
	
	char sym[livelen + SS_RESOLVE_LEN + 1 + ownerlen + 1 + treenamelen + SS_RESOLVE_LEN + 1] ;
	memcpy(sym, live,livelen) ;
	memcpy(sym + livelen, SS_RESOLVE, SS_RESOLVE_LEN) ;
	sym[livelen + SS_RESOLVE_LEN] = '/' ;
	memcpy(sym + livelen + SS_RESOLVE_LEN + 1, ownerstr,ownerlen) ;
	newlen = livelen + SS_RESOLVE_LEN + 1 + ownerlen ;
	sym[livelen + SS_RESOLVE_LEN + 1 + ownerlen] = '/' ;
	memcpy(sym + livelen + SS_RESOLVE_LEN + 1 + ownerlen + 1, treename,treenamelen) ;
	sym[livelen + SS_RESOLVE_LEN + 1 + ownerlen + 1 + treenamelen] = 0 ;
	
	r = scan_mode(sym,S_IFDIR) ;
	if (r < 0)
	{
		VERBO3 strerr_warnw2x("invalid directory: ",sym) ;
		return 0 ;
	}
	if (!r)
	{
		sym[newlen] = 0 ;
		if (!dir_create_under(sym,treename,0755))
		{
			VERBO3 strerr_warnwu4sys("create directory: ",sym,"/",treename) ;
			return 0 ;
		}
		sym[newlen] = '/' ;
		memcpy(sym + newlen + 1, treename,treenamelen) ;
		sym[livelen + newlen + 1 + treenamelen] = 0 ;
	}
	memcpy(sym + newlen + 1 + treenamelen, SS_RESOLVE,SS_RESOLVE_LEN) ;
	sym[newlen + 1 + treenamelen + SS_RESOLVE_LEN] = 0 ;
		
	if(lstat(sym,&st) < 0)
	{
		if (symlink(dst,sym) < 0)
		{
			VERBO3 strerr_warnwu4x("point symlink: ",sym," to ",dst) ;
			return 0 ;
		}
	}
	if (unlink(sym) < 0)
	{
		VERBO3 strerr_warnwu2sys("unlink: ",sym) ;
		return 0 ;
	}
	if (symlink(dst,sym) < 0)
	{
		VERBO3 strerr_warnwu4x("point symlink: ",sym," to ",dst) ;
		return 0 ;
	}
		
	return 1 ;
}
*/
int ss_resolve_pointo(stralloc *sa,ssexec_t *info,unsigned int type, unsigned int where)
{
	stralloc tmp = STRALLOC_ZERO ;
	
	char ownerstr[256] ;
	size_t ownerlen = uid_fmt(ownerstr,info->owner) ;
	ownerstr[ownerlen] = 0 ;

	if (where == SS_RESOLVE_LIVE)
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
	
	if (type && where)
	{
		if (type == CLASSIC)
		{
			if (!stralloc_cats(&tmp,SS_SVC)) goto err ;
		}
		else if (!stralloc_cats(&tmp,SS_DB)) goto err ;
	}
	if (!stralloc_0(&tmp) ||
	!stralloc_obreplace(sa,tmp.s)) goto err ;
	
	stralloc_free(&tmp) ;
	return 1 ;
	
	err:
		stralloc_free(&tmp) ;
		return 0 ;
}

int ss_resolve_src(genalloc *ga, stralloc *sasrc, char const *name, char const *src,unsigned int *found)
{
	int fdsrc, obr, insta ;
	
	diuint32 svtmp = DIUINT32_ZERO ;//left->name,right->src
	
	size_t srclen = strlen(src) ;
	size_t namelen = strlen(name) ;
	genalloc tmp = GENALLOC_ZERO ; //type stralist
	
	stralloc sainsta = STRALLOC_ZERO ;
	stralloc subdir = STRALLOC_ZERO ;
	if (!stralloc_cats(&subdir,src)) goto errstra ;
	
	obr = insta = 0 ;
	
	DIR *dir = opendir(src) ;
	if (!dir)
	{
		VERBO3 strerr_warnwu2sys("open : ", src) ;
		goto errstra ;
	}
	fdsrc = dir_fd(dir) ;

	for (;;)
    {
		struct stat st ;
		direntry *d ;
		d = readdir(dir) ;
		if (!d) break ;
		if (d->d_name[0] == '.')
		if (((d->d_name[1] == '.') && !d->d_name[2]) || !d->d_name[1])
			continue ;
	
		if (stat_at(fdsrc, d->d_name, &st) < 0)
		{
			VERBO3 strerr_warnwu3sys("stat ", src, d->d_name) ;
			goto errdir ;
		}
		if (S_ISDIR(st.st_mode))
		{
			if (!stralloc_cats(&subdir,d->d_name)) goto errdir ;
			if (!stralloc_cats(&subdir,"/")) goto errdir ;
			if (!stralloc_0(&subdir)) goto errdir ;
			*found = ss_resolve_src(ga,sasrc,name,subdir.s,found) ;
			if (*found < 0) goto errdir ;
		}
		obr = 0 ;
		insta = 0 ;
		obr = obstr_equal(name,d->d_name) ;
		insta = insta_check(name) ;
			
		if (insta > 0)
		{	
			if (!insta_splitname(&sainsta,name,insta,0)) goto errdir ;
			obr = obstr_equal(sainsta.s,d->d_name) ;
		}
				
		if (obr)
		{
			*found = 1 ;
			if (stat_at(fdsrc, d->d_name, &st) < 0)
			{
				VERBO3 strerr_warnwu3sys("stat ", src, d->d_name) ;
				goto errdir ;
			}
			
			if (S_ISDIR(st.st_mode))
			{
				if (!stralloc_cats(&subdir,d->d_name)) goto errdir ;
				if (!stralloc_0(&subdir)) goto errdir ;
				
				if (!dir_get(&tmp,subdir.s,"",S_IFREG))
				{
					strerr_warnwu2sys("get services from directory: ",subdir.s) ;
					goto errdir ;
				}
				for (unsigned int i = 0 ; i < genalloc_len(stralist,&tmp) ; i++)
				{
					svtmp.left = sasrc->len ;
					if (!stralloc_catb(sasrc,gaistr(&tmp,i), gaistrlen(&tmp,i) + 1)) goto errdir ;
					svtmp.right = sasrc->len ;
					if (!stralloc_catb(sasrc,subdir.s, subdir.len + 1)) goto errdir ;
					if (!genalloc_append(diuint32,ga,&svtmp)) goto errdir ;
				}
				break ;
			}
			else if(S_ISREG(st.st_mode))
			{
				svtmp.left = sasrc->len ;
				if (!stralloc_catb(sasrc,name, namelen + 1)) goto errdir ;
				svtmp.right = sasrc->len ;
				if (!stralloc_catb(sasrc,src,srclen + 1)) goto errdir ;
				if (!genalloc_append(diuint32,ga,&svtmp)) goto errdir ;
				break ;
			}
			else goto errdir ;
		}
	}
	
	dir_close(dir) ;
	genalloc_deepfree(stralist,&tmp,stra_free) ;
	stralloc_free(&subdir) ;
	stralloc_free(&sainsta) ;

	return (*found) ;

	errdir:
		dir_close(dir) ;
	errstra:
		genalloc_deepfree(stralist,&tmp,stra_free) ;
		stralloc_free(&subdir) ;
		stralloc_free(&sainsta) ;
		return -1 ;
}

int ss_resolve_rmfile(ss_resolve_t *res, char const *src,char const *name,int both)
{
	stralloc sa = STRALLOC_ZERO ;
	
	size_t srclen = strlen(src) ;
	size_t namelen = strlen(name) ;

	char tmp[srclen + SS_RESOLVE_LEN + 1 + namelen +1] ;
	memcpy(tmp,src,srclen) ;
	memcpy(tmp + srclen, SS_RESOLVE,SS_RESOLVE_LEN) ;
	tmp[srclen + SS_RESOLVE_LEN] = '/' ;
	memcpy(tmp + srclen + SS_RESOLVE_LEN + 1, name, namelen) ;
	tmp[srclen + SS_RESOLVE_LEN + 1 + namelen] = 0 ;

	unlink_void(tmp) ;
	if (both)
	{
		/** write in src */
		if (!stralloc_cats(&sa,res->sa.s + res->tree)) goto err ;
		if (!stralloc_cats(&sa,SS_SVDIRS)) goto err ;
		if (!stralloc_cats(&sa,SS_RESOLVE)) goto err ;
		if (!stralloc_cats(&sa,"/")) goto err ;
		if (!stralloc_cats(&sa,name)) goto err ;
		if (!stralloc_0(&sa)) goto err ;
		unlink_void(sa.s) ;
	}
	stralloc_free(&sa) ;
	return 1 ;
	err:
		stralloc_free(&sa) ;
		return 0 ;
}

int ss_resolve_add_uint32(stralloc *sa, uint32_t data)
{
	char pack[4] ;
	uint32_pack_big(pack,data) ;
	return stralloc_catb(sa,pack,4) ;
}

int ss_resolve_add_uint64(stralloc *sa, uint64_t data)
{
	char pack[8] ;
	uint64_pack_big(pack,data) ;
	return stralloc_catb(sa,pack,8) ;
}

uint32_t ss_resolve_add_string(ss_resolve_t *res, char const *data)
{
	uint32_t baselen = res->sa.len ;
	if (!data)
	{
		if (!stralloc_catb(&res->sa,"",1)) strerr_diefu1sys(111,"stralloc:resolve_add_string") ;
		return baselen ;
	}
	size_t datalen = strlen(data) ;
	if (!stralloc_catb(&res->sa,data,datalen + 1)) strerr_diefu1sys(111,"stralloc:resolve_add_string") ;
	return baselen ;
}

int ss_resolve_pack(stralloc *sa, ss_resolve_t *res)
{
	if(!ss_resolve_add_uint32(sa,res->sa.len)) return 0 ;
	if (!stralloc_catb(sa,res->sa.s,res->sa.len)) return 0 ;
	if(!ss_resolve_add_uint32(sa,res->name) ||
	!ss_resolve_add_uint32(sa,res->description) ||
	!ss_resolve_add_uint32(sa,res->logger) ||
	!ss_resolve_add_uint32(sa,res->logreal) ||
	!ss_resolve_add_uint32(sa,res->logassoc) ||
	!ss_resolve_add_uint32(sa,res->dstlog) ||
	!ss_resolve_add_uint32(sa,res->deps) ||
	!ss_resolve_add_uint32(sa,res->src) ||
	!ss_resolve_add_uint32(sa,res->live) ||
	!ss_resolve_add_uint32(sa,res->runat) ||
	!ss_resolve_add_uint32(sa,res->tree) ||
	!ss_resolve_add_uint32(sa,res->treename) ||
	!ss_resolve_add_uint32(sa,res->resolve) ||
	!ss_resolve_add_uint32(sa,res->exec_run) ||
	!ss_resolve_add_uint32(sa,res->exec_finish) ||
	!ss_resolve_add_uint32(sa,res->type) ||
	!ss_resolve_add_uint32(sa,res->ndeps) ||
	!ss_resolve_add_uint32(sa,res->reload) ||
	!ss_resolve_add_uint32(sa,res->disen) ||
	!ss_resolve_add_uint32(sa,res->init) ||
	!ss_resolve_add_uint32(sa,res->unsupervise) ||
	!ss_resolve_add_uint32(sa,res->down) ||
	!ss_resolve_add_uint32(sa,res->run) ||
	!ss_resolve_add_uint64(sa,res->pid)) return 0 ;
	
	return 1 ;
}

int ss_resolve_write(ss_resolve_t *res, char const *dst, char const *name,int both)
{
	
	stralloc sa = STRALLOC_ZERO ;
	stralloc sasrc = STRALLOC_ZERO ;
	
	size_t dstlen = strlen(dst) ;
	size_t namelen = strlen(name) ;
	
	char tmp[dstlen + SS_RESOLVE_LEN + 1 + namelen + 1] ;
	memcpy(tmp,dst,dstlen) ;
	memcpy(tmp + dstlen, SS_RESOLVE, SS_RESOLVE_LEN) ;
	tmp[dstlen + SS_RESOLVE_LEN] = '/' ;
	memcpy(tmp + dstlen + SS_RESOLVE_LEN + 1, name, namelen) ;
	tmp[dstlen + SS_RESOLVE_LEN + 1 + namelen] = 0 ;
	
	if (!ss_resolve_pack(&sa,res)) goto err ;
	if (!openwritenclose_unsafe(tmp,sa.s,sa.len)) goto err ;
	if (both)
	{
		/** write in src */
		if (!stralloc_cats(&sasrc,res->sa.s + res->tree)) goto err ;
		if (!stralloc_cats(&sasrc,SS_SVDIRS)) goto err ;
		if (!stralloc_cats(&sasrc,SS_RESOLVE)) goto err ;
		if (!stralloc_cats(&sasrc,"/")) goto err ;
		if (!stralloc_cats(&sasrc,name)) goto err ;
		if (!stralloc_0(&sasrc)) goto err ;
		if (!openwritenclose_unsafe(sasrc.s,sa.s,sa.len)) goto err ;
	}
	
	stralloc_free(&sa) ;
	stralloc_free(&sasrc) ;
	return 1 ;
	
	err:
		stralloc_free(&sa) ;
		stralloc_free(&sasrc) ;
		return 0 ;
}

int ss_resolve_read(ss_resolve_t *res, char const *src, char const *name)
{
	stralloc sa = STRALLOC_ZERO ;
	size_t srclen = strlen(src) ;
	size_t namelen = strlen(name) ;
	
	char tmp[srclen + SS_RESOLVE_LEN + 1 + namelen + 1] ;
	memcpy(tmp,src,srclen) ;
	memcpy(tmp + srclen, SS_RESOLVE, SS_RESOLVE_LEN) ;
	tmp[srclen + SS_RESOLVE_LEN] = '/' ;
	memcpy(tmp + srclen + SS_RESOLVE_LEN + 1, name, namelen) ;
	tmp[srclen + SS_RESOLVE_LEN + 1 + namelen] = 0 ;

	size_t global = 0 ;
	size_t filen = file_get_size(tmp) ;
	int r = openreadfileclose(tmp,&sa,filen) ;
	if (r < 0) goto end ;

	uint32_unpack_big(sa.s,&res->salen) ;
	global += 4 ;
	stralloc_copyb(&res->sa,sa.s+global,res->salen) ;
	global += res->sa.len ;
	uint32_unpack_big(sa.s + global,&res->name) ;
	global += 4 ;
	uint32_unpack_big(sa.s + global,&res->description) ;
	global += 4 ;
	uint32_unpack_big(sa.s + global,&res->logger) ;
	global += 4 ;
	uint32_unpack_big(sa.s + global,&res->logreal) ;
	global += 4 ;
	uint32_unpack_big(sa.s + global,&res->logassoc) ;
	global += 4 ;
	uint32_unpack_big(sa.s + global,&res->dstlog) ;
	global += 4 ;
	uint32_unpack_big(sa.s + global,&res->deps) ;
	global += 4 ;
	uint32_unpack_big(sa.s + global,&res->src) ;
	
	global += 4 ;
	uint32_unpack_big(sa.s + global,&res->live) ;
	global += 4 ;
	uint32_unpack_big(sa.s + global,&res->runat) ;
	global += 4 ;
	uint32_unpack_big(sa.s + global,&res->tree) ;
	global += 4 ;
	uint32_unpack_big(sa.s + global,&res->treename) ;
	global += 4 ;
	uint32_unpack_big(sa.s + global,&res->resolve) ;
	global += 4 ;
	uint32_unpack_big(sa.s + global,&res->exec_run) ;
	global += 4 ;
	uint32_unpack_big(sa.s + global,&res->exec_finish) ;
	
	global += 4 ;
	uint32_unpack_big(sa.s + global,&res->type) ;
	global += 4 ;
	uint32_unpack_big(sa.s + global,&res->ndeps) ;
	global += 4 ;
	uint32_unpack_big(sa.s + global,&res->reload) ;
	global += 4 ;
	uint32_unpack_big(sa.s + global,&res->disen) ;
	global += 4 ;
	uint32_unpack_big(sa.s + global,&res->init) ;
	global += 4 ;
	uint32_unpack_big(sa.s + global,&res->unsupervise) ;
	global += 4 ;
	uint32_unpack_big(sa.s + global,&res->down) ;
	global += 4 ;
	uint32_unpack_big(sa.s + global,&res->run) ;
	global += 4 ;
	uint64_unpack_big(sa.s + global,&res->pid) ;
	
	stralloc_0(&res->sa) ;
	stralloc_free(&sa) ;
	
	return 1 ;	
	
	end :
		stralloc_free(&sa) ;
		return 0 ;
}

void ss_resolve_free(ss_resolve_t *res)
{
	stralloc_free(&res->sa) ;
}

void ss_resolve_init(ss_resolve_t *res)
{
	res->sa.len = 0 ;
	ss_resolve_add_string(res,"") ;
}

int ss_resolve_check(char const *src, char const *name)
{
	int r ;
	size_t srclen = strlen(src) ;
	char tmp[srclen + 1] ;
	memcpy(tmp,src,srclen) ;
	memcpy(tmp + srclen, SS_RESOLVE,SS_RESOLVE_LEN) ;
	tmp[srclen + SS_RESOLVE_LEN] = 0 ;
	r = dir_search(tmp,name,S_IFREG) ;
	if (!r || r < 0) return 0 ;
	else return 1 ;
}

int ss_resolve_setlognwrite(ss_resolve_t *sv, char const *dst)
{
	int r ;
	
	if (!sv->logger) return 1 ;
	
	s6_svstatus_t status = S6_SVSTATUS_ZERO ;
	
	ss_resolve_t res = RESOLVE_ZERO ;
	ss_resolve_init(&res) ;
	
	char *string = sv->sa.s ;
	size_t svlen = strlen(string + sv->name) ;
	char descrip[svlen + 7] ;
	memcpy(descrip,string + sv->name,svlen) ;
	memcpy(descrip + svlen," logger",7) ;
	descrip[svlen + 7] = 0 ;
	
	size_t runlen = strlen(string + sv->runat) ;
	char live[runlen + 4 + 1] ;
	memcpy(live,string + sv->runat,runlen) ;
	if (sv->type >= BUNDLE)
	{
		memcpy(live + runlen,"-log",4)  ;
	}else memcpy(live + runlen,"/log",4)  ;
	live[runlen + 4] = 0 ;
	
	res.name = ss_resolve_add_string(&res,string + sv->logger) ;
	res.description = ss_resolve_add_string(&res,descrip) ;
	res.logreal = ss_resolve_add_string(&res,string + sv->logreal) ;
	res.logassoc = ss_resolve_add_string(&res,string + sv->name) ;
	res.dstlog = ss_resolve_add_string(&res,string + sv->dstlog) ;
	res.live = ss_resolve_add_string(&res,string + sv->live) ;
	res.runat = ss_resolve_add_string(&res,live) ;
	//res.deps = ss_resolve_add_string(&res,string + sv->name) ;
	res.tree = ss_resolve_add_string(&res,string + sv->tree) ;
	res.treename = ss_resolve_add_string(&res,string + sv->treename) ;
	res.resolve = ss_resolve_add_string(&res,string + sv->resolve) ;
	res.src = ss_resolve_add_string(&res,string + sv->src) ;
	//res.ndeps = 1 ;
	res.type = sv->type ;
	ss_resolve_setflag(&res,SS_FLAGS_RELOAD,sv->reload) ;
	ss_resolve_setflag(&res,SS_FLAGS_DISEN,sv->disen) ;
	ss_resolve_setflag(&res,SS_FLAGS_DOWN,sv->down) ;
	ss_resolve_setflag(&res,SS_FLAGS_INIT,sv->init) ;
	ss_resolve_setflag(&res,SS_FLAGS_UNSUPERVISE,sv->unsupervise) ;
	
	r = s6_svc_ok(res.sa.s + res.runat) ;
	if (r < 0) { strerr_warnwu2sys("check ", res.sa.s + res.runat) ; goto err ; }
	if (!r)
	{	
		ss_resolve_setflag(&res,SS_FLAGS_PID,SS_FLAGS_FALSE) ;
		ss_resolve_setflag(&res,SS_FLAGS_RUN,SS_FLAGS_FALSE) ;
	}
	else
	{
		ss_resolve_setflag(&res,SS_FLAGS_RUN,SS_FLAGS_TRUE) ;
		if (!s6_svstatus_read(res.sa.s + res.runat,&status))
		{ 
			strerr_warnwu2sys("read status of: ",res.sa.s + res.name) ;
			goto err ;
		}
		ss_resolve_setflag(&res,SS_FLAGS_PID,(uint32_t)status.pid) ;
	}
	
	if (!ss_resolve_write(&res,dst,res.sa.s + res.name,SS_SIMPLE))
	{
		strerr_warnwu5sys("write resolve file: ",dst,SS_RESOLVE,"/",res.sa.s + res.name) ;
		goto err ;
	}
	ss_resolve_free(&res) ;
	return 1 ;
	err:
		ss_resolve_free(&res) ;
		return 0 ;
}

int ss_resolve_setnwrite(sv_alltype *services, ssexec_t *info, char const *dst)
{
	int r ;
	
	char ownerstr[256] ;
	size_t ownerlen = uid_fmt(ownerstr,info->owner) ;
	ownerstr[ownerlen] = 0 ;
	
	ss_resolve_t res = RESOLVE_ZERO ;
	ss_resolve_init(&res) ;
	
	char *name = keep.s + services->cname.name ;
	size_t namelen = strlen(name) ; 
	char logname[namelen + SS_LOG_SUFFIX_LEN + 1] ;
	char logreal[namelen + SS_LOG_SUFFIX_LEN + 1] ;
	char stmp[info->livetree.len + 1 + info->treename.len + SS_SVDIRS_LEN + 1 + namelen + SS_LOG_SUFFIX_LEN + 1] ;
	
	size_t livelen = info->live.len - 1 ; 
	char resolve[livelen + SS_STATE_LEN + 1 + ownerlen + 1 + info->treename.len + 1] ;
	memcpy(resolve,info->live.s,livelen) ;
	memcpy(resolve + livelen, SS_STATE,SS_STATE_LEN) ;
	resolve[livelen+ SS_STATE_LEN] = '/' ;
	memcpy(resolve + livelen + SS_STATE_LEN + 1,ownerstr,ownerlen) ;
	resolve[livelen + SS_STATE_LEN + 1 + ownerlen] = '/' ;
	memcpy(resolve + livelen + SS_STATE_LEN + 1 + ownerlen + 1,info->treename.s,info->treename.len) ;
	resolve[livelen + SS_STATE_LEN + 1 + ownerlen + 1 + info->treename.len] = 0 ;
	
	s6_svstatus_t status = S6_SVSTATUS_ZERO ;
			
	stralloc destlog = STRALLOC_ZERO ;
	stralloc namedeps = STRALLOC_ZERO ;
	stralloc final = STRALLOC_ZERO ;
	
	res.name = ss_resolve_add_string(&res,name) ;
	res.description = ss_resolve_add_string(&res,keep.s + services->cname.description) ;
	res.tree = ss_resolve_add_string(&res,info->tree.s) ;
	res.treename = ss_resolve_add_string(&res,info->treename.s) ;
	res.live = ss_resolve_add_string(&res,info->live.s) ;
	res.resolve = ss_resolve_add_string(&res,resolve) ;
	res.src = ss_resolve_add_string(&res,keep.s + services->src) ;
	if (services->type.classic_longrun.run.exec)
		res.exec_run = ss_resolve_add_string(&res,keep.s + services->type.classic_longrun.run.exec) ;
	if (services->type.classic_longrun.finish.exec)
		res.exec_finish = ss_resolve_add_string(&res,keep.s + services->type.classic_longrun.finish.exec) ;
	res.type = services->cname.itype ;
	res.ndeps = services->cname.nga ;
	ss_resolve_setflag(&res,SS_FLAGS_RELOAD,SS_FLAGS_TRUE) ;
	ss_resolve_setflag(&res,SS_FLAGS_DISEN,SS_FLAGS_TRUE) ;
	ss_resolve_setflag(&res,SS_FLAGS_UNSUPERVISE,SS_FLAGS_FALSE) ;
	if (services->flags[0])	ss_resolve_setflag(&res,SS_FLAGS_DOWN,SS_FLAGS_TRUE) ;

	if (res.type == CLASSIC)
	{
		
		memcpy(stmp,info->scandir.s,info->scandir.len) ;
		stmp[info->scandir.len] = '/' ;
		memcpy(stmp + info->scandir.len + 1,name,namelen) ;
		stmp[info->scandir.len + 1 + namelen] = 0 ;
		res.runat = ss_resolve_add_string(&res,stmp) ;
	}
	else if (res.type >= BUNDLE)
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
	r = s6_svc_ok(res.sa.s + res.runat) ;
	if (r < 0) { strerr_warnwu2sys("check ", res.sa.s + res.runat) ; goto err ; }
	if (!r)
	{	
		ss_resolve_setflag(&res,SS_FLAGS_INIT,SS_FLAGS_TRUE) ;
		ss_resolve_setflag(&res,SS_FLAGS_RELOAD,SS_FLAGS_FALSE) ;
		ss_resolve_setflag(&res,SS_FLAGS_PID,SS_FLAGS_FALSE) ;
		ss_resolve_setflag(&res,SS_FLAGS_RUN,SS_FLAGS_FALSE) ;
	}
	else
	{
		ss_resolve_setflag(&res,SS_FLAGS_INIT,SS_FLAGS_FALSE) ;
		ss_resolve_setflag(&res,SS_FLAGS_RELOAD,SS_FLAGS_TRUE) ;
		ss_resolve_setflag(&res,SS_FLAGS_RUN,SS_FLAGS_TRUE) ;
		if (!s6_svstatus_read(res.sa.s + res.runat,&status))
		{ 
			strerr_warnwu2sys("read status of: ",res.sa.s + res.name) ;
			goto err ;
		}
		ss_resolve_setflag(&res,SS_FLAGS_PID,(uint32_t)status.pid) ;
	}
	
	if (res.ndeps)
	{
		for (unsigned int i = 0; i < res.ndeps; i++)
		{
			if (!stralloc_obreplace(&namedeps,deps.s+genalloc_s(unsigned int,&gadeps)[services->cname.idga+i])) goto err ;
			r = insta_check(namedeps.s) ;
			if (!r) 
			{
				strerr_warnw2x(" invalid instance name: ",namedeps.s) ;
				goto err ;
			}
			if (r > 0)
			{
				if (!insta_splitname(&namedeps,namedeps.s,r,1))
				{
					strerr_warnwu2x("split copy name of instance: ",namedeps.s) ;
					goto err ;
				}
			}
			namedeps.len--;
			if (!stralloc_catb(&final,namedeps.s,namedeps.len)) { warnstralloc("ss_resolve_setnwrite") ; goto err ; }
			if (!stralloc_catb(&final," ",1)) { warnstralloc("ss_resolve_setnwrite") ; goto err ; }
		}
		final.len-- ;
		if (!stralloc_0(&final)){ warnstralloc("ss_resolve_setnwrite") ; goto err ; } 
		res.deps = ss_resolve_add_string(&res,final.s) ;
	}
	
	if (services->opts[0])
	{
		memcpy(logname,name,namelen) ;
		memcpy(logname + namelen,SS_LOG_SUFFIX,SS_LOG_SUFFIX_LEN) ;
		logname[namelen + SS_LOG_SUFFIX_LEN] = 0 ;
	
		memcpy(logreal,name,namelen) ;
		if (res.type == CLASSIC)
		{
			memcpy(logreal + namelen,"/log",SS_LOG_SUFFIX_LEN) ;
		}
		else memcpy(logreal + namelen,"-log",SS_LOG_SUFFIX_LEN) ;
		logreal[namelen + SS_LOG_SUFFIX_LEN] = 0 ;
		
		res.logger = ss_resolve_add_string(&res,logname) ;
		res.logreal = ss_resolve_add_string(&res,logreal) ;
		if (final.len) final.len--;
		if (!stralloc_catb(&final," ",1)) { warnstralloc("ss_resolve_setnwrite") ; goto err ; }
		if (!stralloc_cats(&final,res.sa.s + res.logger))	{ warnstralloc("ss_resolve_setnwrite") ; goto err ; }	
		if (!stralloc_0(&final)){ warnstralloc("ss_resolve_setnwrite") ; goto err ; } 
		res.deps = ss_resolve_add_string(&res,final.s) ;	
		if (res.type == CLASSIC) res.ndeps = 1 ;
		else if (res.type == LONGRUN) res.ndeps += 1 ;
		// destination of the logger
		if (!services->type.classic_longrun.log.destination)
		{	
			if(info->owner > 0)
			{	
				if (!stralloc_cats(&destlog,get_userhome(info->owner))) { warnstralloc("ss_resolve_setnwrite") ; goto err ; } 
				if (!stralloc_cats(&destlog,"/")) { warnstralloc("ss_resolve_setnwrite") ; goto err ; } 
				if (!stralloc_cats(&destlog,SS_LOGGER_USERDIR)) { warnstralloc("ss_resolve_setnwrite") ; goto err ; } 
				if (!stralloc_cats(&destlog,name)) { warnstralloc("ss_resolve_setnwrite") ; goto err ; } 
			}
			else
			{
				if (!stralloc_cats(&destlog,SS_LOGGER_SYSDIR)) { warnstralloc("ss_resolve_setnwrite") ; goto err ; } 
				if (!stralloc_cats(&destlog,name)) { warnstralloc("ss_resolve_setnwrite") ; goto err ; } 
			}
		}
		else
		{
			if (!stralloc_cats(&destlog,keep.s+services->type.classic_longrun.log.destination)) { warnstralloc("ss_resolve_setnwrite") ; goto err ; } 
		}
		if (!stralloc_0(&destlog)) { warnstralloc("ss_resolve_setnwrite") ; goto err ; } 
		
		res.dstlog = ss_resolve_add_string(&res,destlog.s) ;
		
		if (!ss_resolve_setlognwrite(&res,dst)) goto err ;
	}
	/** may on workdir so a copy with made to source, write it SIMPLE */
	if (!ss_resolve_write(&res,dst,res.sa.s + res.name,SS_SIMPLE))
	{
		strerr_warnwu5sys("write resolve file: ",dst,SS_RESOLVE,"/",res.sa.s + res.name) ;
		goto err ;
	}
	
	ss_resolve_free(&res) ;
	stralloc_free(&namedeps) ;
	stralloc_free(&final) ;
	stralloc_free(&destlog) ;
	return 1 ;
	
	err:
		ss_resolve_free(&res) ;
		stralloc_free(&namedeps) ;
		stralloc_free(&final) ;
		stralloc_free(&destlog) ;
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
	
void ss_resolve_setflag(ss_resolve_t *res,int flags,int flags_val)
{
	switch (flags)
	{
		case SS_FLAGS_RELOAD: res->reload = flags_val ; break ;
		case SS_FLAGS_DISEN: res->disen = flags_val ; break ;
		case SS_FLAGS_INIT: res->init = flags_val ; break ;
		case SS_FLAGS_UNSUPERVISE: res->unsupervise = flags_val ; break ;
		case SS_FLAGS_DOWN: res->down = flags_val ; break ;
		case SS_FLAGS_RUN: res->run = flags_val ; break ;
		case SS_FLAGS_PID: res->pid = flags_val ; break ;
		default: return ;
	}
}

int ss_resolve_copy(ss_resolve_t *dst,ss_resolve_t *res)
{
	size_t len = res->sa.len - 1 ;
	dst->salen = res->salen ;
	if (!stralloc_catb(&dst->sa,res->sa.s,len)) return 0 ;
	dst->name = res->name ;
	dst->description = res->description ;
	dst->logger = res->logger ;
	dst->logreal = res->logreal ;
	dst->logassoc = res->logassoc ;
	dst->dstlog = res->dstlog ;
	dst->deps = res->deps ;
	dst->src = res->src ;
	dst->live = res->live ;
	dst->runat = res->runat ;
	dst->tree = res->tree ;
	dst->treename = res->treename ;
	dst->resolve = res->resolve ;
	dst->exec_run = res->exec_run ;
	dst->exec_finish = res->exec_finish ;
	dst->type = res->type ;
	dst->ndeps = res->ndeps ;
	dst->reload = res->reload ;
	dst->disen = res->disen ;
	dst->init = res->init ;
	dst->unsupervise = res->unsupervise ;
	dst->down = res->down ;
	dst->run = res->run ;
	dst->pid = res->pid ;
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
	unsigned int i = 0 ;
	genalloc tmp = GENALLOC_ZERO ;
	
	char *name = res->sa.s + res->name ;
	char *deps = res->sa.s + res->deps ;
	if (!ss_resolve_cmp(tokeep,name))
	{
		if (!ss_resolve_append(tokeep,res)) goto err ;
	}
	
	if (res->ndeps)
	{
		if (!clean_val(&tmp,deps)) return 0 ;
		for (;i < genalloc_len(stralist,&tmp) ; i++)
		{
			
			ss_resolve_t dres = RESOLVE_ZERO ;
			if (!ss_resolve_check(src,gaistr(&tmp,i))) goto err ;
			if (!ss_resolve_read(&dres,src,gaistr(&tmp,i))) goto err ;
			if (dres.ndeps)
			{
				if (!ss_resolve_add_deps(tokeep,&dres,src)) goto err ;
			}
			if (!ss_resolve_cmp(tokeep,gaistr(&tmp,i)))
			{
				if (!ss_resolve_append(tokeep,&dres)) goto err ;
			}
			ss_resolve_free(&dres) ;
		}
	}
	genalloc_deepfree(stralist,&tmp,stra_free) ;
	return 1 ;
	err:
		genalloc_deepfree(stralist,&tmp,stra_free) ;
		return 0 ;
}

int ss_resolve_add_rdeps(genalloc *tokeep, ss_resolve_t *res, char const *src)
{
	
	int type ;
	genalloc tmp = GENALLOC_ZERO ;
	genalloc nsv = GENALLOC_ZERO ;
	
	char *name = res->sa.s + res->name ;
	size_t srclen = strlen(src) ;
	char s[srclen + SS_RESOLVE_LEN + 1] ;
	memcpy(s,src,srclen) ;
	memcpy(s + srclen,SS_RESOLVE,SS_RESOLVE_LEN) ;
	s[srclen + SS_RESOLVE_LEN] = 0 ;
	
	if (res->type == CLASSIC) type = 0 ;
	else type = 1 ;
	
	if (!dir_get(&nsv,s,SS_MASTER+1,S_IFREG)) goto err ;
	
	if (!ss_resolve_cmp(tokeep,name))
	{
		if (!ss_resolve_append(tokeep,res)) goto err ;
	}
	
	for (unsigned int i = 0 ; i < genalloc_len(stralist,&nsv) ; i++)
	{
		int dtype = 0 ;
		genalloc_deepfree(stralist,&tmp,stra_free) ;
		ss_resolve_t dres = RESOLVE_ZERO ;
		char *dname = gaistr(&nsv,i) ;
		if (obstr_equal(name,dname)) { ss_resolve_free(&dres) ; continue ; }
		if (!ss_resolve_check(src,dname)) goto err ;
		if (!ss_resolve_read(&dres,src,dname)) goto err ;
		if (dres.type == CLASSIC) dtype = 0 ;
		else dtype = 1 ;
		if (dtype != type || !dres.disen){ ss_resolve_free(&dres) ; continue ; }
		if (!ss_resolve_cmp(tokeep,dname))
		{
			if (dres.ndeps || (dres.type == BUNDLE && dres.ndeps) || (res->type == BUNDLE && res->ndeps))
			{
				if (!clean_val(&tmp,dres.sa.s + dres.deps)) goto err ;
				for (unsigned int j = 0 ; j < genalloc_len(stralist,&tmp) ; j++)
				{
					if (obstr_equal(name,gaistr(&tmp,j)) || res->type == BUNDLE)
					{
							if (!ss_resolve_append(tokeep,&dres)) goto err ;
							if (!ss_resolve_add_rdeps(tokeep,&dres,src)) goto err ;
							ss_resolve_free(&dres) ;
							break ;
					}
				}
			}
		}
		ss_resolve_free(&dres) ;
	}
	
	genalloc_deepfree(stralist,&tmp,stra_free) ;
	genalloc_deepfree(stralist,&nsv,stra_free) ;
	return 1 ;
	err:
		genalloc_deepfree(stralist,&tmp,stra_free) ;
		genalloc_deepfree(stralist,&nsv,stra_free) ;
		return 0 ;
}

int ss_resolve_add_logger(genalloc *ga,char const *src)
{
	genalloc gatmp = GENALLOC_ZERO ;

	for (unsigned int i = 0 ; i < genalloc_len(ss_resolve_t,ga) ; i++) 
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
	char ownerstr[256] ;
	size_t ownerlen = uid_fmt(ownerstr,info->owner) ;
	ownerstr[ownerlen] = 0 ;
	gid_t gidowner ;
	if (!yourgid(&gidowner,info->owner)) return 0 ;
	stralloc sares = STRALLOC_ZERO ;
	stralloc ressrc = STRALLOC_ZERO ;
	stralloc resdst = STRALLOC_ZERO ;
	
	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_SRC)) goto err ;
	
	if (!stralloc_copy(&ressrc,&sares)) goto err ; 
	ressrc.len--;
	if (!stralloc_cats(&ressrc,SS_RESOLVE)) goto err ;
	if (!stralloc_0(&ressrc)) goto err ;
		
	if (!ss_resolve_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_LIVE)) goto err ;
	r = scan_mode(sares.s,S_IFDIR) ;
	if (r < 0) goto err ;
	if (!r)
	{
		r = dir_create_under(sares.s,SS_RESOLVE,0700) ;
		if (!r) goto err ;
	
		if (!stralloc_copy(&resdst,&sares)) goto err ;
		resdst.len--;
		if (!stralloc_cats(&resdst,SS_RESOLVE)) goto err ;
		if (!stralloc_0(&resdst)) goto err ;
		if (chown(resdst.s,info->owner,gidowner) < 0) goto err ;
		
		if (!hiercopy(ressrc.s,resdst.s)) goto err ;
	}
	
	stralloc_free(&ressrc) ;
	stralloc_free(&resdst) ;
	stralloc_free(&sares) ;
	
	return 1 ;
	err:
		stralloc_free(&ressrc) ;
		stralloc_free(&resdst) ;
		stralloc_free(&sares) ;
		return 0 ;
}
