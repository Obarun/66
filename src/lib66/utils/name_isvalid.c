/*
 * name_isvalid.c
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

#include <string.h>

#include <oblibs/log.h>

#include <66/constants.h>
#include <66/utils.h>

void name_isvalid(char const *name)
{
    log_flow() ;

    if (!memcmp(name, SS_MASTER + 1, 6))
        log_die(LOG_EXIT_USER, "service name: ", name, ": starts with reserved prefix Master") ;

    if (!strcmp(name, SS_SERVICE + 1))
        log_die(LOG_EXIT_USER, "service as service name is a reserved name") ;

    if (!strcmp(name, SS_SERVICE "@"))
        log_die(LOG_EXIT_USER, "service@ as service name is a reserved name") ;
}
