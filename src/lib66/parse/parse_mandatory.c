/*
 * parse_mandatory.c
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

#include <oblibs/log.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/enum.h>

int parse_mandatory(resolve_service_t *res)
{
    log_flow() ;

    if (!res->description)
        log_warn_return(LOG_EXIT_ZERO,"key @description at section [main] must be set") ;

    if (!res->user)
        log_warn_return(LOG_EXIT_ZERO,"key @user at section [main] must be set") ;

    if (!res->version)
        log_warn_return(LOG_EXIT_ZERO,"key @version at section [main] must be set") ;

    switch (res->type) {

        case TYPE_ONESHOT:

            if (res->execute.run.shebang)
                log_warn_return(LOG_EXIT_ZERO,"key @shebang is deprecated -- shebang should be define at @execute key") ;

            if (!res->execute.run.run_user)
                log_warn_return(LOG_EXIT_ZERO,"key @execute at section [start] must be set") ;

            if (res->execute.finish.shebang)
                log_warn_return(LOG_EXIT_ZERO,"key @shebang is deprecated -- shebang should be define at @execute key") ;

            break ;

        case TYPE_CLASSIC:

             if (res->execute.run.shebang)
                log_warn_return(LOG_EXIT_ZERO,"key @shebang is deprecated -- shebang should be define at @execute key") ;

            if (!res->execute.run.run_user)
                log_warn_return(LOG_EXIT_ZERO,"key @execute at section [start] must be set") ;

            if (res->execute.finish.shebang)
                log_warn_return(LOG_EXIT_ZERO,"key @shebang is deprecated -- shebang should be define at @execute key") ;

            if (res->logger.execute.finish.shebang)
                log_warn_return(LOG_EXIT_ZERO,"key @shebang is deprecated -- shebang should be define at @execute key") ;

            break ;

        case TYPE_MODULE:

            /*

            if (!sasection->idx[SECTION_REGEX])
                log_warn_return(LOG_EXIT_ZERO,"section [regex] must be set") ;

            if (service->type.module.iddir < 0)
                log_warn_return(LOG_EXIT_ZERO,"key @directories at section [regex] must be set") ;

            if (service->type.module.idfiles < 0)
                log_warn_return(LOG_EXIT_ZERO,"key @files at section [regex] must be set") ;

            if (service->type.module.start_infiles < 0)
                log_warn_return(LOG_EXIT_ZERO,"key @infiles at section [regex] must be set") ;

            break ;

            */

        /** really nothing to do here */
        default: break ;
    }
    return 1 ;
}
