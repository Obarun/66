/*
 * service.c
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

#include <string.h>
#include <stdint.h>
#include <stdlib.h>//free
#include <unistd.h>//getuid
#include <sys/stat.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/sastr.h>
#include <oblibs/files.h>

#include <skalibs/djbunix.h>//openreadnclose
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/cdb.h>
#include <skalibs/cdbmake.h>
#include <skalibs/types.h>

#include <66/utils.h>
#include <66/parser.h>
#include <66/enum.h>
#include <66/constants.h>
#include <66/resolve.h>
#include <66/ssexec.h>
#include <66/service.h>
#include <66/state.h>

/** @Return 0 if not found
 * @Return 1 if found
 * @Return 2 if found but marked disabled
 * @Return -1 system error */
int service_isenabled(char const *sv)
{

    log_flow() ;

    stralloc sa = STRALLOC_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(SERVICE_STRUCT, &res) ;
    size_t newlen = 0, pos = 0 ;
    int e = -1 ;
    char const *exclude[3] = { SS_BACKUP + 1, SS_RESOLVE + 1, 0 } ;

    if (!set_ownersysdir(&sa, getuid())) {

        log_warnusys("set owner directory") ;
        stralloc_free(&sa) ;
        return 0 ;
    }

    char tmp[sa.len + SS_SYSTEM_LEN + 2] ;
    auto_strings(tmp, sa.s, SS_SYSTEM) ;

    // no tree exist yet
    if (!scan_mode(tmp, S_IFDIR))
        goto empty ;

    auto_strings(tmp, sa.s, SS_SYSTEM, "/") ;

    newlen = sa.len + SS_SYSTEM_LEN + 1 ;
    sa.len = 0 ;

    if (!sastr_dir_get(&sa, tmp, exclude, S_IFDIR)) {

        log_warnu("get list of trees from: ", tmp) ;
        goto freed ;
    }

    FOREACH_SASTR(&sa, pos) {

        char *treename = sa.s + pos ;

        char trees[newlen + strlen(treename) + SS_SVDIRS_LEN + 1] ;
        auto_strings(trees, tmp, treename, SS_SVDIRS) ;

        if (resolve_check(trees, sv)) {

            if (!resolve_read(wres, trees, sv)) {

                log_warnu("read resolve file: ", trees, "/", sv) ;
                goto freed ;
            }

            if (res.disen) {

                log_trace(sv, " enabled at tree: ", treename) ;

                e = 1 ;
                goto freed ;

            } else {

                e = 2 ;
                goto freed ;
            }
        }
    }
    empty:
        e = 0 ;
    freed:
        stralloc_free(&sa) ;
        resolve_free(wres) ;
        return e ;
}

/** @Return 0 if not found
 * @Return 1 if found
 * @Return 2 if found but marked disabled
 * @Return -1 system error */
int service_isenabledat(stralloc *tree, char const *sv)
{

    log_flow() ;

    stralloc sa = STRALLOC_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(SERVICE_STRUCT, &res) ;
    size_t newlen = 0, pos = 0 ;
    int e = -1 ;
    char const *exclude[3] = { SS_BACKUP + 1, SS_RESOLVE + 1, 0 } ;

    if (!set_ownersysdir(&sa, getuid())) {

        log_warnusys("set owner directory") ;
        stralloc_free(&sa) ;
        return 0 ;
    }

    char tmp[sa.len + SS_SYSTEM_LEN + 2] ;
    auto_strings(tmp, sa.s, SS_SYSTEM) ;

    // no tree exist yet
    if (!scan_mode(tmp, S_IFDIR))
        goto empty ;

    auto_strings(tmp, sa.s, SS_SYSTEM, "/") ;

    newlen = sa.len + SS_SYSTEM_LEN + 1 ;
    sa.len = 0 ;

    if (!sastr_dir_get(&sa, tmp, exclude, S_IFDIR)) {

        log_warnu("get list of trees from: ", tmp) ;
        goto freed ;
    }

    FOREACH_SASTR(&sa, pos) {

        char *treename = sa.s + pos ;

        char trees[newlen + strlen(treename) + SS_SVDIRS_LEN + 1] ;
        auto_strings(trees, tmp, treename, SS_SVDIRS) ;

        if (resolve_check(trees, sv)) {

            if (!resolve_read(wres, trees, sv)) {

                log_warnu("read resolve file: ", trees, "/", sv) ;
                goto freed ;
            }

            if (res.disen) {

                log_trace(sv, " enabled at tree: ", treename) ;
                e = 1 ;

            } else {

                log_trace(sv, " disabled at tree: ", treename) ;
                e = 2 ;
            }

            if (!auto_stra(tree, treename)) {
                e = -1 ;
                goto freed ;
            }
            goto freed ;
        }
    }
    empty:
        e = 0 ;
    freed:
        stralloc_free(&sa) ;
        resolve_free(wres) ;
        return e ;
}

int service_frontend_src(stralloc *sasrc, char const *name, char const *src)
{
    log_flow() ;

    int  insta, equal = 0, e = -1, r = 0, found = 0 ;
    stralloc sa = STRALLOC_ZERO ;
    size_t pos = 0, dpos = 0 ;

    char const *exclude[1] = { 0 } ;

    if (!sastr_dir_get_recursive(&sa, src, exclude, S_IFREG|S_IFDIR, 1)) {
        stralloc_free(&sa) ;
        goto err ;
    }
    size_t len = sort.len ;
    char tmp[len + 1] ;
    sastr_to_char(tmp, &sa)

    for (; pos < len ; pos += strlen(tmp + pos) + 1) {

        sa.len = 0 ;
        char *dname = tmp + pos ;

        struct stat st ;
        /** use stat instead of lstat to accept symlink */
        if (stat(dname,&st) == -1)
            goto err ;

        size_t dnamelen = strlen(dname) ;
        char bname[dnamelen + 1] ;

        if (!ob_basename(bname,dname))
            goto err ;

        equal = strcmp(name, bname) ;

        insta = instance_check(bname) ;

        if (insta > 0) {

            if (!instance_splitname(&sa, bname, insta, SS_INSTANCE_TEMPLATE))
                goto err ;

            equal = strcmp(sa.s,bname) ;
        }

        if (!equal) {

            found = 1 ;

            if (S_ISREG(st.st_mode)) {

                if (sastr_cmp(sasrc, dname) == -1)
                    if (!sastr_add_string(sasrc, dname))
                        goto err ;

                break ;

            } else if (S_ISDIR(st.st_mode)) {

                if (!insta) {
                    log_warn("invalid name format for: ", dname," -- directory instance name are case reserved") ;
                    goto err ;
                }

                int rd = service_endof_dir(dname, bname) ;
                if (rd == -1)
                    goto err ;

                if (!rd) {
                    /** user ask to deal which each frontend file
                     * inside the directory */
                    sa.len = 0 ;

                    if (!sastr_dir_get_recursive(&sa, dname, exclude, S_IFREG|S_IFDIR, 0))
                        goto err ;

                    /** directory may be empty. */
                    if (!sa.len)
                        found = 0 ;

                    dpos = 0 ;
                    FOREACH_SASTR(&sa, dpos) {

                        r = service_frontend_src(sasrc, sa.s + dpos, dname) ;
                        if (r < 0)
                            /** system error */
                            goto err ;
                        if (!r)
                            /** nothing found on empty sub-directory */
                            found = 0 ;
                    }

                    break ;

                }
            }
        }
    }

    e = found ;

    err:
        stralloc_free(&sa) ;
        return e ;
}

int service_frontend_path(stralloc *sasrc,char const *sv, uid_t owner,char const *directory_forced)
{
    log_flow() ;

    int r ;
    char const *src = 0 ;
    int err = -1 ;
    stralloc home = STRALLOC_ZERO ;
    if (directory_forced)
    {
        if (!service_cmp_basedir(directory_forced)) { log_warn("invalid base service directory: ",directory_forced) ; goto err ; }
        src = directory_forced ;
        r = service_frontend_src(sasrc,sv,src) ;
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

        r = service_frontend_src(sasrc,sv,src) ;
        if (r == -1){ log_warnusys("parse source directory: ",src) ; goto err ; }
        if (!r)
        {
            src = SS_SERVICE_ADMDIR ;
            r = service_frontend_src(sasrc,sv,src) ;
            if (r == -1) { log_warnusys("parse source directory: ",src) ; goto err ; }
            if (!r)
            {
                src = SS_SERVICE_SYSDIR ;
                r = service_frontend_src(sasrc,sv,src) ;
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

int service_endof_dir(char const *dir, char const *name)
{
    log_flow() ;

    size_t dirlen = strlen(dir) ;
    size_t namelen = strlen(name) ;
    char t[dirlen + 1 + namelen + 1] ;
    auto_strings(t, dir, "/", name) ;

    int r = scan_mode(t,S_IFREG) ;

    if (!ob_basename(t,dir))
        return -1 ;

    if (!strcmp(t,name) && (r > 0))
        return 1 ;

    return 0 ;
}

int service_cmp_basedir(char const *dir)
{
    log_flow() ;

    /** dir can be 0, so nothing to do */
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

/**
 *
 * RESOLVE API
 *
 * */

resolve_service_field_table_t resolve_service_field_table[] = {

    [SERVICE_ENUM_NAME] = { .field = "name" },
    [SERVICE_ENUM_DESCRIPTION] = { .field = "description" },
    [SERVICE_ENUM_VERSION] = { .field = "version" },
    [SERVICE_ENUM_LOGGER] = { .field = "logger" },
    [SERVICE_ENUM_LOGREAL] = { .field = "logreal" },
    [SERVICE_ENUM_LOGASSOC] = { .field = "logassoc" },
    [SERVICE_ENUM_DSTLOG] = { .field = "dstlog" },
    [SERVICE_ENUM_DEPS] = { .field = "deps" },
    [SERVICE_ENUM_OPTSDEPS] = { .field = "optsdeps" },
    [SERVICE_ENUM_EXTDEPS] = { .field = "extdeps" },
    [SERVICE_ENUM_CONTENTS] = { .field = "contents" },
    [SERVICE_ENUM_SRC] = { .field = "src" },
    [SERVICE_ENUM_SRCONF] = { .field = "srconf" },
    [SERVICE_ENUM_LIVE] = { .field = "live" },
    [SERVICE_ENUM_RUNAT] = { .field = "runat" },
    [SERVICE_ENUM_TREE] = { .field = "tree" },
    [SERVICE_ENUM_TREENAME] = { .field = "treename" },
    [SERVICE_ENUM_STATE] = { .field = "state" },
    [SERVICE_ENUM_EXEC_RUN] = { .field = "exec_run" },
    [SERVICE_ENUM_EXEC_LOG_RUN] = { .field = "exec_log_run" },
    [SERVICE_ENUM_REAL_EXEC_RUN] = { .field = "real_exec_run" },
    [SERVICE_ENUM_REAL_EXEC_LOG_RUN] = { .field = "real_exec_log_run" },
    [SERVICE_ENUM_EXEC_FINISH] = { .field = "exec_finish" },
    [SERVICE_ENUM_REAL_EXEC_FINISH] = { .field = "real_exec_finish" },
    [SERVICE_ENUM_TYPE] = { .field = "type" },
    [SERVICE_ENUM_NDEPS] = { .field = "ndeps" },
    [SERVICE_ENUM_NOPTSDEPS] = { .field = "noptsdeps" },
    [SERVICE_ENUM_NEXTDEPS] = { .field = "nextdeps" },
    [SERVICE_ENUM_NCONTENTS] = { .field = "ncontents" },
    [SERVICE_ENUM_DOWN] = { .field = "down" },
    [SERVICE_ENUM_DISEN] = { .field = "disen" },
    [SERVICE_ENUM_ENDOFKEY] = { .field = 0 }
} ;

int service_read_cdb(cdb *c, resolve_service_t *res)
{
    log_flow() ;

    stralloc tmp = STRALLOC_ZERO ;
    resolve_wrapper_t_ref wres ;
    uint32_t x ;

    wres = resolve_set_struct(SERVICE_STRUCT, res) ;

    /* name */
    resolve_find_cdb(&tmp,c,"name") ;
    res->name = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* description */
    resolve_find_cdb(&tmp,c,"description") ;
    res->description = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* version */
    resolve_find_cdb(&tmp,c,"version") ;
    res->version = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* logger */
    resolve_find_cdb(&tmp,c,"logger") ;
    res->logger = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* logreal */
    resolve_find_cdb(&tmp,c,"logreal") ;
    res->logreal = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* logassoc */
    resolve_find_cdb(&tmp,c,"logassoc") ;
    res->logassoc = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* dstlog */
    resolve_find_cdb(&tmp,c,"dstlog") ;
    res->dstlog = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* deps */
    resolve_find_cdb(&tmp,c,"deps") ;
    res->deps = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* optsdeps */
    resolve_find_cdb(&tmp,c,"optsdeps") ;
    res->optsdeps = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* extdeps */
    resolve_find_cdb(&tmp,c,"extdeps") ;
    res->extdeps = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* contents */
    resolve_find_cdb(&tmp,c,"contents") ;
    res->contents = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* src */
    resolve_find_cdb(&tmp,c,"src") ;
    res->src = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* srconf */
    resolve_find_cdb(&tmp,c,"srconf") ;
    res->srconf = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* live */
    resolve_find_cdb(&tmp,c,"live") ;
    res->live = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* runat */
    resolve_find_cdb(&tmp,c,"runat") ;
    res->runat = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* tree */
    resolve_find_cdb(&tmp,c,"tree") ;
    res->tree = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* treename */
    resolve_find_cdb(&tmp,c,"treename") ;
    res->treename = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* state */
    resolve_find_cdb(&tmp,c,"state") ;
    res->state = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* exec_run */
    resolve_find_cdb(&tmp,c,"exec_run") ;
    res->exec_run = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* exec_log_run */
    resolve_find_cdb(&tmp,c,"exec_log_run") ;
    res->exec_log_run = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* real_exec_run */
    resolve_find_cdb(&tmp,c,"real_exec_run") ;
    res->real_exec_run = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* real_exec_log_run */
    resolve_find_cdb(&tmp,c,"real_exec_log_run") ;
    res->real_exec_log_run = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* exec_finish */
    resolve_find_cdb(&tmp,c,"exec_finish") ;
    res->exec_finish = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* real_exec_finish */
    resolve_find_cdb(&tmp,c,"real_exec_finish") ;
    res->real_exec_finish = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* type */
    x = resolve_find_cdb(&tmp,c,"type") ;
    res->type = x ;

    /* ndeps */
    x = resolve_find_cdb(&tmp,c,"ndeps") ;
    res->ndeps = x ;

    /* noptsdeps */
    x = resolve_find_cdb(&tmp,c,"noptsdeps") ;
    res->noptsdeps = x ;

    /* nextdeps */
    x = resolve_find_cdb(&tmp,c,"nextdeps") ;
    res->nextdeps = x ;

    /* ncontents */
    x = resolve_find_cdb(&tmp,c,"ncontents") ;
    res->ncontents = x ;

    /* down */
    x = resolve_find_cdb(&tmp,c,"down") ;
    res->down = x ;

    /* disen */
    x = resolve_find_cdb(&tmp,c,"disen") ;
    res->disen = x ;

    free(wres) ;
    stralloc_free(&tmp) ;

    return 1 ;
}

int service_write_cdb(cdbmaker *c, resolve_service_t *sres)
{

    log_flow() ;

    char *str = sres->sa.s ;

    /* name */
    if (!resolve_add_cdb(c,"name",str + sres->name) ||

    /* description */
    !resolve_add_cdb(c,"description",str + sres->description) ||

    /* version */
    !resolve_add_cdb(c,"version",str + sres->version) ||

    /* logger */
    !resolve_add_cdb(c,"logger",str + sres->logger) ||

    /* logreal */
    !resolve_add_cdb(c,"logreal",str + sres->logreal) ||

    /* logassoc */
    !resolve_add_cdb(c,"logassoc",str + sres->logassoc) ||

    /* dstlog */
    !resolve_add_cdb(c,"dstlog",str + sres->dstlog) ||

    /* deps */
    !resolve_add_cdb(c,"deps",str + sres->deps) ||

    /* optsdeps */
    !resolve_add_cdb(c,"optsdeps",str + sres->optsdeps) ||

    /* extdeps */
    !resolve_add_cdb(c,"extdeps",str + sres->extdeps) ||

    /* contents */
    !resolve_add_cdb(c,"contents",str + sres->contents) ||

    /* src */
    !resolve_add_cdb(c,"src",str + sres->src) ||

    /* srconf */
    !resolve_add_cdb(c,"srconf",str + sres->srconf) ||

    /* live */
    !resolve_add_cdb(c,"live",str + sres->live) ||

    /* runat */
    !resolve_add_cdb(c,"runat",str + sres->runat) ||

    /* tree */
    !resolve_add_cdb(c,"tree",str + sres->tree) ||

    /* treename */
    !resolve_add_cdb(c,"treename",str + sres->treename) ||

    /* dstlog */
    !resolve_add_cdb(c,"dstlog",str + sres->dstlog) ||

    /* state */
    !resolve_add_cdb(c,"state",str + sres->state) ||

    /* exec_run */
    !resolve_add_cdb(c,"exec_run",str + sres->exec_run) ||

    /* exec_log_run */
    !resolve_add_cdb(c,"exec_log_run",str + sres->exec_log_run) ||

    /* real_exec_run */
    !resolve_add_cdb(c,"real_exec_run",str + sres->real_exec_run) ||

    /* real_exec_log_run */
    !resolve_add_cdb(c,"real_exec_log_run",str + sres->real_exec_log_run) ||

    /* exec_finish */
    !resolve_add_cdb(c,"exec_finish",str + sres->exec_finish) ||

    /* real_exec_finish */
    !resolve_add_cdb(c,"real_exec_finish",str + sres->real_exec_finish) ||

    /* type */
    !resolve_add_cdb_uint(c,"type",sres->type) ||

    /* ndeps */
    !resolve_add_cdb_uint(c,"ndeps",sres->ndeps) ||

    /* noptsdeps */
    !resolve_add_cdb_uint(c,"noptsdeps",sres->noptsdeps) ||

    /* nextdeps */
    !resolve_add_cdb_uint(c,"nextdeps",sres->nextdeps) ||

    /* ncontents */
    !resolve_add_cdb_uint(c,"ncontents",sres->ncontents) ||

    /* down */
    !resolve_add_cdb_uint(c,"down",sres->down) ||

    /* disen */
    !resolve_add_cdb_uint(c,"disen",sres->disen)) return 0 ;

    return 1 ;
}

int service_resolve_copy(resolve_service_t *dst, resolve_service_t *res)
{
    log_flow() ;

    stralloc_free(&dst->sa) ;

    size_t len = res->sa.len - 1 ;
    dst->salen = res->salen ;

    if (!stralloc_catb(&dst->sa,res->sa.s,len) ||
        !stralloc_0(&dst->sa))
            return 0 ;

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

    return 1 ;
}

int service_resolve_sort_bytype(genalloc *gares, stralloc *list, char const *src)
{
    log_flow() ;

    size_t pos = 0 ;
    int e = 0 ;

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;

    resolve_wrapper_t_ref wres = resolve_set_struct(SERVICE_STRUCT, &res) ;

    size_t classic_list = 0, module_list = 0 ;

    FOREACH_SASTR(list, pos) {

        char *name = list->s + pos ;

        resolve_service_t cp = RESOLVE_SERVICE_ZERO ;

        if (!resolve_read(wres, src, name))
            log_warnu_return(LOG_EXIT_ZERO,"read resolve file of: ", src, name) ;

        if (!service_resolve_copy(&cp, &res))
            goto err ;

        switch (res.type) {

            case TYPE_CLASSIC:

                if (!genalloc_insertb(resolve_service_t, gares, 0, &cp, 1))
                    goto err ;

                classic_list++ ;
                module_list = classic_list ;

                break ;

            case TYPE_MODULE:

                if (!genalloc_insertb(resolve_service_t, gares, classic_list, &cp, 1))
                    goto err ;

                module_list++ ;

                break ;

            case TYPE_BUNDLE:
            case TYPE_LONGRUN:
            case TYPE_ONESHOT:

                if (!genalloc_insertb(resolve_service_t, gares, module_list, &cp, 1))
                    goto err ;

                break ;

            default: log_warn_return(LOG_EXIT_ZERO,"unknown action") ;
        }
    }

    e = 1 ;

    err:
        resolve_free(wres) ;
        return e ;
}

/** @Return -1 system error
 * @Return 0 no tree exist yet
 * @Return 1 svname doesn't exist
 * @Return 2 on success
 * @Return > 2, service exist on different tree */
int service_resolve_svtree(stralloc *svtree, char const *svname, char const *tree)
{
    log_flow() ;

    uint8_t found = 1, copied = 0 ;
    uid_t owner = getuid() ;
    size_t pos = 0, newlen ;
    stralloc satree = STRALLOC_ZERO ;
    stralloc tmp = STRALLOC_ZERO ;
    char const *exclude[3] = { SS_BACKUP + 1, SS_RESOLVE + 1, 0 } ;

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

    if (!sastr_dir_get(&satree, svtree->s, exclude, S_IFDIR)) {
        log_warnu("get list of trees from directory: ",svtree->s) ;
        goto err ;
    }

    if (satree.len)
    {

        FOREACH_SASTR(&satree, pos) {

            tmp.len = newlen ;
            char *name = satree.s + pos ;

            if (!auto_stra(&tmp,name,SS_SVDIRS)) goto err ;
            if (resolve_check(tmp.s,svname)) {

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

int service_resolve_setnwrite(sv_alltype *services, ssexec_t *info, char const *dst)
{
    log_flow() ;

    int e = 0 ;
    char ownerstr[UID_FMT] ;
    size_t ownerlen = uid_fmt(ownerstr,info->owner), id, nid ;
    ownerstr[ownerlen] = 0 ;

    stralloc destlog = STRALLOC_ZERO ;
    stralloc ndeps = STRALLOC_ZERO ;
    stralloc other_deps = STRALLOC_ZERO ;

    ss_state_t sta = STATE_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(SERVICE_STRUCT, &res) ;

    resolve_init(wres) ;

    char *name = keep.s + services->cname.name ;
    size_t namelen = strlen(name) ;
    char logname[namelen + SS_LOG_SUFFIX_LEN + 1] ;
    char logreal[namelen + SS_LOG_SUFFIX_LEN + 1] ;
    char stmp[info->livetree.len + 1 + info->treename.len + SS_SVDIRS_LEN + 1 + namelen + SS_LOG_SUFFIX_LEN + 1] ;

    size_t livelen = info->live.len - 1 ;
    char state[livelen + SS_STATE_LEN + 1 + ownerlen + 1 + info->treename.len + 1 + namelen + 1] ;
    auto_strings(state, info->live.s, SS_STATE, "/", ownerstr, "/", info->treename.s) ;


    res.type = services->cname.itype ;
    res.name = resolve_add_string(wres,name) ;
    res.description = resolve_add_string(wres,keep.s + services->cname.description) ;
    res.version = resolve_add_string(wres,keep.s + services->cname.version) ;
    res.tree = resolve_add_string(wres,info->tree.s) ;
    res.treename = resolve_add_string(wres,info->treename.s) ;
    res.live = resolve_add_string(wres,info->live.s) ;
    res.state = resolve_add_string(wres,state) ;
    res.src = resolve_add_string(wres,keep.s + services->src) ;

    if (services->srconf > 0)
        res.srconf = resolve_add_string(wres,keep.s + services->srconf) ;

    if (res.type == TYPE_ONESHOT) {

        if (services->type.oneshot.up.exec >= 0) {

            res.exec_run = resolve_add_string(wres,keep.s + services->type.oneshot.up.exec) ;
            res.real_exec_run = resolve_add_string(wres,keep.s + services->type.oneshot.up.real_exec) ;
        }

        if (services->type.oneshot.down.exec >= 0) {

            res.exec_finish = resolve_add_string(wres,keep.s + services->type.oneshot.down.exec) ;
            res.real_exec_finish = resolve_add_string(wres,keep.s + services->type.oneshot.down.real_exec) ;
        }
    }

    if (res.type == TYPE_CLASSIC || res.type == TYPE_LONGRUN) {

        if (services->type.classic_longrun.run.exec >= 0) {
            res.exec_run = resolve_add_string(wres,keep.s + services->type.classic_longrun.run.exec) ;
            res.real_exec_run = resolve_add_string(wres,keep.s + services->type.classic_longrun.run.real_exec) ;
        }

        if (services->type.classic_longrun.finish.exec >= 0) {

            res.exec_finish = resolve_add_string(wres,keep.s + services->type.classic_longrun.finish.exec) ;
            res.real_exec_finish = resolve_add_string(wres,keep.s + services->type.classic_longrun.finish.real_exec) ;
        }
    }

    res.ndeps = services->cname.nga ;
    res.noptsdeps = services->cname.nopts ;
    res.nextdeps = services->cname.next ;
    res.ncontents = services->cname.ncontents ;

    if (services->flags[0])
        res.down = 1 ;

    res.disen = 1 ;

    if (res.type == TYPE_CLASSIC) {

        auto_strings(stmp, info->scandir.s, "/", name) ;
        res.runat = resolve_add_string(wres,stmp) ;

    } else if (res.type >= TYPE_BUNDLE) {

        auto_strings(stmp, info->livetree.s, "/", info->treename.s, SS_SVDIRS, "/", name) ;
        res.runat = resolve_add_string(wres,stmp) ;
    }

    if (state_check(state,name)) {

        if (!state_read(&sta,state,name)) {
            log_warnusys("read state file of: ",name) ;
            goto err ;
        }

        if (!sta.init)
            state_setflag(&sta,SS_FLAGS_RELOAD,SS_FLAGS_TRUE) ;

        if (sta.init) {

            state_setflag(&sta,SS_FLAGS_INIT,SS_FLAGS_TRUE) ;

        } else {

            state_setflag(&sta,SS_FLAGS_INIT,SS_FLAGS_FALSE) ;
        }

        state_setflag(&sta,SS_FLAGS_UNSUPERVISE,SS_FLAGS_FALSE) ;

        if (!state_write(&sta,res.sa.s + res.state,name)) {
            log_warnusys("write state file of: ",name) ;
            goto err ;
        }
    }

    if (res.ndeps)
    {
        id = services->cname.idga, nid = res.ndeps ;
        for (;nid; id += strlen(deps.s + id) + 1, nid--) {

            if (!stralloc_catb(&ndeps,deps.s + id,strlen(deps.s + id)) ||
                !stralloc_catb(&ndeps," ",1)) {
                    log_warnsys("stralloc") ;
                    goto err ;
                }
        }
        ndeps.len-- ;

        if (!stralloc_0(&ndeps)) {
            log_warnsys("stralloc") ;
            goto err ;
        }

        res.deps = resolve_add_string(wres,ndeps.s) ;
    }

    if (res.noptsdeps)
    {
        id = services->cname.idopts, nid = res.noptsdeps ;
        for (;nid; id += strlen(deps.s + id) + 1, nid--) {

            if (!stralloc_catb(&other_deps,deps.s + id,strlen(deps.s + id)) ||
                !stralloc_catb(&other_deps," ",1)) {
                    log_warnsys("stralloc") ;
                    goto err ;
                }
        }
        other_deps.len-- ;

        if (!stralloc_0(&other_deps)) {
            log_warnsys("stralloc") ;
            goto err ;
        }

        res.optsdeps = resolve_add_string(wres,other_deps.s) ;
    }

    if (res.nextdeps)
    {
        other_deps.len = 0 ;
        id = services->cname.idext, nid = res.nextdeps ;
        for (;nid; id += strlen(deps.s + id) + 1, nid--) {

            if (!stralloc_catb(&other_deps,deps.s + id,strlen(deps.s + id)) ||
                !stralloc_catb(&other_deps," ",1)){
                    log_warnsys("stralloc") ;
                    goto err ;
                }
        }
        other_deps.len-- ;

        if (!stralloc_0(&other_deps)){
            log_warnsys("stralloc") ;
            goto err ;
        }

        res.extdeps = resolve_add_string(wres,other_deps.s) ;
    }

    if (res.ncontents) {

        other_deps.len = 0 ;
        id = services->cname.idcontents, nid = res.ncontents ;
        for (;nid; id += strlen(deps.s + id) + 1, nid--) {

            if (!stralloc_catb(&other_deps,deps.s + id,strlen(deps.s + id)) ||
                !stralloc_catb(&other_deps," ",1)) {
                    log_warnsys("stralloc") ;
                    goto err ;
                }
        }
        other_deps.len-- ;
        if (!stralloc_0(&other_deps)) {
            log_warnsys("stralloc") ;
            goto err ;
        }
        res.contents = resolve_add_string(wres,other_deps.s) ;
    }

    if (services->opts[0]) {

        // destination of the logger
        if (services->type.classic_longrun.log.destination < 0) {

            if(info->owner > 0) {

                if (!auto_stra(&destlog, get_userhome(info->owner), "/", SS_LOGGER_USERDIR, name)) {
                    log_warnsys("stralloc") ;
                    goto err ;
                }

            } else {

                if (!auto_stra(&destlog, SS_LOGGER_SYSDIR, name)) {
                    log_warnsys("stralloc") ;
                    goto err ;
                }
            }

        } else {

            if (!auto_stra(&destlog,keep.s + services->type.classic_longrun.log.destination)) {
                log_warnsys("stralloc") ;
                goto err ;
            }
        }

        res.dstlog = resolve_add_string(wres,destlog.s) ;

        if ((res.type == TYPE_CLASSIC) || (res.type == TYPE_LONGRUN)) {

            auto_strings(logname, name, SS_LOG_SUFFIX) ;
            auto_strings(logreal, name) ;

            if (res.type == TYPE_CLASSIC) {
                auto_strings(logreal + namelen, "/log") ;

            } else {

                auto_strings(logreal + namelen,"-log") ;
            }

            res.logger = resolve_add_string(wres,logname) ;
            res.logreal = resolve_add_string(wres,logreal) ;
            if (ndeps.len)
                ndeps.len--;

            if (!stralloc_catb(&ndeps," ",1) ||
                !stralloc_cats(&ndeps,res.sa.s + res.logger) ||
                !stralloc_0(&ndeps)) {
                    log_warnsys("stralloc") ;
                    goto err ;
                }

            res.deps = resolve_add_string(wres,ndeps.s) ;

            if (res.type == TYPE_CLASSIC) {

                res.ndeps = 1 ;

            } else if (res.type == TYPE_LONGRUN) {

                res.ndeps += 1 ;
            }

            if (services->type.classic_longrun.log.run.exec >= 0)
                res.exec_log_run = resolve_add_string(wres,keep.s + services->type.classic_longrun.log.run.exec) ;

            if (services->type.classic_longrun.log.run.real_exec >= 0)
                res.real_exec_log_run = resolve_add_string(wres,keep.s + services->type.classic_longrun.log.run.real_exec) ;

            if (!service_resolve_setlognwrite(&res,dst))
                goto err ;
        }
    }

    if (!resolve_write(wres,dst,res.sa.s + res.name)) {

        log_warnusys("write resolve file: ",dst,SS_RESOLVE,"/",res.sa.s + res.name) ;
        goto err ;
    }

    e = 1 ;

    err:
        resolve_free(wres) ;
        stralloc_free(&ndeps) ;
        stralloc_free(&other_deps) ;
        stralloc_free(&destlog) ;
        return e ;
}

int service_resolve_setlognwrite(resolve_service_t *sv, char const *dst)
{
    log_flow() ;

    if (!sv->logger) return 1 ;

    int e = 0 ;
    ss_state_t sta = STATE_ZERO ;

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(SERVICE_STRUCT, &res) ;

    resolve_init(wres) ;

    char *str = sv->sa.s ;
    size_t svlen = strlen(str + sv->name) ;
    char descrip[svlen + 7 + 1] ;
    auto_strings(descrip, str + sv->name, " logger") ;

    size_t runlen = strlen(str + sv->runat) ;
    char live[runlen + 4 + 1] ;
    memcpy(live,str + sv->runat,runlen) ;
    if (sv->type >= TYPE_BUNDLE) {

        memcpy(live + runlen,"-log",4)  ;

    } else memcpy(live + runlen,"/log",4)  ;

    live[runlen + 4] = 0 ;

    res.type = sv->type ;
    res.name = resolve_add_string(wres,str + sv->logger) ;
    res.description = resolve_add_string(wres,descrip) ;
    res.version = resolve_add_string(wres,str + sv->version) ;
    res.logreal = resolve_add_string(wres,str + sv->logreal) ;
    res.logassoc = resolve_add_string(wres,str + sv->name) ;
    res.dstlog = resolve_add_string(wres,str + sv->dstlog) ;
    res.live = resolve_add_string(wres,str + sv->live) ;
    res.runat = resolve_add_string(wres,live) ;
    res.tree = resolve_add_string(wres,str + sv->tree) ;
    res.treename = resolve_add_string(wres,str + sv->treename) ;
    res.state = resolve_add_string(wres,str + sv->state) ;
    res.src = resolve_add_string(wres,str + sv->src) ;
    res.down = sv->down ;
    res.disen = sv->disen ;

    if (sv->exec_log_run > 0)
        res.exec_log_run = resolve_add_string(wres,str + sv->exec_log_run) ;

    if (sv->real_exec_log_run > 0)
        res.real_exec_log_run = resolve_add_string(wres,str + sv->real_exec_log_run) ;

    if (state_check(str + sv->state,str + sv->logger)) {

        if (!state_read(&sta,str + sv->state,str + sv->logger)) {

            log_warnusys("read state file of: ",str + sv->logger) ;
            goto err ;
        }

        if (!sta.init)
            state_setflag(&sta,SS_FLAGS_RELOAD,SS_FLAGS_TRUE) ;

        state_setflag(&sta,SS_FLAGS_INIT,SS_FLAGS_FALSE) ;
        state_setflag(&sta,SS_FLAGS_UNSUPERVISE,SS_FLAGS_FALSE) ;

        if (!state_write(&sta,str + sv->state,str + sv->logger)) {
            log_warnusys("write state file of: ",str + sv->logger) ;
            goto err ;
        }
    }

    if (!resolve_write(wres,dst,res.sa.s + res.name)) {
        log_warnusys("write resolve file: ",dst,SS_RESOLVE,"/",res.sa.s + res.name) ;
        goto err ;
    }

    e = 1 ;

    err:
        resolve_free(wres) ;
        return e ;
}

int service_resolve_modify_field(resolve_service_t *res, resolve_service_enum_t field, char const *data)
{
    log_flow() ;

    uint32_t ifield ;
    int e = 0 ;

    resolve_wrapper_t_ref wres = resolve_set_struct(SERVICE_STRUCT, res) ;

    switch(field) {

        case SERVICE_ENUM_NAME:
            res->name = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_DESCRIPTION:
            res->description = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_VERSION:
            res->version = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_LOGGER:
            res->logger = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_LOGREAL:
            res->logreal = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_LOGASSOC:
            res->logassoc = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_DSTLOG:
            res->dstlog = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_DEPS:
            res->deps = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_OPTSDEPS:
            res->optsdeps = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_EXTDEPS:
            res->extdeps = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_CONTENTS:
            res->contents = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_SRC:
            res->src = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_SRCONF:
            res->srconf = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_LIVE:
            res->live = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_RUNAT:
            res->runat = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_TREE:
            res->tree = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_TREENAME:
            res->treename = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_STATE:
            res->state = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_EXEC_RUN:
            res->exec_run = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_EXEC_LOG_RUN:
            res->exec_log_run = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_REAL_EXEC_RUN:
            res->real_exec_run = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_REAL_EXEC_LOG_RUN:
            res->real_exec_log_run = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_EXEC_FINISH:
            res->exec_finish = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_REAL_EXEC_FINISH:
            res->real_exec_finish = resolve_add_string(wres,data) ;
            break ;

        case SERVICE_ENUM_TYPE:
            if (!uint0_scan(data, &ifield)) goto err ;
            res->type = ifield ;
            break ;

        case SERVICE_ENUM_NDEPS:
            if (!uint0_scan(data, &ifield)) goto err ;
            res->ndeps = ifield ;
            break ;

        case SERVICE_ENUM_NOPTSDEPS:
            if (!uint0_scan(data, &ifield)) goto err ;
            res->noptsdeps = ifield ;
            break ;

        case SERVICE_ENUM_NEXTDEPS:
            if (!uint0_scan(data, &ifield)) goto err ;
            res->nextdeps = ifield ;
            break ;

        case SERVICE_ENUM_NCONTENTS:
            if (!uint0_scan(data, &ifield)) goto err ;
            res->ncontents = ifield ;
            break ;

        case SERVICE_ENUM_DOWN:
            if (!uint0_scan(data, &ifield)) goto err ;
            res->down = ifield ;
            break ;

        case SERVICE_ENUM_DISEN:
            if (!uint0_scan(data, &ifield)) goto err ;
            res->disen = ifield ;
            break ;

        default:
            break ;
    }

    e = 1 ;

    err:
        free(wres) ;
        return e ;

}

int service_resolve_field_to_sa(stralloc *sa, resolve_service_t *res, resolve_service_enum_t field)
{
    log_flow() ;

    uint32_t ifield ;

    switch(field) {

        case SERVICE_ENUM_NAME:
            ifield = res->name ;
            break ;

        case SERVICE_ENUM_DESCRIPTION:
            ifield = res->description ;
            break ;

        case SERVICE_ENUM_VERSION:
            ifield = res->version ;
            break ;

        case SERVICE_ENUM_LOGGER:
            ifield = res->logger ;
            break ;

        case SERVICE_ENUM_LOGREAL:
            ifield = res->logreal ;
            break ;

        case SERVICE_ENUM_LOGASSOC:
            ifield = res->logassoc ;
            break ;

        case SERVICE_ENUM_DSTLOG:
            ifield = res->dstlog ;
            break ;

        case SERVICE_ENUM_DEPS:
            ifield = res->deps ;
            break ;

        case SERVICE_ENUM_OPTSDEPS:
            ifield = res->optsdeps ;
            break ;

        case SERVICE_ENUM_EXTDEPS:
            ifield = res->extdeps ;
            break ;

        case SERVICE_ENUM_CONTENTS:
            ifield = res->contents ;
            break ;

        case SERVICE_ENUM_SRC:
            ifield = res->src ;
            break ;

        case SERVICE_ENUM_SRCONF:
            ifield = res->srconf ;
            break ;

        case SERVICE_ENUM_LIVE:
            ifield = res->live ;
            break ;

        case SERVICE_ENUM_RUNAT:
            ifield = res->runat ;
            break ;

        case SERVICE_ENUM_TREE:
            ifield = res->tree ;
            break ;

        case SERVICE_ENUM_TREENAME:
            ifield = res->treename ;
            break ;

        case SERVICE_ENUM_STATE:
            ifield = res->state ;
            break ;

        case SERVICE_ENUM_EXEC_RUN:
            ifield = res->exec_run ;
            break ;

        case SERVICE_ENUM_EXEC_LOG_RUN:
            ifield = res->exec_log_run ;
            break ;

        case SERVICE_ENUM_REAL_EXEC_RUN:
            ifield = res->real_exec_run ;
            break ;

        case SERVICE_ENUM_REAL_EXEC_LOG_RUN:
            ifield = res->real_exec_log_run ;
            break ;

        case SERVICE_ENUM_EXEC_FINISH:
            ifield = res->exec_finish ;
            break ;

        case SERVICE_ENUM_REAL_EXEC_FINISH:
            ifield = res->real_exec_finish ;
            break ;

        case SERVICE_ENUM_TYPE:
            ifield = res->type ;
            break ;

        case SERVICE_ENUM_NDEPS:
            ifield = res->ndeps ;
            break ;

        case SERVICE_ENUM_NOPTSDEPS:
            ifield = res->noptsdeps ;
            break ;

        case SERVICE_ENUM_NEXTDEPS:
            ifield = res->nextdeps ;
            break ;

        case SERVICE_ENUM_NCONTENTS:
            ifield = res->ncontents ;
            break ;

        case SERVICE_ENUM_DOWN:
            ifield = res->down ;
            break ;

        case SERVICE_ENUM_DISEN:
            ifield = res->disen ;
            break ;

        default:
            return 0 ;
    }

    if (!auto_stra(sa,res->sa.s + ifield))
        return 0 ;

    return 1 ;
}

int service_resolve_add_deps(genalloc *tokeep, resolve_service_t *res, char const *src)
{
    log_flow() ;

    int e = 0 ;
    size_t pos = 0 ;
    stralloc tmp = STRALLOC_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(SERVICE_STRUCT, res) ;

    char *name = res->sa.s + res->name ;
    char *deps = res->sa.s + res->deps ;
    if (!resolve_cmp(tokeep, name, SERVICE_STRUCT) && (!obstr_equal(name,SS_MASTER+1)))
        if (!resolve_append(tokeep,wres)) goto err ;

    if (res->ndeps)
    {
        if (!sastr_clean_string(&tmp,deps)) return 0 ;
        for (;pos < tmp.len ; pos += strlen(tmp.s + pos) + 1)
        {
            resolve_service_t dres = RESOLVE_SERVICE_ZERO ;
            resolve_wrapper_t_ref dwres = resolve_set_struct(SERVICE_STRUCT, &dres) ;
            char *dname = tmp.s + pos ;
            if (!resolve_check(src,dname)) goto err ;
            if (!resolve_read(dwres,src,dname)) goto err ;
            if (dres.ndeps && !resolve_cmp(tokeep, dname, SERVICE_STRUCT))
            {
                if (!service_resolve_add_deps(tokeep,&dres,src)) goto err ;
            }
            if (!resolve_cmp(tokeep, dname, SERVICE_STRUCT))
            {
                if (!resolve_append(tokeep,dwres)) goto err ;
            }
            resolve_free(dwres) ;
        }
    }

    e = 1 ;

    err:
        stralloc_free(&tmp) ;
        free(wres) ;
        return e ;
}

int service_resolve_add_rdeps(genalloc *tokeep, resolve_service_t *res, char const *src)
{
    log_flow() ;

    int type, e = 0 ;
    stralloc tmp = STRALLOC_ZERO ;
    stralloc nsv = STRALLOC_ZERO ;
    ss_state_t sta = STATE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(SERVICE_STRUCT, res) ;
    char const *exclude[2] = { SS_MASTER + 1, 0 } ;

    char *name = res->sa.s + res->name ;
    size_t srclen = strlen(src), a = 0, b = 0, c = 0 ;
    char s[srclen + SS_RESOLVE_LEN + 1] ;
    memcpy(s,src,srclen) ;
    memcpy(s + srclen,SS_RESOLVE,SS_RESOLVE_LEN) ;
    s[srclen + SS_RESOLVE_LEN] = 0 ;

    if (res->type == TYPE_CLASSIC) type = 0 ;
    else type = 1 ;

    if (!sastr_dir_get(&nsv,s,exclude,S_IFREG)) goto err ;

    if (!resolve_cmp(tokeep, name, SERVICE_STRUCT) && (!obstr_equal(name,SS_MASTER+1)))
    {
        if (!resolve_append(tokeep,wres)) goto err ;
    }
    if ((res->type == TYPE_BUNDLE || res->type == TYPE_MODULE) && res->ndeps)
    {
        uint32_t deps = res->type == TYPE_MODULE ? res->contents : res->deps ;
        if (!sastr_clean_string(&tmp,res->sa.s + deps)) goto err ;
        resolve_service_t dres = RESOLVE_SERVICE_ZERO ;
        resolve_wrapper_t_ref dwres = resolve_set_struct(SERVICE_STRUCT, &dres) ;
        for (; a < tmp.len ; a += strlen(tmp.s + a) + 1)
        {
            char *name = tmp.s + a ;
            if (!resolve_check(src,name)) goto err ;
            if (!resolve_read(dwres,src,name)) goto err ;
            if (dres.type == TYPE_CLASSIC) continue ;
            if (!resolve_cmp(tokeep, name, SERVICE_STRUCT))
            {
                if (!resolve_append(tokeep,dwres)) goto err ;
                if (!service_resolve_add_rdeps(tokeep,&dres,src)) goto err ;
            }
            resolve_free(dwres) ;
        }
    }
    for (; b < nsv.len ; b += strlen(nsv.s + b) +1)
    {
        int dtype = 0 ;
        tmp.len = 0 ;
        resolve_service_t dres = RESOLVE_SERVICE_ZERO ;
        resolve_wrapper_t_ref dwres = resolve_set_struct(SERVICE_STRUCT, &dres) ;
        char *dname = nsv.s + b ;
        if (obstr_equal(name,dname)) { resolve_free(wres) ; continue ; }
        if (!resolve_check(src,dname)) goto err ;
        if (!resolve_read(dwres,src,dname)) goto err ;

        if (dres.type == TYPE_CLASSIC) dtype = 0 ;
        else dtype = 1 ;

        if (state_check(dres.sa.s + dres.state,dname))
        {
            if (!state_read(&sta,dres.sa.s + dres.state,dname)) goto err ;
            if (dtype != type || (!dres.disen && !sta.unsupervise)){ resolve_free(dwres) ; continue ; }
        }
        else if (dtype != type || (!dres.disen)){ resolve_free(dwres) ; continue ; }
        if (dres.type == TYPE_BUNDLE && !dres.ndeps){ resolve_free(dwres) ; continue ; }

        if (!resolve_cmp(tokeep, dname, SERVICE_STRUCT))
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
                        if (!resolve_append(tokeep,dwres)) goto err ;
                        if (!service_resolve_add_rdeps(tokeep,&dres,src)) goto err ;
                        resolve_free(dwres) ;
                        break ;
                    }
                }
            }
        }
        skip:
        resolve_free(dwres) ;
    }

    e = 1 ;
    err:
        free(wres) ;
        stralloc_free(&nsv) ;
        stralloc_free(&tmp) ;
        return e ;
}

int service_resolve_add_logger(genalloc *ga,char const *src)
{
    log_flow() ;

    int e = 0 ;
    size_t i = 0 ;
    genalloc gatmp = GENALLOC_ZERO ;

    for (; i < genalloc_len(resolve_service_t,ga) ; i++)
    {
        resolve_service_t res = RESOLVE_SERVICE_ZERO ;
        resolve_wrapper_t_ref wres = resolve_set_struct(SERVICE_STRUCT, &res) ;
        resolve_service_t dres = RESOLVE_SERVICE_ZERO ;
        resolve_wrapper_t_ref dwres = resolve_set_struct(SERVICE_STRUCT, &dres) ;
        if (!service_resolve_copy(&res,&genalloc_s(resolve_service_t,ga)[i])) goto err ;

        char *string = res.sa.s ;
        char *name = string + res.name ;
        if (!resolve_cmp(&gatmp, name, SERVICE_STRUCT))
        {
            if (!resolve_append(&gatmp,wres))
                goto err ;

            if (res.logger)
            {
                if (!resolve_check(src,string + res.logger)) goto err ;
                if (!resolve_read(dwres,src,string + res.logger))
                    goto err ;
                if (!resolve_cmp(&gatmp, string + res.logger, SERVICE_STRUCT))
                    if (!resolve_append(&gatmp,dwres)) goto err ;
            }
        }
        resolve_free(wres) ;
        resolve_free(dwres) ;
    }
    resolve_deep_free(SERVICE_STRUCT, ga) ;
    if (!genalloc_copy(resolve_service_t,ga,&gatmp)) goto err ;

    e = 1 ;

    err:
        genalloc_free(resolve_service_t,&gatmp) ;
        resolve_deep_free(SERVICE_STRUCT, &gatmp) ;
        return e ;
}

int service_resolve_write_master(ssexec_t *info, ss_resolve_graph_t *graph,char const *dir, unsigned int reverse)
{
    log_flow() ;

    int r, e = 0 ;
    size_t i = 0 ;
    char ownerstr[UID_FMT] ;
    size_t ownerlen = uid_fmt(ownerstr,info->owner) ;
    ownerstr[ownerlen] = 0 ;

    stralloc in = STRALLOC_ZERO ;
    stralloc inres = STRALLOC_ZERO ;
    stralloc gain = STRALLOC_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(SERVICE_STRUCT, &res) ;

    size_t dirlen = strlen(dir) ;

    resolve_init(wres) ;

    char runat[info->livetree.len + 1 + info->treename.len + SS_SVDIRS_LEN + SS_MASTER_LEN + 1] ;
    auto_strings(runat, info->livetree.s, "/", info->treename.s, SS_SVDIRS, SS_MASTER) ;

    char dst[dirlen + SS_DB_LEN + SS_SRC_LEN + SS_MASTER_LEN + 1] ;
    auto_strings(dst, dir, SS_DB, SS_SRC, SS_MASTER) ;

    size_t livelen = info->live.len - 1 ;
    char state[livelen + SS_STATE_LEN + 1 + ownerlen + 1 + info->treename.len + 1] ;
    auto_strings(state, info->live.s, SS_STATE, "/", ownerstr, "/", info->treename.s) ;

    if (reverse)
    {
        size_t dstlen = strlen(dst) ;
        char file[dstlen + 1 + SS_CONTENTS_LEN + 1] ;
        auto_strings(file, dst, "/", SS_CONTENTS) ;

        size_t filesize=file_get_size(file) ;

        r = openreadfileclose(file,&gain,filesize) ;
        if(!r) goto err ;
        /** ensure that we have an empty line at the end of the string*/
        if (!auto_stra(&gain,"\n")) goto err ;
        if (!sastr_clean_element(&gain)) goto err ;
    }

    for (; i < genalloc_len(resolve_service_t,&graph->sorted); i++)
    {
        char *string = genalloc_s(resolve_service_t,&graph->sorted)[i].sa.s ;
        char *name = string + genalloc_s(resolve_service_t,&graph->sorted)[i].name ;
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

    res.name = resolve_add_string(wres,SS_MASTER+1) ;
    res.description = resolve_add_string(wres,"inner bundle - do not use it") ;
    res.treename = resolve_add_string(wres,info->treename.s) ;
    res.tree = resolve_add_string(wres,info->tree.s) ;
    res.live = resolve_add_string(wres,info->live.s) ;
    res.type = TYPE_BUNDLE ;
    res.deps = resolve_add_string(wres,inres.s) ;
    res.ndeps = genalloc_len(resolve_service_t,&graph->sorted) ;
    res.runat = resolve_add_string(wres,runat) ;
    res.state = resolve_add_string(wres,state) ;

    if (!resolve_write(wres,dir,SS_MASTER+1)) goto err ;

    e = 1 ;

    err:
        resolve_free(wres) ;
        stralloc_free(&in) ;
        stralloc_free(&inres) ;
        stralloc_free(&gain) ;
        return e ;
}

