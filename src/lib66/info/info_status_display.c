/*
 * info_status_display.c
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

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/types.h>
#include <oblibs/stream.h>

#include <66/info.h>
#include <66/status.h>

enum status_kind_e {
    STATUS_KIND_STATE,
    STATUS_KIND_RESULT,
    STATUS_KIND_WHO,
    STATUS_KIND_U32,
    STATUS_KIND_EPOCH,
    STATUS_KIND_U8,
} ;

typedef struct status_field_s status_field_t ;
struct status_field_s {
    char const *key ; // what -f matches
    uint8_t kind ; // status_kind_e
    size_t offset ; // offsetof the member in service_status_t
} ;

static status_field_t const fields_status[] = {
    { "state",        STATUS_KIND_STATE,  offsetof(service_status_t, state) },
    { "result",       STATUS_KIND_RESULT, offsetof(service_status_t, result) },
    { "who",          STATUS_KIND_WHO,    offsetof(service_status_t, who) },
    { "pid",          STATUS_KIND_U32,    offsetof(service_status_t, pid) },
    { "code",         STATUS_KIND_U32,    offsetof(service_status_t, code) },
    { "stamp",        STATUS_KIND_EPOCH,  offsetof(service_status_t, stamp) },
    { "readystamp",   STATUS_KIND_EPOCH,  offsetof(service_status_t, readystamp) },
    { "window_start", STATUS_KIND_EPOCH,  offsetof(service_status_t, window_start) },
    { "ndeaths",      STATUS_KIND_U8,     offsetof(service_status_t, ndeaths) },
} ;

#define NFIELDS_STATUS OPT_COUNT(fields_status)

static void display_value(service_status_t const *st, status_field_t const *f)
{
    void const *p = (char const *)st + f->offset ;
    char ui[U64_FMT] ;

    switch (f->kind) {

        case STATUS_KIND_STATE :

            if (!ostream_puts(ostream_1, status_state_to_string(*(uint8_t const *)p)))
                log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
            break ;

        case STATUS_KIND_RESULT :

            if (!ostream_puts(ostream_1, status_result_to_string(*(uint8_t const *)p)))
                log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
            break ;

        case STATUS_KIND_WHO :

            if (!ostream_puts(ostream_1, status_who_to_string(*(uint8_t const *)p)))
                log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
            break ;

        case STATUS_KIND_U32 :

            ui[u32_fmt(ui, *(uint32_t const *)p)] = 0 ;
            if (!ostream_puts(ostream_1, ui))
                log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
            break ;

        case STATUS_KIND_U8 :

            ui[u32_fmt(ui, *(uint8_t const *)p)] = 0 ;
            if (!ostream_puts(ostream_1, ui))
                log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
            break ;

        case STATUS_KIND_EPOCH :

            ui[u64_fmt(ui, (uint64_t)((struct timespec const *)p)->tv_sec)] = 0 ;
            if (!ostream_puts(ostream_1, ui))
                log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
            break ;
    }

    if (!ostream_putflush(ostream_1, "\n", 1))
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
}

static void write_value(void *ctx, size_t index)
{
    display_value((service_status_t const *)ctx, &fields_status[index]) ;
}

void info_status_display(service_status_t const *st, char const *select, uint8_t noname)
{
    log_flow() ;

    char const *keys[NFIELDS_STATUS] ;
    for (size_t i = 0 ; i < NFIELDS_STATUS ; i++)
        keys[i] = fields_status[i].key ;

    info_fields_display(keys, NFIELDS_STATUS, select, noname, &write_value, (void *)st) ;
}
