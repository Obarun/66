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
        log_warn_return(LOG_EXIT_ZERO,"key Description at section [Main] must be set") ;

    if (!res->user)
        log_warn_return(LOG_EXIT_ZERO,"key User at section [Main] must be set") ;

    if (!res->version)
        log_warn_return(LOG_EXIT_ZERO,"key Version at section [Main] must be set") ;

    switch (res->type) {

        case TYPE_ONESHOT:

            if (!res->execute.run.run_user)
                log_warn_return(LOG_EXIT_ZERO,"key Execute at section [Start] must be set") ;

            break ;

        case TYPE_CLASSIC:

            if (!res->execute.run.run_user)
                log_warn_return(LOG_EXIT_ZERO,"key Execute at section [Start] must be set") ;

            break ;

        case TYPE_MODULE:

            /*

            if (!sasection->idx[SECTION_REGEX])
                log_warn_return(LOG_EXIT_ZERO,"section [Regex] must be set") ;

            if (service->type.module.iddir < 0)
                log_warn_return(LOG_EXIT_ZERO,"key Directories at section [Regex] must be set") ;

            if (service->type.module.idfiles < 0)
                log_warn_return(LOG_EXIT_ZERO,"key Files at section [Regex] must be set") ;

            if (service->type.module.start_infiles < 0)
                log_warn_return(LOG_EXIT_ZERO,"key InFiles at section [Regex] must be set") ;

            break ;

            */

        /** really nothing to do here */
        default: break ;
    }
    return 1 ;
}
