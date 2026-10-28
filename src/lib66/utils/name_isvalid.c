/*
 * name_isvalid.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
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

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/utils.h>

int name_isvalid(char const *name, strbuf *msg)
{
    log_flow() ;

    int r = 1 ;

    // no Provide link, no resolve path holds a longer one
    if (strlen(name) > SS_MAX_SERVICE_NAME)
        r = auto_strbuf(msg, "service name: ", name, ": is longer than the maximum of a service name") ? 0 : -1 ;

    // <base>/.tmp and the hidden entries of 66 start with a dot (contract, Names)
    else if (name[0] == '.')
        r = auto_strbuf(msg, "service name: ", name, ": starts with a dot") ? 0 : -1 ;

    else if (!strncmp(name, SS_MASTER + 1, 6))
        r = auto_strbuf(msg, "service name: ", name, ": starts with reserved prefix Master") ? 0 : -1 ;

    else if (!strcmp(name, SS_SERVICE + 1))
        r = auto_strbuf(msg, "service as service name is a reserved name") ? 0 : -1 ;

    else if (!strcmp(name, SS_SERVICE "@"))
        r = auto_strbuf(msg, "service@ as service name is a reserved name") ? 0 : -1 ;

    else if (!strcmp(name, SS_PROVIDE + 1))
        r = auto_strbuf(msg, SS_PROVIDE + 1, " as service name is a reserved name") ? 0 : -1 ;

    // the logger of a service is made by the parse, never named by a frontend
    else if (get_rstrlen_until(name, SS_LOG_SUFFIX) >= 0)
        r = auto_strbuf(msg, "service name: ", name, ": ends with the suffix " SS_LOG_SUFFIX " reserved to the loggers") ? 0 : -1 ;

    return r ;
}
