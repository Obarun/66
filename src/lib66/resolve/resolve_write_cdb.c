/*
 * resolve_write_cdb.c
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
#include <stdlib.h>
#include <unistd.h>

#include <oblibs/log.h>
#include <oblibs/io.h>
#include <oblibs/string.h>
#include <oblibs/cdb.h>
#include <oblibs/files.h>
#include <oblibs/fd.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/tree.h>

int resolve_write_cdb(resolve_wrapper_t *wres, const char *path, const char *name)
{
    log_flow() ;

    int fd ;
    size_t pathlen = strlen(path), namelen = strlen(name) ;
    ocdbmaker c = OCDBMAKER_ZERO ;
    char file[pathlen + namelen + 1] ;
    char tfile[5 + strlen(name) + 8] ;

    auto_strings(file, path, name) ;
    auto_strings(tfile, "/tmp/", name, ":", "XXXXXX") ;

    fd = mkstemp(tfile) ;
    if (fd < 0 || !io_set_block(fd)) {
        log_warnusys("mkstemp: ", tfile) ;
        goto err_fd ;
    }

    if (!ocdb_make_start(&c, fd)) {
        log_warnusys("cdbmake_start") ;
        goto err ;
    }

    if (wres->type == DATA_SERVICE) {

        if (!service_resolve_write_cdb(&c, ((resolve_service_t *)wres->obj)))
            goto err ;

    } else if (wres->type == DATA_TREE) {

        if (!tree_resolve_write_cdb(&c, ((resolve_tree_t *)wres->obj)))
            goto err ;

    } else if (wres->type == DATA_TREE_MASTER) {

        if (!tree_resolve_master_write_cdb(&c, ((resolve_tree_master_t *)wres->obj)))
            goto err ;

    } else if (wres->type == DATA_SERVICE_LIMIT) {

        if (!service_resolve_write_addon_limit_cdb(&c, ((resolve_service_addon_limit_t *)wres->obj)))
            goto err ;

    } else if (wres->type == DATA_SERVICE_ENVIRON) {

        if (!service_resolve_write_addon_environ_cdb(&c, ((resolve_service_addon_environ_t *)wres->obj)))
            goto err ;

    } else if (wres->type == DATA_SERVICE_IO) {

        if (!service_resolve_write_addon_io_cdb(&c, ((resolve_service_addon_io_t *)wres->obj)))
            goto err ;

    } else if (wres->type == DATA_SERVICE_LOGGER) {

        if (!service_resolve_write_addon_logger_cdb(&c, ((resolve_service_addon_logger_t *)wres->obj)))
            goto err ;

    } else if (wres->type == DATA_SERVICE_EXECUTE) {

        if (!service_resolve_write_addon_execute_cdb(&c, ((resolve_service_addon_execute_t *)wres->obj)))
            goto err ;

    } else if (wres->type == DATA_SERVICE_DEPENDENCIES) {

        if (!service_resolve_write_addon_dependencies_cdb(&c, ((resolve_service_addon_dependencies_t *)wres->obj)))
            goto err ;

    } else if (wres->type == DATA_SERVICE_REGEX) {

        if (!service_resolve_write_addon_regex_cdb(&c, ((resolve_service_addon_regex_t *)wres->obj)))
            goto err ;
    }

    if (!ocdb_make_finish(&c) || fsync(fd) < 0) {
        log_warnusys("write to: ", tfile) ;
        goto err ;
    }

    close_fd(fd) ;

    if (!file_copy(tfile, file, 0600)) {
        log_warnusys("copy: ", tfile, " to ", file) ;
        goto err_fd ;
    }

    file_tryunlink(tfile) ;

    return 1 ;

    err:
        close_fd(fd) ;
    err_fd:
        file_tryunlink(tfile) ;
        return 0 ;
}
