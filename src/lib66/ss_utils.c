/* 
 * ss_utils.c
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

#include <66/utils.h>

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/files.h>
#include <oblibs/sastr.h>
#include <oblibs/string.h>

#include <skalibs/genalloc.h>
#include <skalibs/stralloc.h>
#include <skalibs/types.h>
#include <skalibs/djbunix.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/resolve.h>
#include <66/parser.h>

#include <s6/s6-supervise.h>

#define DATASIZE 64 //scandir_send_signal

sv_alltype const sv_alltype_zero = SV_ALLTYPE_ZERO ;
sv_name_t const sv_name_zero = SV_NAME_ZERO ;
keynocheck const keynocheck_zero = KEYNOCHECK_ZERO ;
ss_resolve_t const ss_resolve_zero = RESOLVE_ZERO ;

/** this following function come from Laurent Bercot 
 * author of s6 library all rights reserved on this author
 * It was just modified a little bit to be able to scan
 * a scandir directory instead of a service directory */
int scandir_ok (char const *dir)
{
	size_t dirlen = strlen(dir) ;
	int fd ;
	char fn[dirlen + 1 + strlen(S6_SVSCAN_CTLDIR) + 9 + 1] ;
	memcpy(fn, dir, dirlen) ;
	fn[dirlen] = '/' ;
	memcpy(fn + dirlen + 1, S6_SVSCAN_CTLDIR, strlen(S6_SVSCAN_CTLDIR)) ;
	memcpy(fn + dirlen + 1 + strlen(S6_SVSCAN_CTLDIR), "/control", 9) ;
	fn[dirlen + 1 + strlen(S6_SVSCAN_CTLDIR) + 9] = 0 ;
	fd = open_write(fn) ;
	if (fd < 0)
	{
		if ((errno == ENXIO) || (errno == ENOENT)) return 0 ;
		else return -1 ;
	}
	fd_close(fd) ;
	return 1 ;
}

int scandir_send_signal(char const *scandir,char const *signal)
{
	char data[DATASIZE] ;
	size_t datalen = 0 ;
	
	size_t id = strlen(signal) ;
	while (datalen < id)
	{
		data[datalen] = signal[datalen] ;
		datalen++ ;
	}	
	if (datalen >= DATASIZE)
		log_warn_return(LOG_EXIT_ZERO,"too many command to send to: ",scandir) ;
	
	switch (s6_svc_writectl(scandir, S6_SVSCAN_CTLDIR, data, datalen))
	{
		case -1: log_warnusys("control: ", scandir) ; 
				return 0 ;
		case -2: log_warnsys("something is wrong with the ", scandir, "/" S6_SVSCAN_CTLDIR " directory. errno reported") ; 
				return 0 ;
		case 0: log_warnu("control: ", scandir, ": supervisor not listening") ;
				return 0 ;
	}

	return 1 ;
}

char const *get_userhome(uid_t myuid)
{
	char const *user_home = NULL ;
	struct passwd *st = getpwuid(myuid) ;
	int e = errno ;
	errno = 0 ;
	if (!st)
	{
		if (!errno) errno = ESRCH ;
		return 0 ;
	}
	user_home = st->pw_dir ;

	if (!user_home) return 0 ;
	errno = e ;
	return user_home ;
}

int youruid(uid_t *passto,char const *owner)
{
	int e ;
	e = errno ;
	errno = 0 ;
	struct passwd *st ;
	st = getpwnam(owner) ;
	if (!st)
	{
		if (!errno) errno = ESRCH ;
		return 0 ;
	}
	*passto = st->pw_uid ;
	errno = e ;
	return 1 ;
}

int yourgid(gid_t *passto,uid_t owner)
{
	int e ;
	e = errno ;
	errno = 0 ;
	struct passwd *st ;
	st = getpwuid(owner) ;
	if (!st)
	{
		if (!errno) errno = ESRCH ;
		return 0 ;
	}
	*passto = st->pw_gid ;
	errno = e ;
	return 1 ;
}

int set_livedir(stralloc *live)
{
	
	if (live->len)
	{
		if (live->s[0] != '/') return -1 ;
		if (live->s[live->len - 2] != '/')
		{
			live->len-- ;
			if (!stralloc_cats(live,"/")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
			if (!stralloc_0(live)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		}
	}
	else
	{
		if (!stralloc_cats(live,SS_LIVE)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		if (!stralloc_0(live)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	}
	live->len--;
	return 1 ;
}

int set_livescan(stralloc *scandir,uid_t owner)
{
	int r ;
	char ownerpack[UID_FMT] ;
		
	r = set_livedir(scandir) ;
	if (r < 0) return -1 ;
	if (!r) return 0 ;
		
	size_t ownerlen = uid_fmt(ownerpack,owner) ;
	ownerpack[ownerlen] = 0 ;
	
	if (!stralloc_cats(scandir,SS_SCANDIR "/")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	if (!stralloc_cats(scandir,ownerpack)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	if (!stralloc_0(scandir)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	scandir->len--;
	return 1 ;
}

int set_livetree(stralloc *livetree,uid_t owner)
{
	int r ;
	char ownerpack[UID_FMT] ;

	r = set_livedir(livetree) ;
	if (r < 0) return -1 ;
	if (!r) return 0 ;
		
	size_t ownerlen = uid_fmt(ownerpack,owner) ;
	ownerpack[ownerlen] = 0 ;
	
	if (!stralloc_cats(livetree,SS_TREE "/")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	if (!stralloc_cats(livetree,ownerpack)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	if (!stralloc_0(livetree)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	livetree->len--;
	return 1 ;
}

int set_livestate(stralloc *livestate,uid_t owner)
{
	int r ;
	char ownerpack[UID_FMT] ;
		
	r = set_livedir(livestate) ;
	if (r < 0) return -1 ;
	if (!r) return 0 ;
		
	size_t ownerlen = uid_fmt(ownerpack,owner) ;
	ownerpack[ownerlen] = 0 ;
	
	if (!stralloc_cats(livestate,SS_STATE + 1)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	if (!stralloc_cats(livestate,"/")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	if (!stralloc_cats(livestate,ownerpack)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	if (!stralloc_0(livestate)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	livestate->len--;
	return 1 ;
}

int set_ownerhome(stralloc *base,uid_t owner)
{
	char const *user_home = 0 ;
	int e = errno ;
	struct passwd *st = getpwuid(owner) ;
	errno = 0 ;
	if (!st)
	{
		if (!errno) errno = ESRCH ;
		return 0 ;
	}
	user_home = st->pw_dir ;
	errno = e ;
	if (!user_home) return 0 ;
	
	if (!stralloc_cats(base,user_home)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	if (!stralloc_cats(base,"/")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	if (!stralloc_0(base)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	base->len--;
	return 1 ;
}

int set_ownersysdir(stralloc *base, uid_t owner)
{
	char const *user_home = NULL ;
	int e = errno ;
	struct passwd *st = getpwuid(owner) ;
	errno = 0 ;
	if (!st)
	{
		if (!errno) errno = ESRCH ;
		return 0 ;
	}
	user_home = st->pw_dir ;
	errno = e ;
	if (user_home == NULL) return 0 ;
	
	if(owner > 0){
		if (!stralloc_cats(base,user_home))	log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		if (!stralloc_cats(base,"/")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		if (!stralloc_cats(base,SS_USER_DIR)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		if (!stralloc_0(base)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	}
	else
	{
		if (!stralloc_cats(base,SS_SYSTEM_DIR)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
		if (!stralloc_0(base)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	}
	base->len--;
	return 1 ;
}

int read_svfile(stralloc *sasv,char const *name,char const *src)
{
	int r ; 
	size_t srclen = strlen(src) ;
	size_t namelen = strlen(name) ;
	
	char svtmp[srclen + namelen + 1] ;
	memcpy(svtmp,src,srclen) ;
	memcpy(svtmp + srclen, name, namelen) ;
	svtmp[srclen + namelen] = 0 ;
	
	size_t filesize=file_get_size(svtmp) ;
	if (!filesize)
		log_warn_return(LOG_EXIT_LESSONE,svtmp," is empty") ;
	
	r = openreadfileclose(svtmp,sasv,filesize) ;
	if(!r)
		log_warnusys_return(LOG_EXIT_ZERO,"open ", svtmp) ;

	/** ensure that we have an empty line at the end of the string*/
	if (!stralloc_cats(sasv,"\n")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	if (!stralloc_0(sasv)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
	
	return 1 ;
}

int module_in_cmdline(genalloc *gares, ss_resolve_t *res, char const *dir)
{
	stralloc tmp = STRALLOC_ZERO ;
	size_t pos = 0 ;
	
	if (!ss_resolve_append(gares,res)) goto err ;
	
	if (res->contents)
	{
		if (!sastr_clean_string(&tmp,res->sa.s + res->contents))
			goto err ;
	}
	for (; pos < tmp.len ; pos += strlen(tmp.s + pos) + 1)
	{
		char *name = tmp.s + pos ;
		if (!ss_resolve_check(dir,name)) goto err ;
		if (!ss_resolve_read(res,dir,name)) goto err ;
		if (res->type == TYPE_CLASSIC)
			if (ss_resolve_search(gares,name) < 0)
				if (!ss_resolve_append(gares,res)) goto err ;
	}
	stralloc_free(&tmp) ;
	return 1 ;
	err:
		stralloc_free(&tmp) ;
		return 0 ;
}

int module_search_service(char const *src, genalloc *gares, char const *name,uint8_t *found, char module_name[255])
{
	size_t srclen = strlen(src), pos = 0, deps = 0 ;
	stralloc list = STRALLOC_ZERO ;
	stralloc tmp = STRALLOC_ZERO ;
	ss_resolve_t res = RESOLVE_ZERO ;

	char t[srclen + SS_RESOLVE_LEN + 1] ;
	auto_strings(t,src,SS_RESOLVE) ;

	if (!sastr_dir_get(&list,t,SS_MASTER+1,S_IFREG)) goto err ;

	for (;pos < list.len ; pos += strlen(list.s + pos) + 1)
	{
		char *dname = list.s + pos ;
		if (!ss_resolve_read(&res,src,dname)) goto err ;
		if (res.type == TYPE_MODULE && res.contents)
		{
			if (!sastr_clean_string(&tmp,res.sa.s + res.contents)) goto err ;
			for (deps = 0 ; deps < tmp.len ; deps += strlen(tmp.s + deps) + 1)
			{
				if (!strcmp(name,tmp.s + deps))
				{
					(*found)++ ;
					if (strlen(dname) > 255) log_1_warn_return(LOG_EXIT_ZERO,"module name too long") ;
					auto_strings(module_name,dname) ;
					goto end ;
				}
			}
		}
	}
	end:
	/** search if the service is on the commandline
	 * if not we crash */
	for(pos = 0 ; pos < genalloc_len(ss_resolve_t,gares) ; pos++)
	{
		ss_resolve_t_ref pres = &genalloc_s(ss_resolve_t,gares)[pos] ;
		char *string = pres->sa.s ;
		char  *name = string + pres->name ;
		if (!strcmp(name,module_name)) {
			(*found) = 0 ;
			break ;
		}
	}

	stralloc_free(&list) ;
	stralloc_free(&tmp) ;
	ss_resolve_free(&res) ;
	return 1 ;
	err:
		stralloc_free(&list) ;
		stralloc_free(&tmp) ;
		ss_resolve_free(&res) ;
		return 0 ;
}

