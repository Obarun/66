/*
 * event_codec.c
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
 *
 * The wire codec: the only place that knows an event's single-byte encoding.
 * Keeping it isolated lets the rest of the engine (matcher, waiters) work on the
 * typed event_t, format-independent -- a future rich payload adds a codec beside
 * this one without touching them. The bytes are the transition alphabet emitted
 * by 66-supervise's fanout.
 */

#include <66/event.h>

char event_to_byte(event_t e)
{
    switch (e) {
        case EVENT_UP :             return 'u' ;
        case EVENT_READY :          return 'U' ;
        case EVENT_DOWN :           return 'd' ;
        case EVENT_DOWN_READY :     return 'D' ;
        case EVENT_NORESTART :      return 'O' ;
        case EVENT_SUPERVISE_UP :   return 's' ;
        case EVENT_SUPERVISE_DOWN : return 'x' ;
        default :                   return 0 ; // RESTART / RESTART_READY / NONE: not on the wire
    }
}

event_t event_from_byte(char c)
{
    switch (c) {
        case 'u' : return EVENT_UP ;
        case 'U' : return EVENT_READY ;
        case 'd' : return EVENT_DOWN ;
        case 'D' : return EVENT_DOWN_READY ;
        case 'O' : return EVENT_NORESTART ;
        case 's' : return EVENT_SUPERVISE_UP ;
        case 'x' : return EVENT_SUPERVISE_DOWN ;
        default :  return EVENT_NONE ;
    }
}
