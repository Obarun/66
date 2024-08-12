/*
 * parse_compute_scripts.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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
#include <stdlib.h>

#include <oblibs/string.h>
#include <oblibs/log.h>

#include <66/service.h>
#include <66/resolve.h>
#include <66/config.h>

#ifndef FAKELEN
#define FAKELEN strlen(run)
#endif

/**
 * @!runorfinish -> finish, @runorfinish -> run
 * */
static void compute_wrapper_scripts(resolve_service_t *res, uint8_t runorfinish)
{
    log_flow() ;

    resolve_service_addon_scripts_t *script = runorfinish ? &res->execute.run : &res->execute.finish ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;
    char *shebang = "#!" SS_EXECLINE_SHEBANGPREFIX "execlineb -" ;
    /** TODO:
     *  -v${VERBOSITY} should use to correspond to the
     *  request of the user.
     *   */
    char *exec = SS_EXTLIBEXECPREFIX "66-execute" ;
    char run[strlen(shebang) + 3 + strlen(exec) + 7 + strlen(res->sa.s + res->name) + 4 + 1] ;

    auto_strings(run, \
        shebang, (!runorfinish) ? ((res->type == TYPE_CLASSIC) ? "S0\n" : "P\n") : "P\n", \
        exec, \
        !runorfinish ? " stop " : " start ", \
        res->sa.s + res->name, (!runorfinish) ? " $@\n" : "\n") ;

    script->run = resolve_add_string(wres, run) ;

    free(wres) ;
}

/**
 * @!runorfinish -> finish.user, @runofinish -> run.user
 * */
static void compute_wrapper_scripts_user(resolve_service_t *res, uint8_t runorfinish)
{

    log_flow() ;

    char *shebang = "#!" SS_EXECLINE_SHEBANGPREFIX "execlineb -P" ;
    size_t fakelen = 0, shebanglen = strlen(shebang) ;
    resolve_service_addon_scripts_t *script = runorfinish ? &res->execute.run : &res->execute.finish ;
    size_t scriptlen = strlen(res->sa.s + script->run_user) ;
    int build = !strcmp(res->sa.s + script->build, "custom") ? BUILD_CUSTOM : BUILD_AUTO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    char run[shebanglen + 1 + scriptlen + 1 + 1] ;

    if (!build) {
        auto_strings(run, shebang, "\n") ;
        fakelen = FAKELEN ;
    }

    if (script->run_user)
        auto_strings(run + fakelen, res->sa.s + script->run_user, "\n") ;

    script->run_user = resolve_add_string(wres, run) ;

    free(wres) ;
}

void parse_compute_scripts(resolve_service_t *res)
{
    if (res->type != TYPE_MODULE) {

        compute_wrapper_scripts(res, 1) ; // run
        compute_wrapper_scripts_user(res, 1) ; // run.user

        if (res->execute.finish.run_user) {
            compute_wrapper_scripts(res, 0) ; // finish
            compute_wrapper_scripts_user(res, 0) ; // finish.user
        }
    }
}
