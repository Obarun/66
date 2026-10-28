/*
 * command_refuse_unknown.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <string.h>
#include <stdbool.h>
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/strbuf.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/ssexec.h>
#include <66/service.h>
#include <66/module.h>
#include <66/command.h>

void command_refuse_unknown(char const *name, char const *base, uid_t owner, bool literal)
{
    log_flow() ;

    int r ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    if (!name[0] || strlen(name) > SS_MAX_SERVICE_NAME)
        log_die(LOG_EXIT_USER, SSEXEC_MSG_UNKNOWN(name)) ;

    if (literal) {

        char holder[SS_MAX_SERVICE_NAME + 1] ;

        r = service_resolve_provide(holder, name, 0, base) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "resolve the provider of: ", name) ;

        if (r)
            log_die(LOG_EXIT_USER, "name: ", name, " is provided by: ", holder, " -- '66 remove ", holder, "' instead") ;
    }

    char const *exclude[2] = { SS_MODULE_ACTIVATED + 1, SS_MODULE_FRONTEND + 1 } ;
    r = service_frontend_path(&sa, name, owner, 0, exclude, 2) ;
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "find the frontend of: ", name) ;

    if (r)
        log_die(LOG_EXIT_USER, SSEXEC_MSG_UNPARSED(name)) ;

    log_die(LOG_EXIT_USER, SSEXEC_MSG_UNKNOWN(name)) ;
}
