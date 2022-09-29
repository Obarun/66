/*
 * write_execute_scripts.c
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
#include <66/constants.h>
#include <66/enum.h>

#include <s6/config.h>

#ifndef FAKELEN
#define FAKELEN strlen(run)
#endif

void write_execute_scripts(resolve_service_t *res, resolve_service_addon_scripts_t *scripts, char const *file, char const *dst)
{
    log_flow() ;

    char write[strlen(dst) + 1 + strlen(file) + 1] ;
    char *shebang = "#!" SS_EXECLINE_SHEBANGPREFIX "execlineb -P\n" ;
    size_t shebanglen = strlen(shebang) ;
    char run[shebanglen + strlen(res->sa.s + res->live.fdholderdir) + SS_FDHOLDER_PIPENAME_LEN + strlen(res->sa.s + res->name) + SS_LOG_SUFFIX_LEN + strlen(S6_BINPREFIX) + strlen(res->sa.s + scripts->runas) + strlen(res->sa.s + res->environ.envdir) + SS_SYM_VERSION_LEN + (SS_MAX_PATH*2) + SS_MAX_PATH + strlen(file) + 117 + 1] ;

    auto_strings(write, dst, "/", file) ;

    /** shebang */
    auto_strings(run, shebang) ;

    if (res->logger.name && res->type != TYPE_ONESHOT)
        auto_strings(run + FAKELEN, \
                "fdmove 1 0\n", \
                "s6-fdholder-retrieve ", \
                res->sa.s + res->live.fdholderdir, "/s ", \
                "\"" SS_FDHOLDER_PIPENAME "w-", \
                res->sa.s + res->name, SS_LOG_SUFFIX "\"\n", \
                "fdswap 0 1\n") ;


    /** environ */
    if (res->environ.env)
        auto_strings(run + FAKELEN, res->sa.s + res->environ.envdir, SS_SYM_VERSION "\n") ;

    /** log redirection for oneshot service */
    if (res->logger.name && res->type == TYPE_ONESHOT) {

        char verbo[UINT_FMT] ;
        verbo[uint_fmt(verbo, VERBOSITY)] = 0 ;

        size_t namelen = strlen(res->sa.s + res->name) ;
        size_t syslen = res->owner ? strlen(res->sa.s + res->path.home) + 1 + strlen(SS_LOGGER_USERDIR) : strlen(SS_LOGGER_SYSDIR) ;
        size_t dstlen = res->logger.destination ? strlen(res->sa.s + res->logger.destination) : strlen(SS_LOGGER_SYSDIR) ;

        char dstlog[syslen + dstlen + namelen + 1] ;

        if (!res->logger.destination) {

            if (res->owner)

                auto_strings(dstlog, res->sa.s + res->path.home, "/", SS_LOGGER_USERDIR, res->sa.s + res->name) ;

            else

                auto_strings(dstlog, SS_LOGGER_SYSDIR, res->sa.s + res->name) ;

        } else {

            auto_strings(dstlog, res->sa.s + res->logger.destination) ;
        }

        auto_strings(run + FAKELEN, "execl-toc -v", verbo, " -d ", dstlog, " -m 0755\n") ;
        auto_strings(run + FAKELEN, "redirfd -a 1 ", dstlog, "/current\n") ;
    }

    /** runas */
    if (!res->owner && scripts->runas)
        auto_strings(run + FAKELEN, S6_BINPREFIX "s6-setuidgid ", res->sa.s + scripts->runas, "\n") ;

    auto_strings(run + FAKELEN, "./", file, ".user") ;

    if (!file_write_unsafe(dst, file, run, FAKELEN))
        log_dieusys(LOG_EXIT_SYS, "write: ", dst, "/", file) ;

    if (chmod(write, 0755) < 0)
        log_dieusys(LOG_EXIT_SYS, "chmod", write) ;

}
