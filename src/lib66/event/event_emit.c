/*
 * event_emit.c
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

#include <time.h>
#include <stdint.h>

#include <oblibs/log.h>

#include <66/event.h>

int event_emit_transition(char const *path, uint8_t state, uint8_t result, uint8_t who, uint32_t code, uint32_t pid, struct timespec const *stamp, uint8_t flags)
{
    log_flow() ;

    char buf[EVENT_FRAME_MAX] ;
    size_t len = event_frame_pack_transition(buf, state, result, who, code, pid, stamp, flags) ;
    return event_fifo_notify(path, buf, len) ;
}

int event_emit_signal(char const *path, uint8_t signo, uint8_t who, struct timespec const *stamp)
{
    log_flow() ;

    char buf[EVENT_FRAME_MAX] ;
    size_t len = event_frame_pack_signal(buf, signo, who, stamp) ;
    return event_fifo_notify(path, buf, len) ;
}

int event_emit_lifecycle(char const *path, uint8_t phase, struct timespec const *stamp)
{
    log_flow() ;

    char buf[EVENT_FRAME_MAX] ;
    size_t len = event_frame_pack_lifecycle(buf, phase, stamp) ;
    return event_fifo_notify(path, buf, len) ;
}
