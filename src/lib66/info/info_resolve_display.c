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
#include <string.h>
#include <stdint.h>
#include <wchar.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/types.h>
#include <oblibs/strbuf.h>
#include <oblibs/sbl.h>
#include <oblibs/stream.h>

#include <66/info.h>

static wchar_t const field_suffix[] = L" :" ;

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

    } else {

        char ui[U64_FMT] ;
        ui[u64_fmt(ui, *(uint64_t const *)p)] = 0 ;

        if (!ostream_puts(ostream_1, ui))
            log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
    }

    if (!ostream_putflush(ostream_1, "\n", 1))
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
}

void info_resolve_display(void const *base, char const *rblob, info_field_t const *fields, size_t nfields, char const *select, uint8_t noname)
{
    log_flow() ;

    /* build the list of field indices to display, in display order */
    size_t idx[nfields] ;
    size_t n = 0 ;

    if (select) {

        _alloc_sbl_(sel, 256) ;

        if (!opt_list(&sel, select, ','))
            log_dieu(LOG_EXIT_SYS, "parse field list: ", select) ;

        size_t pos = 0 ;
        FOREACH_SBL(&sel, pos) {

            char const *key = sel.s + pos ;
            size_t i = 0 ;

            for (; i < nfields ; i++)
                if (!strcmp(fields[i].key, key))
                    break ;

            if (i == nfields)
                log_die(LOG_EXIT_USER, "unknown field: ", key) ;

            idx[n++] = i ;
        }

    } else {

        for (; n < nfields ; n++)
            idx[n] = n ;
    }

    if (noname) {

        for (size_t i = 0 ; i < n ; i++)
            display_value(base, rblob, &fields[idx[i]]) ;

    } else {

        char buf[nfields][INFO_FIELD_MAXLEN] ;
        char aligned[nfields][INFO_FIELD_MAXLEN] ;

        for (size_t i = 0 ; i < n ; i++) {

            char const *key = fields[idx[i]].key ;
            memcpy(buf[i], key, strlen(key) + 1) ;
        }

        info_field_align(buf, aligned, field_suffix, n) ;

        for (size_t i = 0 ; i < n ; i++) {

            info_display_field_name(aligned[i]) ;
            display_value(base, rblob, &fields[idx[i]]) ;
        }
    }
}
