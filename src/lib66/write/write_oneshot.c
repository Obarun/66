/*
 * write_oneshot.c
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

#include <oblibs/log.h>

#include <66/service.h>
#include <66/write.h>

void write_oneshot(resolve_service_t *res, char const *dst)
{
    log_flow() ;

    /**notification,timeout, ... */
    write_common(res, dst) ;

    /** run file */
    write_execute_scripts(res, &res->execute.run, "up", dst) ;

    /** finish file */
    if (res->execute.finish.run_user)
        write_execute_scripts(res, &res->execute.finish, "down", dst) ;

    /** run.user file */
    write_execute_scripts_user(res, &res->execute.run, "up.user", dst) ;

    /** finish.user file */
    if (res->execute.finish.run_user)
        write_execute_scripts_user(res, &res->execute.finish, "down.user", dst) ;

}
