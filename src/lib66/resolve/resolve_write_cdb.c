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
#include <sys/stat.h>

#include <oblibs/log.h>
#include <oblibs/io.h>
#include <oblibs/string.h>
#include <oblibs/cdb.h>
#include <oblibs/files.h>
#include <oblibs/fd.h>
#include <oblibs/directory.h>
#include <oblibs/strbuf.h>

#include <66/constants.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/tree.h>
#include <66/utils.h>
#include <66/config.h>

/** The ssexec_scandir_create takes care of the SS_LIVE SS_LIVE_TMP
 * by itself but it will only work if the machine was booted with 66.
 *
 * Command on chrooted system e.g. from an installer will fail on an non-existant
 * directory.
 * Booting on a ro filesystem will also fail, the temporary directory
 * must be writable.
 *
 * Best-effort here when the machine was not booted and root
 * was never used, typically on a container. A user cannot create
 * anything under /run/66, which belongs to root, so its own base
 * directory is used instead */
static int create_livetmp(strbuf *dir)
{
    log_flow() ;

    if (!auto_strbuf(dir, SS_LIVE SS_LIVE_TMP))
        log_warnusys_return(LOG_EXIT_ZERO, "strbuf") ;

    if (scan_mode(dir->s, S_IFDIR) > 0)
        return 1 ;

    if (getuid()) {

        dir->len = 0 ;
        if (!set_ownersysdir(dir, getuid()) || !auto_strbuf(dir, SS_LIVE_TMP))
            log_warnusys_return(LOG_EXIT_ZERO, "set owner directory") ;

        if (scan_mode(dir->s, S_IFDIR) > 0)
            return 1 ;
    }

    log_trace("create temporary directory: ", dir->s) ;

    if (!dir_create_parent(dir->s, 0755))
        log_warnusys_return(LOG_EXIT_ZERO, "create directory: ", dir->s) ;

    if (!getuid() && chmod(dir->s, S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO) < 0)
        log_warnusys_return(LOG_EXIT_ZERO, "chmod: ", dir->s) ;

    return 1 ;
}

int resolve_write_cdb(resolve_wrapper_t *wres, const char *path, const char *name)
{
    log_flow() ;

    int fd = -1 ;
    size_t pathlen = strlen(path), namelen = strlen(name) ;
    ocdbmaker c = OCDBMAKER_ZERO ;
    char file[pathlen + namelen + 1] ;
    _alloc_strbuf_(tfile, SS_MAX_PATH) ;

    auto_strings(file, path, name) ;

    if (!create_livetmp(&tfile))
        return 0 ;

    if (!auto_strbuf(&tfile, "/", name, ":XXXXXX"))
        log_warnusys_return(LOG_EXIT_ZERO, "strbuf") ;

    fd = mkstemp(tfile.s) ;
    if (fd < 0 || !io_set_block(fd)) {
        log_warnusys("mkstemp: ", tfile.s) ;
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

    } else if (wres->type == DATA_SERVICE_EXECUTE) {

        if (!service_resolve_write_addon_execute_cdb(&c, ((resolve_service_addon_execute_t *)wres->obj)))
            goto err ;

    } else if (wres->type == DATA_SERVICE_DEPENDENCIES) {

        if (!service_resolve_write_addon_dependencies_cdb(&c, ((resolve_service_addon_dependencies_t *)wres->obj)))
            goto err ;

    } else if (wres->type == DATA_SERVICE_REGEX) {

        if (!service_resolve_write_addon_regex_cdb(&c, ((resolve_service_addon_regex_t *)wres->obj)))
            goto err ;

    } else if (wres->type == DATA_SERVICE_EVENT) {

        if (!service_resolve_write_addon_event_cdb(&c, ((resolve_service_addon_event_t *)wres->obj)))
            goto err ;
    }

    if (!ocdb_make_finish(&c) || fsync(fd) < 0) {
        log_warnusys("write to: ", tfile.s) ;
        goto err ;
    }

    close_fd(fd) ;

    if (!file_copy(tfile.s, file, 0600)) {
        log_warnusys("copy: ", tfile.s, " to ", file) ;
        goto err_fd ;
    }

    file_tryunlink(tfile.s) ;

    return 1 ;

    err:
        close_fd(fd) ;
    err_fd:
        file_tryunlink(tfile.s) ;
        return 0 ;
}
