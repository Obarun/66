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

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sastr.h>

#include <skalibs/genalloc.h>
#include <skalibs/stralloc.h>
#include <skalibs/types.h>

#include <66/constants.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/state.h>

int service_resolve_add_deps(genalloc *tokeep, resolve_service_t *res, char const *src)
{
    log_flow() ;

    int e = 0 ;
    size_t pos = 0 ;
    stralloc tmp = STRALLOC_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    char *name = res->sa.s + res->name ;
    char *deps = res->sa.s + res->depends ;
    if (!resolve_cmp(tokeep, name, DATA_SERVICE) && (!obstr_equal(name,SS_MASTER+1)))
        if (!resolve_append(tokeep,wres)) goto err ;

    if (res->ndepends)
    {
        if (!sastr_clean_string(&tmp,deps)) return 0 ;
        for (;pos < tmp.len ; pos += strlen(tmp.s + pos) + 1)
        {
            resolve_service_t dres = RESOLVE_SERVICE_ZERO ;
            resolve_wrapper_t_ref dwres = resolve_set_struct(DATA_SERVICE, &dres) ;
            char *dname = tmp.s + pos ;
            if (!resolve_check(src,dname)) goto err ;
            if (!resolve_read(dwres,src,dname)) goto err ;
            if (dres.ndepends && !resolve_cmp(tokeep, dname, DATA_SERVICE))
            {
                if (!service_resolve_add_deps(tokeep,&dres,src)) goto err ;
            }
            if (!resolve_cmp(tokeep, dname, DATA_SERVICE))
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
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;
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

    if (!resolve_cmp(tokeep, name, DATA_SERVICE) && (!obstr_equal(name,SS_MASTER+1)))
    {
        if (!resolve_append(tokeep,wres)) goto err ;
    }
    if ((res->type == TYPE_BUNDLE || res->type == TYPE_MODULE) && res->ndepends)
    {
        uint32_t deps = res->type == TYPE_MODULE ? res->contents : res->depends ;
        if (!sastr_clean_string(&tmp,res->sa.s + deps)) goto err ;
        resolve_service_t dres = RESOLVE_SERVICE_ZERO ;
        resolve_wrapper_t_ref dwres = resolve_set_struct(DATA_SERVICE, &dres) ;
        for (; a < tmp.len ; a += strlen(tmp.s + a) + 1)
        {
            char *name = tmp.s + a ;
            if (!resolve_check(src,name)) goto err ;
            if (!resolve_read(dwres,src,name)) goto err ;
            if (dres.type == TYPE_CLASSIC) continue ;
            if (!resolve_cmp(tokeep, name, DATA_SERVICE))
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
        resolve_wrapper_t_ref dwres = resolve_set_struct(DATA_SERVICE, &dres) ;
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
        if (dres.type == TYPE_BUNDLE && !dres.ndepends){ resolve_free(dwres) ; continue ; }

        if (!resolve_cmp(tokeep, dname, DATA_SERVICE))
        {
            if (dres.ndepends)// || (dres.type == TYPE_BUNDLE && dres.ndepends) || )
            {
                if (!sastr_clean_string(&tmp,dres.sa.s + dres.depends)) goto err ;
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
        resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;
        resolve_service_t dres = RESOLVE_SERVICE_ZERO ;
        resolve_wrapper_t_ref dwres = resolve_set_struct(DATA_SERVICE, &dres) ;
        if (!service_resolve_copy(&res,&genalloc_s(resolve_service_t,ga)[i])) goto err ;

        char *string = res.sa.s ;
        char *name = string + res.name ;
        if (!resolve_cmp(&gatmp, name, DATA_SERVICE))
        {
            if (!resolve_append(&gatmp,wres))
                goto err ;

            if (res.logger)
            {
                if (!resolve_check(src,string + res.logger)) goto err ;
                if (!resolve_read(dwres,src,string + res.logger))
                    goto err ;
                if (!resolve_cmp(&gatmp, string + res.logger, DATA_SERVICE))
                    if (!resolve_append(&gatmp,dwres)) goto err ;
            }
        }
        resolve_free(wres) ;
        resolve_free(dwres) ;
    }
    resolve_deep_free(DATA_SERVICE, ga) ;
    if (!genalloc_copy(resolve_service_t,ga,&gatmp)) goto err ;

    e = 1 ;

    err:
        genalloc_free(resolve_service_t,&gatmp) ;
        resolve_deep_free(DATA_SERVICE, &gatmp) ;
        return e ;
}

