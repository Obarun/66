/*
 * ssexec_tree_resolve.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <string.h>
#include <wchar.h>

#include <oblibs/log.h>
#include <oblibs/sastr.h>
#include <oblibs/string.h>
#include <oblibs/types.h>

#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/lolstdio.h>
#include <skalibs/buffer.h>

#include <66/resolve.h>
#include <66/ssexec.h>
#include <66/tree.h>
#include <66/info.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/config.h>
#include <66/state.h>

#define MAXOPTS 19

static wchar_t const field_suffix[] = L" :" ;
static char fields[INFO_NKEY][INFO_FIELD_MAXLEN] = {{ 0 }} ;

static void info_display_string(char const *field, char const *str, uint32_t element, uint8_t check)
{
    info_display_field_name(field) ;

    if (check && !element) {

        if (!bprintf(buffer_1,"%s%s", log_color->warning, "None"))
            log_dieusys(LOG_EXIT_SYS, "write to stdout") ;

    } else {

        if (!buffer_puts(buffer_1, str + element))
            log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
    }

    if (buffer_putsflush(buffer_1, "\n") == -1)
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;


}

static void info_display_int(char const *field, uint32_t element)
{
    info_display_field_name(field) ;

    char ui[UINT_FMT] ;
    ui[uint_fmt(ui, element)] = 0 ;

    if (!buffer_puts(buffer_1, ui))
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;

    if (buffer_putsflush(buffer_1, "\n") == -1)
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
}

int ssexec_tree_resolve(int argc, char const *const *argv, ssexec_t *info)
{
    int r = 0 ;
    uint8_t master = 0 ;

    char const *svname = 0 ;

    resolve_wrapper_t_ref wres = 0 ;
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_tree_master_t mres = RESOLVE_TREE_MASTER_ZERO ;

    argc-- ;
    argv++ ;

    if (argc < 1)
        log_usage(usage_tree_resolve, "\n", help_tree_resolve) ;

    svname = *argv ;

    char tree_buf[MAXOPTS][INFO_FIELD_MAXLEN] = {
        "name",
        "depends" ,
        "requiredby",
        "allow",
        "groups",
        "contents",
        "ndepends",
        "nrequiredby",
        "nallow",
        "ngroups",
        "ncontents",
        "init" ,
        "supervised",
        "disen", // 14
        // Master
        "enabled",
        "current",
        "contents",
        "nenabled",
        "ncontents" // 19
    } ;

    if (!strcmp(argv[0], SS_MASTER + 1)) {

        master = 1 ;
        wres = resolve_set_struct(DATA_TREE_MASTER, &mres) ;

    } else {

        wres = resolve_set_struct(DATA_TREE, &tres) ;
    }

    r = tree_isvalid(info->base.s, svname) ;
    if (r < 0)
        log_diesys(LOG_EXIT_SYS, "invalid tree directory") ;

    if (!r)
        log_dieusys(LOG_EXIT_SYS, "find tree: ", svname) ;

    if (!resolve_read_g(wres, info->base.s, svname))
        log_dieusys(LOG_EXIT_SYS, "read resolve file") ;

    info_field_align(tree_buf, fields, field_suffix,MAXOPTS) ;

    if (!master) {

        info_display_string(fields[0], tres.sa.s, tres.name, 0) ;
        info_display_string(fields[1], tres.sa.s, tres.depends, 1) ;
        info_display_string(fields[2], tres.sa.s, tres.requiredby, 1) ;
        info_display_string(fields[3], tres.sa.s, tres.allow, 1) ;
        info_display_string(fields[4], tres.sa.s, tres.groups, 1) ;
        info_display_string(fields[5], tres.sa.s, tres.contents, 1) ;
        info_display_int(fields[6], tres.ndepends) ;
        info_display_int(fields[7], tres.nrequiredby) ;
        info_display_int(fields[8], tres.nallow) ;
        info_display_int(fields[9], tres.ngroups) ;
        info_display_int(fields[10], tres.ncontents) ;
        info_display_int(fields[11], tres.init) ;
        info_display_int(fields[12], tres.supervised) ;
        info_display_int(fields[13], tres.disen) ;

    } else {

        info_display_string(fields[0], mres.sa.s, mres.name, 1) ;
        info_display_string(fields[3], mres.sa.s, mres.allow, 1) ;
        info_display_string(fields[14], mres.sa.s, mres.enabled, 1) ;
        info_display_string(fields[15], mres.sa.s, mres.current, 1) ;
        info_display_string(fields[16], mres.sa.s, mres.contents, 1) ;
        info_display_int(fields[17], mres.nenabled) ;
        info_display_int(fields[18], mres.ncontents) ;
    }

    resolve_free(wres) ;

    return 0 ;
}
