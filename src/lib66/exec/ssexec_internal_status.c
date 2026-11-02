/*
 * ssexec_internal_status.c
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
#include <oblibs/opt.h>
#include <oblibs/string.h>
#include <oblibs/stream.h>

#include <66/ssexec.h>
#include <66/svc.h>
#include <66/info.h>
#include <66/status.h>
#include <66/constants.h>

static char const *const internal_names[] = { SS_FDHOLDER, SS_ONESHOTD, SS_EVENTD } ;

static void internal_status_display(char const *scandir, char const *name)
{
    log_flow() ;

    char dir[strlen(scandir) + 1 + strlen(name) + 1] ;
    auto_strings(dir, scandir, "/", name) ;

    char file[sizeof(dir) + SS_SUPERVISEDIR_LEN + 1 + SS_STATUS_LEN] ;
    auto_strings(file, dir, SS_SUPERVISEDIR, "/", SS_STATUS) ;

    if (!ostream_fmt(ostream_1, "%s%-*s%s : ", log_color->info, (int)SS_FDHOLDER_LEN, name, log_color->off))
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;

    /* the status file outlives its supervisor: read it only while one listens */
    int r = svc_supervise_ok(dir) ;
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "check the supervisor of: ", dir) ;

    service_status_t st = STATUS_ZERO ;
    if (r) {
        r = status_read(&st, file) ;
        if (r < 0)
            log_warnusys("read status of: ", dir) ;
    }

    if (r <= 0) {

        if (!ostream_fmt(ostream_1, "%s%s%s\n", log_color->warning, r < 0 ? "unreadable" : "None", log_color->off))
            log_dieusys(LOG_EXIT_SYS, "write to stdout") ;

        return ;
    }

    info_status_line(&st, st.state, 0, "") ;
}

int ssexec_internal_status(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;
    size_t pos = 0, n = OPT_COUNT(internal_names) ;

    if (argc > 1)
        log_die(LOG_EXIT_USER, "expected at most one internal service: " SS_FDHOLDER ", " SS_ONESHOTD " or " SS_EVENTD) ;

    if (argc) {

        for (; pos < n ; pos++) {
            if (!strcmp(argv[0], internal_names[pos]))
                break ;
        }

        if (pos == n)
            log_die(LOG_EXIT_USER, "unknown internal service: ", argv[0]) ;

        internal_status_display(info->scandir.s, argv[0]) ;

    } else {

        for (; pos < n ; pos++)
            internal_status_display(info->scandir.s, internal_names[pos]) ;
    }

    if (!ostream_flush(ostream_1))
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;

    return 0 ;
}
