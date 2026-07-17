/*
 * info_fields_display.c
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
#include <string.h>
#include <wchar.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/sbl.h>

#include <66/info.h>

static wchar_t const field_suffix[] = L" :" ;

void info_fields_display(char const *const *keys, char const *const *labels, size_t nfields, char const *select, uint8_t noname, info_value_writer *write, void *ctx)
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
                if (!strcmp(keys[i], key))
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
            write(ctx, idx[i], 0) ;

    } else {

        char buf[nfields][INFO_FIELD_MAXLEN] ;
        char aligned[nfields][INFO_FIELD_MAXLEN] ;

        for (size_t i = 0 ; i < n ; i++)
            memcpy(buf[i], labels[idx[i]], strlen(labels[idx[i]]) + 1) ;

        info_field_align(buf, aligned, field_suffix, n) ;

        for (size_t i = 0 ; i < n ; i++) {

            info_display_field_name(aligned[i]) ;
            write(ctx, idx[i], aligned[i]) ;
        }
    }
}
