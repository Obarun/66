/*
 * 66-instate.c
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
#include <oblibs/obgetopt.h>
#include <oblibs/types.h>

#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/lolstdio.h>
#include <skalibs/buffer.h>

#include <66/resolve.h>
#include <66/info.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/state.h>
#include <66/config.h>

#define MAXOPTS 11

static wchar_t const field_suffix[] = L" :" ;
static char fields[INFO_NKEY][INFO_FIELD_MAXLEN] = {{ 0 }} ;

#define USAGE "66-instate [ -h ] [ -v verbosity ] [ -z ] service"

static inline void info_help (void)
{
    DEFAULT_MSG = 0 ;

    static char const *help =
"\n"
"options :\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -t: only search service at the specified tree\n"
;

    log_info(USAGE,"\n",help) ;
}

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

static void info_display_int(char const *field,unsigned int id)
{
    char *str = 0 ;
    char ival[UINT_FMT] ;
    ival[uint_fmt(ival,id)] = 0 ;
    str = ival ;

    info_display_string(field,str) ;
}

int main(int argc, char const *const *argv)
{
    int found = 0 ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;
    char base[SS_MAX_PATH + 1] ;
    ss_state_t sta = STATE_ZERO ;
    char const *svname = 0 ;
    char const *ste = 0 ;
    char atree[SS_MAX_TREENAME + 1] ;

    log_color = &log_color_disable ;

    char buf[MAXOPTS][INFO_FIELD_MAXLEN] = {
        "toinit",
        "toreload",
        "torestart",
        "tounsupervise",
        "isdownfile",
        "isearlier" ,
        "isenabled" ,
        "isparsed" ,
        "issupervised",
        "isup" } ;

    PROG = "66-instate" ;
    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = getopt_args(argc,argv, ">hv:zt:", &l) ;
            if (opt == -1) break ;
            if (opt == -2) log_die(LOG_EXIT_USER,"options must be set first") ;
            switch (opt)
            {
                case 'h' :

                    info_help();
                    return 0 ;

                case 'v' :

                        if (!uint0_scan(l.arg, &VERBOSITY))
                            log_usage(USAGE) ;

                        break ;

                case 'z' :

                    log_color = !isatty(1) ? &log_color_disable : &log_color_enable ;
                    break ;

                case 't' :

                    log_1_warn("deprecated option -t -- ignore it") ;
                    break ;

                default :
                    log_usage(USAGE) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (!argc) log_usage(USAGE) ;
    svname = *argv ;

    if (!set_ownersysdir_stack(base, getuid()))
        log_dieu(LOG_EXIT_SYS, "set owner directory") ;

    found = service_is_g(atree, svname, STATE_FLAGS_ISPARSED) ;
    if (found == -1)
        log_dieusys(LOG_EXIT_SYS, "get information of service: ", svname, " -- please a bug report") ;
    else if (!found)
        log_die(LOG_EXIT_USER, "unknown service: ", svname) ;

    info_field_align(buf,fields,field_suffix,MAXOPTS) ;

    if (!state_check(base, svname)) log_diesys(111,"unitialized service: ",svname) ;
    if (!state_read(&sta, base, svname)) log_dieusys(111,"read state file of: ",ste,"/",svname) ;

    info_display_int(fields[0],sta.toinit) ;
    info_display_int(fields[1],sta.toreload) ;
    info_display_int(fields[2],sta.torestart) ;
    info_display_int(fields[3],sta.tounsupervise) ;
    info_display_int(fields[4],sta.isdownfile) ;
    info_display_int(fields[5],sta.isearlier) ;
    info_display_int(fields[6],sta.isenabled) ;
    info_display_int(fields[7],sta.isparsed) ;
    info_display_int(fields[8],sta.issupervised) ;
    info_display_int(fields[9],sta.isup) ;


    resolve_free(wres) ;

    return 0 ;
}
