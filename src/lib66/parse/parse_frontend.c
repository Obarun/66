/*
 * parse_frontend.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h> //free
#include <sys/stat.h>


#include <oblibs/log.h>
#include <oblibs/files.h>
#include <oblibs/sbl.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>

#include <66/utils.h>
#include <66/constants.h>
#include <66/ssexec.h>
#include <66/service.h>
#include <66/config.h>
#include <66/resolve.h>
#include <66/enum_parser.h>
#include <66/state.h> // service_is_g flag
#include <66/parse.h>
#include <66/module.h>
#include <66/instance.h>

static void parse_read_instance(strbuf *frontend, char const *svsrc, char const *sv, int insta)
{
    log_flow() ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    uint8_t exlen = 0 ; // see service_frontend_path file and compute_exclude()
    char const *exclude[1] = { 0 } ;

    if (!instance_splitname(&sa, sv, insta, SS_INSTANCE_TEMPLATE))
        log_die(LOG_EXIT_SYS, "split instance service of: ", sv) ;

    log_trace("read frontend service at: ", svsrc, sa.s) ;

    if (read_svfile(frontend, sa.s, svsrc) <= 0) {

        char instaname[sa.len + 1] ;
        auto_strings(instaname, sa.s) ;
        sa.len = 0 ;
        /** in module the template service may not exist e.g.
         * module which call another module. In this case
         * follow the classic way */
        int r = service_frontend_path(&sa, sv, getuid(), 0, exclude, exlen) ;
        if (r < 1)
            log_dieu(LOG_EXIT_SYS, "get frontend service file of: ", sv) ;

        char svsrc[sa.len + 1] ;

        if (!ob_dirname(svsrc, sa.s))
            log_dieu(LOG_EXIT_SYS, "get dirname of: ", sa.s) ;

        if (read_svfile(frontend, instaname, svsrc) <= 0)
            log_dieusys(LOG_EXIT_SYS, "read frontend service at: ", svsrc, instaname) ;
    }

}

/* @sv -> name of the service to parse with
 * the path of the frontend file source
 * @Die on fail
 * @Return 1 on success
 * @Return 2 -> already parsed */

int parse_frontend(char const *sv, parse_build_ctx_t ctx)
{
    log_flow() ;

    int insta, isparsed ;
    size_t svlen = strlen(sv) ;
    char svname[svlen + 1], svsrc[svlen + 1], instaname[svlen + 1] ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    struct resolve_hash_s *hash ;

    if (!ob_basename(svname, sv))
        log_dieu(LOG_EXIT_SYS, "get basename of: ", sv) ;

    if (!ob_dirname(svsrc, sv))
        log_dieu(LOG_EXIT_SYS, "get dirname of: ", sv) ;

    char known[(ctx.inns ? strlen(ctx.inns) + 1 : 0) + svlen + 1] ;

    hash = parse_get_hashname(known, ctx.hres, svname, ctx.inns) ;
    if (hash != NULL)
        log_warn_return(2, "ignoring: ", known, " service -- already appended to the selection") ;

    log_trace("parse service: ", sv) ;

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    resolve_init(wres) ;

    insta = instance_check(svname) ;

    if (insta > 0) {

        auto_strings(instaname, svname) ;
        instaname[insta + 1] = 0 ;

        parse_read_instance(&sa, svsrc, svname, insta) ;

    } else {

        log_trace("read frontend service at: ", sv) ;

        if (read_svfile(&sa, svname, svsrc) <= 0)
            log_dieu(LOG_EXIT_SYS, "read frontend service at: ", sv) ;
    }

    if (!identifier_replace(&sa, svname))
        log_dieu(LOG_EXIT_SYS, "replace regex for service: ", svname) ;

    isparsed = service_is_g(svname, STATE_FLAGS_ISPARSED) ;
    if (isparsed == -1)
        log_dieusys(LOG_EXIT_SYS, "get information of service: ", svname, " -- please make a bug report") ;
    else if (!isparsed)
        isparsed = STATE_FLAGS_FALSE ;

    if (isparsed == STATE_FLAGS_TRUE && !ctx.force) {
        resolve_free(wres) ;
        log_warn_return(2, "ignoring service: ", svname, " -- already parsed") ;
    }

    parse_store_t st ;
    if (!parse_store_build(&st, sa.s))
        log_die(LOG_EXIT_SYS, "build parse store of service: ", svname) ;

    ctx.st = &st ;
    ctx.frontend = sa.s ;
    ctx.isparsed = isparsed ;

    int r = parse_core(&res, sv, svname, svsrc, svlen, insta, instaname, &ctx) ;
    if (!r) {
        parse_store_free(&st) ;
        resolve_free(wres) ;
        log_die(LOG_EXIT_SYS, "parse core of service: ", svname) ;
    }

    if (r == 2) {
        parse_store_free(&st) ;
        resolve_free(wres) ;
        return 1 ;
    }

    if (resolve_hash_count(ctx.hres) > SS_MAX_SERVICE)
        log_die(LOG_EXIT_SYS, "too many services to parse -- compile again 66 changing the --max-service options") ;

    char *name = res.sa.s + res.name ;
    log_trace("add service: ", name, " to the service selection") ;
    if (!resolve_hash_add(ctx.hres, name, res))
        log_dieu(LOG_EXIT_SYS, "append service selection with: ", name) ;

    free(wres) ;

    struct resolve_hash_s *c = resolve_hash_search(ctx.hres, name) ;

    parse_build(c, &ctx) ;

    parse_store_free(&st) ;

    return 1 ;
}

