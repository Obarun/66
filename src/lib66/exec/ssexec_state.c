/*
 * ssexec_state.c
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

#include <wchar.h>

#include <oblibs/log.h>

#include <skalibs/types.h>
#include <skalibs/lolstdio.h>
#include <skalibs/buffer.h>
#include <skalibs/sgetopt.h>

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
        if (!bprintf(buffer_1,"%s%s",log_color->warning,"None"))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
    }
    else
    {
        if (!buffer_puts(buffer_1,str))
            log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
    }
    if (buffer_putsflush(buffer_1,"\n") == -1)
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
}

static void info_display_int(char const *field, unsigned int id)
{
    char *str = "0" ;
    if (id == STATE_FLAGS_TRUE)
        str = "1" ;

    info_display_string(field, str) ;
}

int ssexec_state(int argc, char const *const *argv, ssexec_t *info)
{
    int r = -1 ;
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
        "isdownfile",
        "isearlier" ,
        "isenabled" ,
        "isparsed" ,
        "issupervised",
        "isup" } ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = subgetopt_r(argc, argv, OPTS_ENABLE, &l) ;
            if (opt == -1) break ;

            switch (opt) {

                case 'h' :

                    info_help(info->help, info->usage) ;
                    return 0 ;

                default :
                    log_usage(info->usage, "\n", info->help) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (!argc)
        log_usage(info->usage, "\n", info->help) ;

    svname = *argv ;

    r = service_is_g(svname, STATE_FLAGS_ISPARSED) ;
    if (r == -1)
        log_dieusys(LOG_EXIT_SYS, "get information of service: ", svname, " -- please a bug report") ;
    else if (!r || r == STATE_FLAGS_FALSE)
        log_die(LOG_EXIT_USER, "service: ", svname, " is not parsed -- try to parse it using '66 parse ", svname, "'") ;

    r = resolve_read_g(wres, info->base.s, svname) ;
    if (r <= 0)
        log_dieu(LOG_EXIT_SYS, "read resolve file: ", svname) ;

    info_field_align(buf,fields,field_suffix,MAXOPTS) ;

    if (!state_read(&sta, &res))
        log_dieusys(111,"read state file of: ", svname) ;

    info_display_int(fields[0],sta.toinit) ;
    info_display_int(fields[1],sta.toreload) ;
    info_display_int(fields[2],sta.torestart) ;
    info_display_int(fields[3],sta.tounsupervise) ;
    info_display_int(fields[4],sta.toparse) ;
    info_display_int(fields[5],sta.isdownfile) ;
    info_display_int(fields[6],sta.isearlier) ;
    info_display_int(fields[7],sta.isenabled) ;
    info_display_int(fields[8],sta.isparsed) ;
    info_display_int(fields[9],sta.issupervised) ;
    info_display_int(fields[10],sta.isup) ;

    resolve_free(wres) ;

    return 0 ;
}
