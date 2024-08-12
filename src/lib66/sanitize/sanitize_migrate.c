/*
 * sanitize_migrate.c
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
#include <oblibs/stack.h>

#include <66/ssexec.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/sanitize.h>

#include <66/migrate_0721.h>

/** Return 0 if no migration was made else 1 */
int sanitize_migrate(ssexec_t *info, const char *oversion, short exist)
{
    log_flow() ;
    int r ;
    r = version_compare(oversion, "0.7.2.1", SS_SYSTEM_VERSION_NDOT) ;
    if (r <= 0) {

        if (!exist) {
            migrate_0721(info) ;
            sanitize_graph(info) ;
        }
        return !r ? 0 : 1 ;
    }

    return 0 ;
}
