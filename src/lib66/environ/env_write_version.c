/*
 * env_write_version.c
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

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/strbuf.h>

#include <66/environ.h>
#include <66/service.h>

int env_write_version(char const *dir, char const *previous, resolve_service_t *res, resolve_service_addon_environ_t *e)
{
    log_flow() ;

    char const *name = res->sa.s + res->name ;
    _cleanup_strbuf_ strbuf contents = STRBUF_ZERO ;
    char file[strlen(name) + 2] ;

    // versions that do not compare are the fault of the frontend: the write fails
    if (previous && !e->env_overwrite && env_import_version_file(name, e->sa.s + e->envdir, previous, res->sa.s + res->version, dir, res->type) <= 0)
        return -1 ;

    if (!env_compute(&contents, res, e))
        return -1 ;

    auto_strings(file, ".", name) ;

    log_trace("write environment file: ", dir, "/", file) ;
    return file_write_at(dir, file, contents.s, contents.len) ? 1 : -1 ;
}
