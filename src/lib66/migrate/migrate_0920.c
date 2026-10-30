/*
 * migrate_0920.c
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
#include <unistd.h>
#include <sys/stat.h>

#include <oblibs/log.h>
#include <oblibs/files.h>
#include <oblibs/strbuf.h>
#include <oblibs/string.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/migrate.h>
#include <66/ssexec.h>
#include <66/status.h>
#include <66/svc.h>
#include <66/utils.h>
#include <66/command.h>


#define MIGRATE_SCANDIR_HINT " -- you may need to force the reboot using 66 reboot -f"
#define MIGRATE_CONTROL_NAME_MAX (sizeof "/SIGWINCH" - 1)

static void migrate_strip_live(char const *file, char const *live)
{
    log_flow() ;

    _cleanup_strbuf_ strbuf b = STRBUF_ZERO ;
    size_t optlen = 4 + strlen(live) ;
    char opt[optlen + 1] ;
    char *at = 0 ;
    uint8_t did = 0 ;

    auto_strings(opt, " -l ", live) ;

    if (file_get_size(file) < 0)
        return ; // no such control script on this system

    if (!strbuf_read_file(&b, file) || !strbuf_uncounted(&b)) {
        log_warnusys("read control script: ", file, MIGRATE_SCANDIR_HINT) ;
        return ;
    }

    while ((at = strstr(b.s, opt))) {
        memmove(at, at + optlen, b.len - (at - b.s) - optlen + 1) ;
        b.len -= optlen ;
        did = 1 ;
    }

    if (!did)
        return ;

    log_trace("rewrite control script: ", file) ;

    if (!file_write(file, b.s, b.len) || chmod(file, 0755) < 0)
        log_warnusys("write control script: ", file, MIGRATE_SCANDIR_HINT) ;
}

void migrate_0920(void)
{
    log_flow() ;

    ssexec_t info = SSEXEC_ZERO ;

    char const *const control[] = {
        "/crash", "/finish", "/SIGINT", "/SIGQUIT", "/SIGTERM",
        "/SIGUSR1", "/SIGUSR2", "/SIGPWR", "/SIGWINCH", 0
    } ;

    info.owner = getuid() ;
    info.ownerlen = uid_format(info.ownerstr, info.owner) ;
    info.ownerstr[info.ownerlen] = 0 ;

    if (!set_ownersysdir(&info.base, info.owner))
        log_dieusys(LOG_EXIT_SYS, "set owner directory") ;

    command_set_info(&info) ;

    char sig[info.scandir.len + SS_SVSCAN_LEN + MIGRATE_CONTROL_NAME_MAX + 1] ;
    char shut[info.scandir.len + 1 + SS_BOOT_SHUTDOWND_LEN + 4 + 1] ;

    for (unsigned int i = 0 ; control[i] ; i++) {

        auto_strings(sig, info.scandir.s, SS_SVSCAN, control[i]) ;

        migrate_strip_live(sig, info.live.s) ;
    }

    auto_strings(shut, info.scandir.s, "/", SS_BOOT_SHUTDOWND, "/run") ;

    migrate_strip_live(shut, info.live.s) ;

    char shutd[info.scandir.len + 1 + SS_BOOT_SHUTDOWND_LEN + 1] ;

    auto_strings(shutd, info.scandir.s, "/", SS_BOOT_SHUTDOWND) ;

    if (!access(shutd, F_OK) && !svc_control_send(shutd, "k", 1, STATUS_WHO_SHUTDOWN))
        log_warnu("relaunch: ", shutd, MIGRATE_SCANDIR_HINT) ;

    ssexec_free(&info) ;
}
