/*
 * 66-umountall.c
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
 *
 * This file is a strict copy of s6-linux-init-umountall.c file
 * coming from skarnet software at https://skarnet.org/software/s6-linux-init.
 * All credits goes to Laurent Bercot <ska-remove-this-if-you-are-not-a-bot@skarnet.org>
 * */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <mntent.h>
#include <sys/mount.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>
#include <skalibs/skamisc.h>

#include <66/config.h>

#define MAXLINES 99

#define EXCLUDEN 3

static char const *exclude_type[EXCLUDEN] = { "devtmpfs", "proc", "sysfs" } ;

int main (int argc, char const *const *argv)
{
    size_t mountpoints[MAXLINES], tmplen = strlen(SS_LIVE), len = 0 ;
    char tmpdir[tmplen + 1] ;
    ob_dirname(tmpdir,SS_LIVE) ;
    len = strlen(tmpdir) ;
    if (tmpdir[len-1] == '/')
        tmpdir[len-1] = 0 ;
    unsigned int got[EXCLUDEN] = { 0, 0, 0 } ;
    stralloc sa = STRALLOC_ZERO ;
    unsigned int line = 0 ;
    FILE *fp = setmntent("/proc/mounts", "r") ;
    int e = 0 ;

    PROG = "66-umountall" ;

    if (!fp) log_dieusys(LOG_EXIT_SYS, "open /proc/mounts") ;

    for (;;)
    {
        struct mntent *p ;
        unsigned int i = 0 ;
        errno = 0 ;
        p = getmntent(fp) ;
        if (!p) break ;
        if (!strcmp(p->mnt_dir,tmpdir)) continue ;
        for (; i < EXCLUDEN ; i++)
        {
            if (!strcmp(p->mnt_type, exclude_type[i]))
            {
                got[i]++ ;
                break ;
            }
        }
        if (i < EXCLUDEN && got[i] == 1) continue ;
        if (line >= MAXLINES)
            log_die(100, "too many mount points") ;
        mountpoints[line++] = sa.len ;
        if (!stralloc_cats(&sa, p->mnt_dir) || !stralloc_0(&sa))
            log_dieusys(LOG_EXIT_SYS, "add mount point to list") ;
    }
    if (errno) log_dieusys(LOG_EXIT_SYS, "read /proc/mounts") ;
    endmntent(fp) ;

    while (line--)
    {
        if (umount(sa.s + mountpoints[line]) == -1)
        {
            e++ ;
            log_warnusys("umount ", sa.s + mountpoints[line]) ;
        }
    }
    stralloc_free(&sa) ;

    return e ;
}
