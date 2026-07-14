/*
 * event_rule.h
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

#ifndef SS_EVENT_RULE_H
#define SS_EVENT_RULE_H

#include <stdint.h>
#include <string.h> // strcmp

#define EVENT_SOURCE_TABLE(macro) \
    macro(EVENT_SOURCE_SERVICE,  "service") \
    macro(EVENT_SOURCE_SIGNAL,   "signal") \
    macro(EVENT_SOURCE_INOTIFY,  "inotify") \
    macro(EVENT_SOURCE_SCHEDULE, "schedule") \
    macro(EVENT_SOURCE_USER,     "user") \
    macro(EVENT_SOURCE_TIMER,    "timer")

typedef enum event_source_e event_source_t ;
enum event_source_e
{
#define EVENT_SOURCE_ENUM(id, str) id,
    EVENT_SOURCE_TABLE(EVENT_SOURCE_ENUM)
#undef EVENT_SOURCE_ENUM
    EVENT_SOURCE_ENDOFKEY
} ;

#define EVENT_DO_TABLE(macro) \
    macro(EVENT_DO_START,       "start") \
    macro(EVENT_DO_STOP,        "stop") \
    macro(EVENT_DO_RESTART,     "restart") \
    macro(EVENT_DO_RELOAD,      "reload") \
    macro(EVENT_DO_RECONFIGURE, "reconfigure") \
    macro(EVENT_DO_FREE,        "free")

typedef enum event_do_e event_do_t ;
enum event_do_e
{
    EVENT_DO_NONE = 0,
#define EVENT_DO_ENUM(id, str) id,
    EVENT_DO_TABLE(EVENT_DO_ENUM)
#undef EVENT_DO_ENUM
    EVENT_DO_ENDOFKEY
} ;

typedef enum event_combine_e event_combine_t ;
enum event_combine_e
{
    EVENT_COMBINE_ANY = 0,
    EVENT_COMBINE_ALL,
    EVENT_COMBINE_ENDOFKEY
} ;

static inline char const *event_src_to_string(event_source_t v)
{
    switch (v) {
#define EVENT_SOURCE_CASE(id, str) case id: return str ;
        EVENT_SOURCE_TABLE(EVENT_SOURCE_CASE)
#undef EVENT_SOURCE_CASE
        default: return 0 ;
    }
}

static inline int event_src_from_string(char const *s)
{
#define EVENT_SOURCE_IF(id, str) if (!strcmp(s, str)) return id ;
    EVENT_SOURCE_TABLE(EVENT_SOURCE_IF)
#undef EVENT_SOURCE_IF
    return -1 ;
}

static inline char const *event_do_to_string(event_do_t v)
{
    switch (v) {
#define EVENT_DO_CASE(id, str) case id: return str ;
        EVENT_DO_TABLE(EVENT_DO_CASE)
#undef EVENT_DO_CASE
        default: return 0 ;
    }
}

static inline int event_do_from_string(char const *s)
{
#define EVENT_DO_IF(id, str) if (!strcmp(s, str)) return id ;
    EVENT_DO_TABLE(EVENT_DO_IF)
#undef EVENT_DO_IF
    return -1 ;
}


/** @brief inotify(7) constants accepted on an inotify source. Stored verbatim;
 * the daemon maps each name to its inotify mask. The On predicates of a service
 * reactor are NOT here -- they are the status vocabulary (see <66/status.h>). */
/** The macro receives the bare inotify token, so a consumer can stringify it for
 * validation (no <sys/inotify.h> needed) or use it as the mask constant (the
 * daemon, which includes <sys/inotify.h>). */
#define EVENT_IN_TABLE(macro) \
    macro(IN_ACCESS) \
    macro(IN_MODIFY) \
    macro(IN_ATTRIB) \
    macro(IN_CLOSE_WRITE) \
    macro(IN_CLOSE_NOWRITE) \
    macro(IN_OPEN) \
    macro(IN_MOVED_FROM) \
    macro(IN_MOVED_TO) \
    macro(IN_CREATE) \
    macro(IN_DELETE) \
    macro(IN_DELETE_SELF) \
    macro(IN_MOVE_SELF) \
    macro(IN_MOVE) \
    macro(IN_CLOSE) \
    macro(IN_ALL_EVENTS)

/** @return 1 if @p s is a known inotify constant, 0 otherwise. */
static inline int event_in_is_valid(char const *s)
{
#define EVENT_IN_IF(tok) if (!strcmp(s, #tok)) return 1 ;
    EVENT_IN_TABLE(EVENT_IN_IF)
#undef EVENT_IN_IF
    return 0 ;
}

#endif
