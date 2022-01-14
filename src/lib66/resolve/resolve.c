/*
 * resolve.c
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
#include <unistd.h>//close, fsync
#include <stdlib.h>//mkstemp, malloc, free
#include <sys/types.h>//ssize_t
#include <stdio.h>//rename

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/cdb.h>
#include <skalibs/djbunix.h>
#include <skalibs/cdbmake.h>
#include <skalibs/posixplz.h>//unlink
#include <skalibs/types.h>//uint##_pack

#include <66/resolve.h>
#include <66/service.h>
#include <66/tree.h>
#include <66/constants.h>
#include <66/graph.h>

/**
 *
 * MAIN API
 *
 * */

resolve_wrapper_t *resolve_set_struct(uint8_t type, void *s)
{
    log_flow() ;

    resolve_wrapper_t *wres = malloc(sizeof(resolve_wrapper_t)) ;

    wres->obj = s ;
    wres->type = type ;
    return wres ;
} ;


int resolve_init(resolve_wrapper_t *wres)
{
    log_flow() ;

    RESOLVE_SET_SAWRES(wres) ;

    sawres->len = 0 ;

    return resolve_add_string(wres,"") ;
}

int resolve_read(resolve_wrapper_t *wres, char const *src, char const *name)
{
    log_flow() ;

    size_t srclen = strlen(src) ;
    size_t namelen = strlen(name) ;

    char tmp[srclen + SS_RESOLVE_LEN + 1 + namelen + 1] ;
    auto_strings(tmp,src,SS_RESOLVE,"/",name) ;

    if (!resolve_read_cdb(wres,tmp))
        return 0 ;

    return 1 ;
}

int resolve_write(resolve_wrapper_t *wres, char const *dst, char const *name)
{
    log_flow() ;

    size_t dstlen = strlen(dst) ;
    size_t namelen = strlen(name) ;

    char tmp[dstlen + SS_RESOLVE_LEN + 1 + namelen + 1] ;
    auto_strings(tmp,dst,SS_RESOLVE,"/") ;

    if (!resolve_write_cdb(wres,tmp,name))
        return 0 ;

    return 1 ;
}

int resolve_check(char const *src, char const *name)
{
    log_flow() ;

    int r ;
    size_t srclen = strlen(src) ;
    size_t namelen = strlen(name) ;

    char tmp[srclen + SS_RESOLVE_LEN + 1 + namelen + 1] ;
    auto_strings(tmp, src, SS_RESOLVE, "/", name) ;

    r = scan_mode(tmp,S_IFREG) ;
    if (r <= 0)
        return 0 ;

    return 1 ;
}

int resolve_append(genalloc *ga, resolve_wrapper_t *wres)
{
    log_flow() ;

    int e = 0 ;

    if (wres->type == DATA_SERVICE) {

        resolve_service_t cp = RESOLVE_SERVICE_ZERO ;
        if (!service_resolve_copy(&cp, ((resolve_service_t *)wres->obj)))
            goto err ;

        if (!genalloc_append(resolve_service_t, ga, &cp))
            goto err ;

    } else if (wres->type == DATA_TREE) {

        resolve_tree_t cp = RESOLVE_TREE_ZERO ;
        if (!tree_resolve_copy(&cp, ((resolve_tree_t *)wres->obj)))
            goto err ;

        if (!genalloc_append(resolve_tree_t, ga, &cp))
            goto err ;

    } else if (wres->type == DATA_TREE_MASTER) {

            resolve_tree_master_t cp = RESOLVE_TREE_MASTER_ZERO ;

            if (!tree_resolve_master_copy(&cp, ((resolve_tree_master_t *)wres->obj)))
                goto err ;

            if (!genalloc_append(resolve_tree_master_t, ga, &cp))
                goto err ;
    }

    e = 1 ;
    err:
        return e ;
}

int resolve_search(genalloc *ga, char const *name, uint8_t type)
{
    log_flow() ;

    size_t len, pos = 0 ;

    if (type == DATA_SERVICE) {

        len = genalloc_len(resolve_service_t, ga) ;

        for (;pos < len ; pos++) {

            char *s = genalloc_s(resolve_service_t,ga)[pos].sa.s + genalloc_s(resolve_service_t,ga)[pos].name ;
            if (!strcmp(name,s))
                return pos ;
        }

    } else if (type == DATA_TREE) {

        len = genalloc_len(resolve_tree_t, ga) ;

        for (;pos < len ; pos++) {

            char *s = genalloc_s(resolve_tree_t,ga)[pos].sa.s + genalloc_s(resolve_tree_t,ga)[pos].name ;
            if (!strcmp(name,s))
                return pos ;
        }
    }

    return -1 ;
}

int resolve_cmp(genalloc *ga, char const *name, uint8_t type)
{
    log_flow() ;

    size_t len, pos = 0 ;

    if (type == DATA_SERVICE) {

        len = genalloc_len(resolve_service_t, ga) ;

        for (;pos < len ; pos++) {

            char *str = genalloc_s(resolve_service_t, ga)[pos].sa.s ;
            char *s = str + genalloc_s(resolve_service_t, ga)[pos].name ;
            if (!strcmp(name,s))
                return 1 ;
        }

    } else if (type == DATA_TREE) {

        len = genalloc_len(resolve_tree_t, ga) ;

        for (;pos < len ; pos++) {

            char *str = genalloc_s(resolve_tree_t, ga)[pos].sa.s ;
            char *s = str + genalloc_s(resolve_tree_t, ga)[pos].name ;
            if (!strcmp(name,s))
                return 1 ;
        }
    }

    return 0 ;
}

void resolve_rmfile(char const *src,char const *name)
{
    log_flow() ;

    size_t srclen = strlen(src) ;
    size_t namelen = strlen(name) ;

    char tmp[srclen + SS_RESOLVE_LEN + 1 + namelen +1] ;
    auto_strings(tmp, src, SS_RESOLVE, "/", name) ;

    unlink_void(tmp) ;
}

ssize_t resolve_add_string(resolve_wrapper_t *wres, char const *data)
{
    log_flow() ;

    RESOLVE_SET_SAWRES(wres) ;

    ssize_t baselen = sawres->len ;

    if (!data) {

        if (!stralloc_catb(sawres,"",1))
            log_warnusys_return(LOG_EXIT_LESSONE,"stralloc") ;

        return baselen ;
    }

    size_t datalen = strlen(data) ;

    if (!stralloc_catb(sawres,data,datalen + 1))
        log_warnusys_return(LOG_EXIT_LESSONE,"stralloc") ;

    return baselen ;
}

int resolve_modify_field(resolve_wrapper_t_ref wres, uint8_t field, char const *by)
{
    log_flow() ;

    int e = 0 ;

    resolve_wrapper_t_ref mwres = 0 ;

    if (wres->type == DATA_SERVICE) {

        resolve_service_t_ref res = (resolve_service_t *)wres->obj  ;
        mwres = resolve_set_struct(DATA_SERVICE, res) ;

        log_trace("modify field ", resolve_service_field_table[field].field," of service ", res->sa.s + res->name, " with value: ", by) ;

        if (!service_resolve_modify_field(res, field, by))
            goto err ;

    } else if (wres->type == DATA_TREE) {

        resolve_tree_t_ref res = (resolve_tree_t *)wres->obj  ;
        mwres = resolve_set_struct(DATA_TREE, res) ;

        log_trace("modify field ", resolve_tree_field_table[field].field," of tree ", res->sa.s + res->name, " with value: ", by) ;

        if (!tree_resolve_modify_field(res, field, by))
            goto err ;

    } else if (wres->type == DATA_TREE_MASTER) {

        resolve_tree_master_t_ref res = (resolve_tree_master_t *)wres->obj  ;
        mwres = resolve_set_struct(DATA_TREE_MASTER, res) ;

        log_trace("modify field ", resolve_tree_master_field_table[field].field," of inner resolve file of trees with value: ", by) ;

        if (!tree_resolve_master_modify_field(res, field, by))
            goto err ;
    }

    e = 1 ;
    err:
        free(mwres) ;
        return e ;
}

int resolve_modify_field_g(resolve_wrapper_t_ref wres, char const *base, char const *element, uint8_t field, char const *value)
{

    log_flow() ;

    size_t baselen = strlen(base), tot = baselen + SS_SYSTEM_LEN + 1, treelen = 0 ;
    char *treename = 0 ;

    if (wres->type == DATA_SERVICE) {
        treename = ((resolve_service_t *)wres->obj)->sa.s + ((resolve_service_t *)wres->obj)->treename ;
        treelen = strlen(treename) ;
        tot += treelen + SS_SVDIRS_LEN + 1 ;

    }

    char solve[tot] ;

    if (wres->type == DATA_SERVICE) {

        auto_strings(solve, base, SS_SYSTEM, "/", treename, SS_SVDIRS) ;

    } else if (wres->type == DATA_TREE || wres->type == DATA_TREE_MASTER) {

        auto_strings(solve, base, SS_SYSTEM) ;
    }

    if (!resolve_read(wres, solve, element))
        log_warnusys_return(LOG_EXIT_ZERO, "read resolve file of: ", solve, "/", element) ;

    if (!resolve_modify_field(wres, field, value))
        log_warnusys_return(LOG_EXIT_ZERO, "modify resolve file of: ", solve, "/", element) ;

    if (!resolve_write(wres, solve, element))
        log_warnusys_return(LOG_EXIT_ZERO, "write resolve file of :", solve, "/", element) ;

    return 1 ;

}

/**
 *
 * FREED
 *
 * */

void resolve_free(resolve_wrapper_t *wres)
{
    log_flow() ;

    RESOLVE_SET_SAWRES(wres) ;

    stralloc_free(sawres) ;

    free(wres) ;
}

void resolve_deep_free(uint8_t type, genalloc *g)
{
    log_flow() ;

    size_t pos = 0 ;

    if (type == DATA_SERVICE) {

        for (; pos < genalloc_len(resolve_service_t, g) ; pos++)
            stralloc_free(&genalloc_s(resolve_service_t, g)[pos].sa) ;

        genalloc_free(resolve_service_t, g) ;

    } else if (type == DATA_TREE) {

        for (; pos < genalloc_len(resolve_tree_t, g) ; pos++)
            stralloc_free(&genalloc_s(resolve_tree_t, g)[pos].sa) ;

         genalloc_free(resolve_tree_t, g) ;

    } else if (type == DATA_TREE_MASTER) {

        for (; pos < genalloc_len(resolve_tree_master_t, g) ; pos++)
            stralloc_free(&genalloc_s(resolve_tree_master_t, g)[pos].sa) ;

         genalloc_free(resolve_tree_master_t, g) ;
    }

}

/**
 *
 * CDB
 *
 * */

int resolve_read_cdb(resolve_wrapper_t *wres, char const *name)
{
    log_flow() ;

    int fd, e = 0 ;
    cdb c = CDB_ZERO ;

    fd = open_readb(name) ;
    if (fd < 0) {
        log_warnusys("open: ",name) ;
        goto err_fd ;
    }

    if (!cdb_init_fromfd(&c, fd)) {
        log_warnusys("cdb_init: ", name) ;
        goto err ;
    }

    if (wres->type == DATA_SERVICE) {

        if (!service_read_cdb(&c, ((resolve_service_t *)wres->obj)))
            goto err ;

    } else if (wres->type == DATA_TREE){

        if (!tree_read_cdb(&c, ((resolve_tree_t *)wres->obj)))
            goto err ;

    } else if (wres->type == DATA_TREE_MASTER) {

        if (!tree_read_master_cdb(&c, ((resolve_tree_master_t *)wres->obj)))
            goto err ;
    }

    e = 1 ;

    err:
        close(fd) ;
    err_fd:
        cdb_free(&c) ;
        return e ;
}

int resolve_write_cdb(resolve_wrapper_t *wres, char const *dst, char const *name)
{
    log_flow() ;

    int fd ;
    size_t dstlen = strlen(dst), namelen = strlen(name);
    cdbmaker c = CDBMAKER_ZERO ;
    char tfile[dstlen + 1 + namelen + namelen + 9] ;
    char dfile[dstlen + 1 + namelen + 1] ;

    auto_strings(dfile,dst,"/",name) ;

    auto_strings(tfile,dst,"/",name,":",name,":","XXXXXX") ;

    fd = mkstemp(tfile) ;
    if (fd < 0 || ndelay_off(fd)) {
        log_warnusys("mkstemp: ", tfile) ;
        goto err_fd ;
    }

    if (!cdbmake_start(&c, fd)) {
        log_warnusys("cdbmake_start") ;
        goto err ;
    }

    if (wres->type == DATA_SERVICE) {

        if (!service_write_cdb(&c, ((resolve_service_t *)wres->obj)))
            goto err ;

    } else if (wres->type == DATA_TREE) {

        if (!tree_write_cdb(&c, ((resolve_tree_t *)wres->obj)))
            goto err ;

    } else if (wres->type == DATA_TREE_MASTER) {

        if (!tree_write_master_cdb(&c, ((resolve_tree_master_t *)wres->obj)))
            goto err ;

    }

    if (!cdbmake_finish(&c) || fsync(fd) < 0) {
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

int resolve_add_cdb(cdbmaker *c, char const *key, char const *data)
{
    log_flow() ;

    size_t klen = strlen(key) ;
    size_t dlen = strlen(data) ;

    if (!cdbmake_add(c,key,klen,dlen ? data : 0,dlen))
        log_warnsys_return(LOG_EXIT_ZERO,"cdb_make_add: ",key) ;

    return 1 ;
}

int resolve_add_cdb_uint(cdbmaker *c, char const *key, uint32_t data)
{
    log_flow() ;

    char pack[4] ;
    size_t klen = strlen(key) ;

    uint32_pack_big(pack, data) ;
    if (!cdbmake_add(c,key,klen,pack,4))
        log_warnsys_return(LOG_EXIT_ZERO,"cdb_make_add: ",key) ;

    return 1 ;
}

int resolve_find_cdb(stralloc *result, cdb const *c, char const *key)
{
    log_flow() ;

    uint32_t x = 0 ;
    size_t klen = strlen(key) ;
    cdb_data cdata ;

    result->len = 0 ;

    int r = cdb_find(c, &cdata, key, klen) ;
    if (r == -1)
        log_warnusys_return(LOG_EXIT_LESSONE,"search on cdb key: ",key) ;

    if (!r)
        log_warn_return(LOG_EXIT_ZERO,"unknown cdb key: ",key) ;

    char pack[cdata.len + 1] ;
    memcpy(pack,cdata.s, cdata.len) ;
    pack[cdata.len] = 0 ;

    uint32_unpack_big(pack, &x) ;

    if (!auto_stra(result,pack))
        log_warnusys_return(LOG_EXIT_LESSONE,"stralloc") ;

    return x ;
}

