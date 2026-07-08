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

#define EVENT_FROMFIELD_TABLE(macro) \
    macro(EVENT_FROMFIELD_DEPENDS,    "Depends",     (1u << 0)) \
    macro(EVENT_FROMFIELD_REQUIREDBY, "RequiredBy",  (1u << 1)) \
    macro(EVENT_FROMFIELD_OPTSDEPS,   "OptsDepends", (1u << 2))

typedef enum event_fromfield_e event_fromfield_t ;
enum event_fromfield_e
{
#define EVENT_FROMFIELD_ENUM(id, str, bit) id = bit,
    EVENT_FROMFIELD_TABLE(EVENT_FROMFIELD_ENUM)
#undef EVENT_FROMFIELD_ENUM
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

static inline int event_fromfield_from_string(char const *s)
{
#define EVENT_FROMFIELD_IF(id, str, bit) if (!strcmp(s, str)) return id ;
    EVENT_FROMFIELD_TABLE(EVENT_FROMFIELD_IF)
#undef EVENT_FROMFIELD_IF
    return -1 ;
}

#endif
