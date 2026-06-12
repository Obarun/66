/*
 * ssexec_state.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <wchar.h>
#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/stream.h>
#include <oblibs/files.h>

#include <66/info.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/state.h>
#include <66/config.h>
#include <66/ssexec.h>

#define MAXOPTS 12

static wchar_t const field_suffix[] = L" :" ;
static char fields[INFO_NKEY][INFO_FIELD_MAXLEN] = {{ 0 }} ;

static void info_display_string(char const *field,char const *str)
{
    info_display_field_name(field) ;

    if (!*str)
    {
        if (!ostream_fmt(ostream_1,"%s%s",log_color->warning,"None"))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
    }
    else
    {
        if (!ostream_puts(ostream_1,str))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
    }
    if (!ostream_putflush(ostream_1, "\n", 1))
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
}

static void info_display_int(char const *field, unsigned int id)
{
    char *str = "0" ;
    if (id == STATE_FLAGS_TRUE)
        str = "1" ;

    info_display_string(field, str) ;
}

static opt_t const opts_state[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help", .arg = OPT_NONE, .help = "print this help" },
} ;

opt_cmd_t const cmd_state = {
    .name = "66 state",
    .help = "display state files contents of services",
    .operands = "service",
    .opts = opts_state,
    .nopts = OPT_COUNT(opts_state),
    .fn = &ssexec_state,
} ;

int ssexec_state(int argc, char const *const *argv, void *data)
{
    ssexec_t *info = data ;
    int r = -1 ;
    uint8_t m = 0 ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    ss_state_t sta = STATE_ZERO ;
    char const *svname = 0 ;

    char buf[MAXOPTS][INFO_FIELD_MAXLEN] = {
        "toinit",
        "toreload",
        "torestart",
        "tounsupervise",
        "toparse",
        "isparsed" ,
        "issupervised",
        "isup" } ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing service argument") ;

    svname = *argv ;

    if (svname[0] == '/') {

        char pack[STATE_STATE_SIZE] ;

        r = file_read(svname, pack, STATE_STATE_SIZE) ;
            if (r < STATE_STATE_SIZE || r < 0)
                log_dieusys(LOG_EXIT_SYS, "read status file") ;

        state_unpack(pack, &sta) ;

    } else {

        r = service_is_g(svname, STATE_FLAGS_ISPARSED) ;
        if (r == -1)
            log_dieusys(LOG_EXIT_SYS, "get information of service: ", svname, " -- please a bug report") ;
        else if (!r || r == STATE_FLAGS_FALSE)
            log_die(LOG_EXIT_USER, "service: ", svname, " is not parsed -- try to parse it using '66 parse ", svname, "'") ;

        r = resolve_read_g(wres, info->base.s, svname) ;
        if (r <= 0)
            log_dieu(LOG_EXIT_SYS, "read resolve file: ", svname) ;

        if (!state_read(&sta, &res))
            log_dieusys(111,"read state file of: ", svname) ;
    }

    info_field_align(buf,fields,field_suffix,MAXOPTS) ;

    info_display_int(fields[m++],sta.toinit) ;
    info_display_int(fields[m++],sta.toreload) ;
    info_display_int(fields[m++],sta.torestart) ;
    info_display_int(fields[m++],sta.tounsupervise) ;
    info_display_int(fields[m++],sta.toparse) ;
    info_display_int(fields[m++],sta.isparsed) ;
    info_display_int(fields[m++],sta.issupervised) ;
    info_display_int(fields[m],sta.isup) ;

    resolve_free(wres) ;

    return 0 ;
}
