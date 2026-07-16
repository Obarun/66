/*
 * umountall.c
 *
 * Copyright (c) 2019 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 *
 * This file is a modified copy of s6-linux-init-umountall.c file
 * coming from skarnet software at https://skarnet.org/software/s6-linux-init.
 * All credits goes to Laurent Bercot <ska-remove-this-if-you-are-not-a-bot@skarnet.org>
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <mntent.h>
#include <sys/mount.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>

#include <66/config.h>
#include <66/hpr.h>

#define MAXLINES 99
#define EXCLUDEN 3

int umountall (void)
{
    static char const *const exclude_type[EXCLUDEN] = { "devtmpfs", "proc", "sysfs" } ;
    size_t mountpoints[MAXLINES], tmplen = strlen(SS_LIVE) ;
    char tmpdir[tmplen + 1] ;
    unsigned int got[EXCLUDEN] = { 0, 0, 0 } ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    unsigned int line = 0 ;
    int e = 0 ;
    size_t len ;
    FILE *fp ;

    ob_dirname(tmpdir, SS_LIVE) ;
    len = strlen(tmpdir) ;
    if (len && tmpdir[len - 1] == '/')
        tmpdir[len - 1] = 0 ;

    fp = setmntent("/proc/mounts", "r") ;
    if (!fp) {
        log_warnusys("open /proc/mounts") ;
        return -1 ;
    }

    for (;;) {

        struct mntent *p ;
        unsigned int i = 0 ;
        errno = 0 ;
        p = getmntent(fp) ;
        if (!p) break ;
        if (!strcmp(p->mnt_dir, tmpdir)) continue ;
        for (; i < EXCLUDEN ; i++) {
            if (!strcmp(p->mnt_type, exclude_type[i])) {
                got[i]++ ;
                break ;
            }
        }
        if (i < EXCLUDEN && got[i] == 1) continue ;
        if (line >= MAXLINES) {
            log_warn("too many mount points - stopping the scan") ;
            break ;
        }
        mountpoints[line++] = sa.len ;
        if (!strbuf_cats(&sa, p->mnt_dir) || !strbuf_terminate(&sa)) {
            log_warnusys("add mount point to list") ;
            endmntent(fp) ;
            return -1 ;
        }
    }
    if (errno) log_warnusys("read /proc/mounts") ;
    endmntent(fp) ;

    while (line--) {
        if (umount(sa.s + mountpoints[line]) == -1) {
            e++ ;
            log_warnusys("umount ", sa.s + mountpoints[line]) ;
        }
    }

    return e ;
}
