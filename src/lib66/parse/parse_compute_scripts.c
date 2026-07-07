/*
 * parse_compute_scripts.c
 *
 * Copyright (c) 2024 Eric Vidal <eric@obarun.org>
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
#include <string.h>

#include <oblibs/string.h>
#include <oblibs/log.h>

#include <66/service.h>
#include <66/resolve.h>
#include <66/enum_parser.h>
#include <66/config.h>

#ifndef FAKELEN
#define FAKELEN strlen(run)
#endif

/**
 * @!runorfinish -> finish, @runorfinish -> run
 * */
static void compute_wrapper_scripts(resolve_service_t *res, resolve_service_addon_execute_t *ex, uint8_t runorfinish)
{
    log_flow() ;

    resolve_service_addon_scripts_t *script = runorfinish ? &ex->run : &ex->finish ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_EXECUTE, ex) ;
    char *shebang = "#!" SS_EXECLINE_SHEBANGPREFIX "execlineb -" ;
    char *env = "importas -D2 VERBOSITY VERBOSITY\n" ;
    char *exec = SS_EXTLIBEXECPREFIX "66-execute -v${VERBOSITY}" ;
    char run[strlen(shebang) + 3 + strlen(env) + strlen(exec) + 7 + strlen(res->sa.s + res->name) + 4 + 1] ;

    auto_strings(run, \
        shebang, (!runorfinish) ? ((res->type == E_PARSER_TYPE_CLASSIC) ? "S0\n" : "P\n") : "P\n", \
        env,
        exec, \
        !runorfinish ? " stop " : " start ", \
        res->sa.s + res->name, (!runorfinish) ? " $@\n" : "\n") ;

    script->run = resolve_add_string(wres, run) ;

    free(wres) ;
}

/**
 * @!runorfinish -> finish.user, @runofinish -> run.user
 * */
static void compute_wrapper_scripts_user(resolve_service_addon_execute_t *ex, uint8_t runorfinish)
{

    log_flow() ;

    char *shebang = "#!" SS_EXECLINE_SHEBANGPREFIX "execlineb -P" ;
    size_t fakelen = 0, shebanglen = strlen(shebang) ;
    resolve_service_addon_scripts_t *script = runorfinish ? &ex->run : &ex->finish ;
    size_t scriptlen = strlen(ex->sa.s + script->run_user) ;
    int build = !strcmp(ex->sa.s + script->build, "custom") ? E_PARSER_BUILD_CUSTOM : E_PARSER_BUILD_AUTO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_EXECUTE, ex) ;

    char run[shebanglen + 1 + scriptlen + 1 + 1] ;

    if (!build) {
        auto_strings(run, shebang, "\n") ;
        fakelen = FAKELEN ;
    }

    if (script->run_user)
        auto_strings(run + fakelen, ex->sa.s + script->run_user, "\n") ;

    script->run_user = resolve_add_string(wres, run) ;

    free(wres) ;
}

void parse_compute_script(resolve_service_t *res, resolve_service_addon_execute_t *ex, uint8_t runorfinish)
{
    compute_wrapper_scripts(res, ex, runorfinish) ;
    compute_wrapper_scripts_user(ex, runorfinish) ;
}
