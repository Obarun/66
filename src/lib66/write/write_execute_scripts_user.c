/*
 * write_execute_scripts_user.c
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
#include <sys/stat.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/files.h>

#include <66/service.h>
#include <66/config.h>
#include <66/enum.h>

#ifndef FAKELEN
#define FAKELEN strlen(run)
#endif

void write_execute_scripts_user(resolve_service_t *res, resolve_service_addon_scripts_t *scripts, char const *file, char const *dst)
{
    log_flow() ;

    char *shebang = scripts->shebang ? res->sa.s + scripts->shebang : SS_EXECLINE_SHEBANGPREFIX "execlineb -P\n" ;
    size_t shebanglen = strlen(shebang) ;
    size_t scriptlen = strlen(res->sa.s + scripts->run_user) ;
    char run[shebanglen + scriptlen + 4 + 1] ;
    int build = !strcmp(res->sa.s + scripts->build, "custom") ? 1 : 0 ;
    char write[strlen(dst) + 1 + strlen(file) + 1] ;

    auto_strings(write, dst, "/", file) ;

    auto_strings(run, "#!") ;

    if (build && scripts->shebang)
        auto_strings(run + FAKELEN, res->sa.s + scripts->shebang, "\n") ;
    else
        auto_strings(run + FAKELEN, shebang, "\n") ;

    auto_strings(run + FAKELEN, res->sa.s + scripts->run_user, "\n") ;

    if (!file_write_unsafe(dst, file, run, FAKELEN))
        log_dieusys(LOG_EXIT_SYS, "write: ", dst, "/", file) ;

    if (chmod(write, 0755) < 0)
        log_dieusys(LOG_EXIT_SYS, "chmod", write) ;

}
