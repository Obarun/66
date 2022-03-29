/*
 * service_resolve_master_write.c
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

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/genalloc.h>

#include <66/constants.h>
#include <66/resolve.h>
#include <66/graph.h>
#include <66/service.h>

int service_resolve_master_write(graph_t *graph, char const *dest)
{
    log_flow() ;

    int e = 0 ;
    unsigned int pos = 0 ;

    resolve_service_master_t mres = RESOLVE_SERVICE_MASTER_ZERO ;
    resolve_wrapper_t_ref mwres = resolve_set_struct(DATA_SERVICE_MASTER, &mres) ;

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    resolve_init(mwres) ;

    {
        for (; pos < graph->sort_count ; pos++) {

           char *name = graph->data.s + genalloc_s(graph_hash_t,&graph->hash)[graph->sort[pos]].vertex ;

            if (resolve_read_g(wres, dest, name) <= 0) {
                log_warnu("read resolve file of: ", dest, name) ;
                goto err ;
            }

            switch (res.type) {

                case TYPE_CLASSIC:

                    mres.classic = resolve_add_string(mwres, res.sa.s + res.name) ;
                    mres.nclassic++ ;
                    break ;

                case TYPE_BUNDLE:

                    mres.bundle = resolve_add_string(mwres, res.sa.s + res.name) ;
                    mres.nbundle++ ;
                    break ;

                case TYPE_LONGRUN:

                    mres.longrun = resolve_add_string(mwres, res.sa.s + res.name) ;
                    mres.nlongrun++ ;
                    break ;


                case TYPE_ONESHOT:

                    mres.oneshot = resolve_add_string(mwres, res.sa.s + res.name) ;
                    mres.noneshot++ ;
                    break ;

                case TYPE_MODULE:

                    mres.module = resolve_add_string(mwres, res.sa.s + res.name) ;
                    mres.nmodule++ ;
                    break ;

                default:
                    log_warn("unknown type") ;
                    goto err ;
            }
        }
    }

    if (!resolve_write(mwres, dest, SS_MASTER + 1))
        goto err ;

    e = 1 ;

    err:
        resolve_free(wres) ;
        resolve_free(mwres) ;
        return e ;
}

