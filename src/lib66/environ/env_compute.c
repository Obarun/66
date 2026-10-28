/*
 * env_compute.c
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

#include <stdint.h>

#include <oblibs/string.h>
#include <oblibs/log.h>
#include <oblibs/strbuf.h>

#include <66/environ.h>
#include <66/service.h>

int env_compute(strbuf *result, resolve_service_t *res, resolve_service_addon_environ_t *e)
{
    log_flow() ;

    char const *name = res->sa.s + res->name ;

    // the version directory is made, and the files of the previous version imported, by parse_write()
    if (!auto_strbuf(result, \
    "## [STARTWARN]\n## DO NOT MODIFY THIS FILE, IT OVERWRITTEN AT UPGRADE TIME.\n## Uses \'66 configure ", \
    name,"\' command instead.\n## Or make a copy of this file at ", e->sa.s + e->envdir, "/", res->sa.s + res->version, "/", name, \
    " and modify it.\n## [ENDWARN]\n", e->sa.s + e->env))
        return 0 ;

    return 1 ;
}
