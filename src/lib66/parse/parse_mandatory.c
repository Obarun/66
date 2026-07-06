/*
 * parse_mandatory.c
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


#include <string.h>
#include <stdint.h>
#include <pwd.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/enum_parser.h>
#include <66/constants.h>
#include <66/config.h>
#include <66/service.h>

static int get_shebang(strbuf *stk, char const *line)
{
    size_t len = strlen(line) ;
    uint32_t i = 0 ;

    while (line[i] == ' ' || line[i] == '\t' || line[i] == '\r' || line[i] == '\n')
        i++ ;

    if (i >= len || line[i] != '#' || line[i + 1] != '!')
        return 0 ;

    if (!sbl_addb(stk, line + i, len - i))
        log_warnsys_return(LOG_EXIT_LESSONE, "stack add") ;

    return 1 ;
}

int parse_mandatory(resolve_service_t *res, resolve_service_addon_logger_t *lg, resolve_service_addon_execute_t *ex, ssexec_t *info)
{
    log_flow() ;

    _cleanup_wres_ resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;
    _cleanup_wres_ resolve_wrapper_t_ref exwres = resolve_set_struct(DATA_SERVICE_EXECUTE, ex) ;

    if (!res->description) {

        char d[strlen(res->sa.s + res->name) + 8 + 1] ;
        auto_strings(d, res->sa.s + res->name, " service") ;
        res->description = resolve_add_string(wres, d) ;
        log_warn("key Description at section [Main] was not set -- define it to: ", d) ;
    }

    if (!res->user) {

        /** The service_frontend_path function restricts the frontend file's location to
         * $HOME, SS_SERVICE_ADMDIR_USER, or SS_SERVICE_SYSDIR_USER for a user service.
         * Consequently, the process owner's account can be used as the default
         * to determine which account manages the parsing process.
         * */

        if (!info->owner) {

            res->user = resolve_add_string(wres, "root") ;
            log_info("key User at section [Main] was not set -- define it to: root") ;

        } else {

            struct passwd *pw = getpwuid(info->owner);
            if (!pw) {
                if (!errno) errno = ESRCH ;
                    log_warnu_return(LOG_EXIT_ZERO,"get user name") ;
            }
            res->user = resolve_add_string(wres, pw->pw_name) ;
            log_info("key User at section [Main] was not set -- define it to: ", pw->pw_name) ;
        }
    }

    if (!res->version) {

        res->version = resolve_add_string(wres, SS_VERSION) ;
        log_warn("key Version at section [Main] was not set -- define it to: ", SS_VERSION) ;
    }

    switch (res->type) {

        case E_PARSER_TYPE_CLASSIC:
        case E_PARSER_TYPE_ONESHOT:

            if (!ex->run.run_user)
                log_warn_return(LOG_EXIT_ZERO,"key Execute at section [Start] must be set") ;

            {
                size_t len = strlen(ex->sa.s + ex->run.run_user) ;
                _alloc_sbl_(stk, len) ;

                int r = get_shebang(&stk, ex->sa.s + ex->run.run_user) ;
                if (r < 0)
                    return 0 ;
                if (r) {
                    ex->run.run_user = resolve_add_string(exwres, stk.s) ;
                    ex->run.build = resolve_add_string(exwres, "custom") ;
                }
            }

            if (ex->finish.run_user) {

                size_t len = strlen(ex->sa.s + ex->finish.run_user) ;
                _alloc_sbl_(stk, len) ;

                int r = get_shebang(&stk, ex->sa.s + ex->finish.run_user) ;
                if (r < 0)
                    return 0 ;
                if (r) {
                    ex->finish.run_user = resolve_add_string(exwres, stk.s) ;
                    ex->finish.build = resolve_add_string(exwres, "custom") ;
                }
            }

            if (lg->execute.run.run_user) {

                _cleanup_wres_ resolve_wrapper_t_ref lgwres = resolve_set_struct(DATA_SERVICE_LOGGER, lg) ;

                size_t len = strlen(lg->sa.s + lg->execute.run.run_user) ;
                _alloc_sbl_(stk, len) ;

                int r = get_shebang(&stk, lg->sa.s + lg->execute.run.run_user) ;
                if (r < 0)
                    return 0 ;
                if (r) {
                    lg->execute.run.run_user = resolve_add_string(lgwres, stk.s) ;
                    lg->execute.run.build = resolve_add_string(lgwres, "custom") ;
                }
            }

            break ;

        case E_PARSER_TYPE_MODULE:
        default:
            break ;
    }
    return 1 ;
}
