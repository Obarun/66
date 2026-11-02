/*
 * info_status_line.c
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

#include <stdint.h>
#include <time.h>

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/clock.h>
#include <oblibs/string.h>
#include <oblibs/stream.h>

#include <66/info.h>
#include <66/status.h>

void info_status_line(service_status_t const *st, uint8_t state, uint8_t raw, char const *suffix)
{
    log_flow() ;

    int warn_color = 0 ;
    char const *word = status_state_to_string(state) ;
    switch (state) {
        case STATUS_STATE_UP :
        case STATUS_STATE_STARTING :
        case STATUS_STATE_DONE :
        case STATUS_STATE_WAITING :    warn_color = 2 ; break ;
        default :                      warn_color = 1 ; break ;
    }

    struct timespec now ;
    clock_now(&now) ;
    uint64_t elapsed = now.tv_sec > st->stamp.tv_sec ? (uint64_t)(now.tv_sec - st->stamp.tv_sec) : 0 ;
    char secs[INFO_DURATION_LEN + 1] ;

    if (raw)
        secs[u64_fmt(secs, elapsed)] = 0 ;
    else
        info_fmt_duration(secs, elapsed) ;

    // what last happened to the process; nothing to report on a clean success
    char code[U64_FMT] ;
    code[u64_fmt(code, st->code)] = 0 ;
    char detail[U64_FMT + 16] = "" ;
    switch (st->result) {
        case STATUS_RESULT_SUCCESS :
            break ;
        case STATUS_RESULT_EXITED :
        case STATUS_RESULT_SIGNALED :
            auto_strings(detail, " (", status_result_to_string(st->result), " ", code, ")") ;
            break ;
        default :
            auto_strings(detail, " (", status_result_to_string(st->result), ")") ;
            break ;
    }

    char whoby[16] = "" ;
    auto_strings(whoby, " by ", status_who_to_string(st->who)) ;

    char const *color = warn_color > 1 ? log_color->valid : log_color->error ;

    if (st->pid > 0) {

        char pid[PID_FMT] ;
        pid[pid_format(pid, st->pid)] = 0 ;

        if (!ostream_fmt(ostream_1, "%s%s%s (pid %s)%s since %s%s%s\n",
                         color, word, log_color->off, pid, detail, secs, whoby, suffix))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

    } else {

        if (!ostream_fmt(ostream_1, "%s%s%s%s since %s%s%s\n",
                         color, word, log_color->off, detail, secs, whoby, suffix))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
    }
}
