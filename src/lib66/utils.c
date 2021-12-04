/*
 * utils.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
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
#include <stdlib.h>

#include <oblibs/log.h>
#include <oblibs/files.h>
#include <oblibs/sastr.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>

#include <skalibs/genalloc.h>
#include <skalibs/stralloc.h>
#include <skalibs/types.h>
#include <skalibs/djbunix.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/parser.h>

#include <s6/supervise.h>

sv_alltype const sv_alltype_zero = SV_ALLTYPE_ZERO ;
sv_name_t const sv_name_zero = SV_NAME_ZERO ;
keynocheck const keynocheck_zero = KEYNOCHECK_ZERO ;


/** this following function come from Laurent Bercot
 * author of s6 library all rights reserved on this author
 * It was just modified a little bit to be able to scan
 * a scandir directory instead of a service directory */
int scandir_ok (char const *dir)
{
    log_flow() ;

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
    log_flow() ;

    size_t idlen = strlen(signal) ;
    char data[idlen + 1] ;
    size_t datalen = 0 ;

    for (; datalen < idlen ; datalen++)
        data[datalen] = signal[datalen] ;

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
    log_flow() ;

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
    log_flow() ;

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
    log_flow() ;

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
    log_flow() ;

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
    log_flow() ;

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
    log_flow() ;

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
    log_flow() ;

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
    log_flow() ;

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
    log_flow() ;

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
        if (!stralloc_cats(base,user_home)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
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
    log_flow() ;

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

int module_in_cmdline(genalloc *gares, resolve_service_t *res, char const *dir)
{
    log_flow() ;

    int e = 0 ;
    stralloc tmp = STRALLOC_ZERO ;
    size_t pos = 0 ;
    resolve_wrapper_t_ref wres = resolve_set_struct(SERVICE_STRUCT, res) ;

    if (!resolve_append(gares,wres)) goto err ;

    if (res->contents)
    {
        if (!sastr_clean_string(&tmp,res->sa.s + res->contents))
            goto err ;
    }
    for (; pos < tmp.len ; pos += strlen(tmp.s + pos) + 1)
    {
        char *name = tmp.s + pos ;
        if (!resolve_check(dir,name)) goto err ;
        if (!resolve_read(wres,dir,name)) goto err ;
        if (res->type == TYPE_CLASSIC)
            if (resolve_search(gares,name, SERVICE_STRUCT) < 0)
                if (!resolve_append(gares,wres)) goto err ;
    }

    e = 1 ;

    err:
        free(wres) ;
        stralloc_free(&tmp) ;
        return e ;
}

int module_search_service(char const *src, genalloc *gares, char const *name,uint8_t *found, char module_name[256])
{
    log_flow() ;

    int e = 0 ;
    size_t srclen = strlen(src), pos = 0, deps = 0 ;
    stralloc list = STRALLOC_ZERO ;
    stralloc tmp = STRALLOC_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(SERVICE_STRUCT, &res) ;
    char const *exclude[2] = { SS_MASTER + 1, 0 } ;

    char t[srclen + SS_RESOLVE_LEN + 1] ;
    auto_strings(t,src,SS_RESOLVE) ;

    if (!sastr_dir_get(&list,t,exclude,S_IFREG)) goto err ;

    for (;pos < list.len ; pos += strlen(list.s + pos) + 1)
    {
        char *dname = list.s + pos ;
        if (!resolve_read(wres,src,dname)) goto err ;
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
    for(pos = 0 ; pos < genalloc_len(resolve_service_t,gares) ; pos++)
    {
        resolve_service_t_ref pres = &genalloc_s(resolve_service_t, gares)[pos] ;
        char *str = pres->sa.s ;
        char *name = str + pres->name ;
        if (!strcmp(name,module_name)) {
            (*found) = 0 ;
            break ;
        }
    }

    e = 1 ;

    err:
        stralloc_free(&list) ;
        stralloc_free(&tmp) ;
        resolve_free(wres) ;
        return e ;
}

/* @sdir -> service dir
 * @mdir -> module dir */
int module_path(stralloc *sdir, stralloc *mdir, char const *sv,char const *frontend_src, uid_t owner)
{
    log_flow() ;

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
        dest = frontend_src ;
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
        if (!auto_stra(mdir,src,sainsta.s)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
        r = scan_mode(mdir->s,S_IFDIR) ;
        if (!r || r == -1)
        {
            mdir->len = 0 ;
            src = SS_MODULE_SYSDIR ;
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

int sa_pointo(stralloc *sa, ssexec_t *info, int type, unsigned int where)
{
    log_flow() ;

    sa->len = 0 ;

    char ownerstr[UID_FMT] ;
    size_t ownerlen = uid_fmt(ownerstr,info->owner) ;
    ownerstr[ownerlen] = 0 ;

    if (where == SS_RESOLVE_STATE) {

        if (!auto_stra(sa, info->live.s, SS_STATE + 1, "/", ownerstr, "/", info->treename.s))
            goto err ;

    } else if (where == SS_RESOLVE_SRC) {

        if (!auto_stra(sa, info->tree.s, SS_SVDIRS))
            goto err ;

    } else if (where == SS_RESOLVE_BACK) {

        if (!auto_stra(sa, info->base.s, SS_SYSTEM, SS_BACKUP, "/", info->treename.s))
            goto err ;

    } else if (where == SS_RESOLVE_LIVE) {

        if (!auto_stra(sa, info->live.s, SS_STATE + 1, "/", ownerstr, "/", info->treename.s, SS_SVDIRS))
            goto err ;
    }

    if (type >= 0 && where) {

        if (type == TYPE_CLASSIC) {

            if (!auto_stra(sa, SS_SVC))
                goto err ;

        } else if (!auto_stra(sa, SS_DB))
            goto err ;
    }

    return 1 ;

    err:
        return 0 ;
}

int create_live(ssexec_t *info)
{
    log_flow() ;

    int r ;
    gid_t gidowner ;
    if (!yourgid(&gidowner,info->owner)) return 0 ;
    stralloc sares = STRALLOC_ZERO ;
    stralloc ressrc = STRALLOC_ZERO ;

    if (!sa_pointo(&ressrc,info,SS_NOTYPE,SS_RESOLVE_SRC)) goto err ;

    if (!sa_pointo(&sares,info,SS_NOTYPE,SS_RESOLVE_STATE)) goto err ;
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
/*
 *
 *
 * Not used
 *
 *
 *

int sort_byfile_first(stralloc *sort, char const *src)
{
    log_flow() ;

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
*/
