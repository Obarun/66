/*
 * info_resolve_display.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stddef.h>
#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/stream.h>

#include <66/info.h>
#include <66/state.h>

typedef struct resolve_ctx_s resolve_ctx_t ;
struct resolve_ctx_s {
    void const *base ;
    char const *rblob ;
    info_field_t const *fields ;
} ;

static void display_value(void const *base, char const *rblob, info_field_t const *f)
{
    void const *p = (char const *)base + f->offset ;

    if (f->type == INFO_FIELD_STR) {

        uint32_t off = *(uint32_t const *)p ;

        if (!off) {

            if (!ostream_fmt(ostream_1, "%s%s", log_color->warning, "None"))
                log_dieu(LOG_EXIT_SYS, "write to stdout") ;

        } else {

            if (!ostream_puts(ostream_1, rblob + off))
                log_dieu(LOG_EXIT_SYS, "write to stdout") ;
        }

    } else if (f->type == INFO_FIELD_U32) {

        char ui[U32_FMT] ;
        ui[u32_fmt(ui, *(uint32_t const *)p)] = 0 ;

        if (!ostream_puts(ostream_1, ui))
            log_dieusys(LOG_EXIT_SYS, "write to stdout") ;

    } else if (f->type == INFO_FIELD_U64) {

        char ui[U64_FMT] ;
        ui[u64_fmt(ui, *(uint64_t const *)p)] = 0 ;

        if (!ostream_puts(ostream_1, ui))
            log_dieusys(LOG_EXIT_SYS, "write to stdout") ;

    } else if (f->type == INFO_FIELD_FLAG) {

        char const *str = (*(uint32_t const *)p == STATE_FLAGS_TRUE) ? "1" : "0" ;

        if (!ostream_puts(ostream_1, str))
            log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
    }

    if (!ostream_putflush(ostream_1, "\n", 1))
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
}

static void write_value(void *ctx, size_t index)
{
    resolve_ctx_t const *c = ctx ;
    display_value(c->base, c->rblob, &c->fields[index]) ;
}

void info_resolve_display(void const *base, char const *rblob, info_field_t const *fields, size_t nfields, char const *select, uint8_t noname)
{
    log_flow() ;

    char const *keys[nfields] ;
    for (size_t i = 0 ; i < nfields ; i++)
        keys[i] = fields[i].key ;

    resolve_ctx_t ctx = { base, rblob, fields } ;

    info_fields_display(keys, nfields, select, noname, &write_value, &ctx) ;
}
