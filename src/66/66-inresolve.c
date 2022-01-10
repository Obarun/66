/*
 * 66-inresolve.c
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

#define MAXOPTS 32

static wchar_t const field_suffix[] = L" :" ;
static char fields[INFO_NKEY][INFO_FIELD_MAXLEN] = {{ 0 }} ;

#define USAGE "66-inresolve [ -h ] [ -z ] [ -v verbosity ] [ -t tree ] [ -l ] tree|service name"

static inline void info_help (void)
{
    DEFAULT_MSG = 0 ;

    static char const *help =
"\n"
"options :\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
"   -t: only search service at the specified tree\n"
"   -l: prints information of the associated logger if exist\n"
;

    log_info(USAGE,"\n",help) ;
}

static inline unsigned int lookup (char const *const *table, char const *data)
{
    log_flow() ;

    unsigned int i = 0 ;
    for (; table[i] ; i++) if (!strcmp(data, table[i])) break ;
    return i ;
}

static inline unsigned int parse_what (char const *str)
{
    log_flow() ;

    static char const *const table[] =
    {
        "service",
        "tree",
        0
    } ;
  unsigned int i = lookup(table, str) ;
  if (!table[i]) i = 2 ;
  return i ;
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
    int found = 0, what = 0 ;
    uint8_t logger = 0 ;

    stralloc satree = STRALLOC_ZERO ;
    stralloc src = STRALLOC_ZERO ;
    stralloc tmp = STRALLOC_ZERO ;
    char const *tname = 0 ;
    char const *svname = 0 ;

    log_color = &log_color_disable ;

    PROG = "66-inresolve" ;
    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = getopt_args(argc,argv, ">hv:zlt:", &l) ;
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

                    tname = l.arg ;
                    break ;

                case 'l' :

                    logger = 1 ;
                    break ;

                default :

                    log_usage(USAGE) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (!argc) log_usage(USAGE) ;

    what = parse_what(*argv) ;
    if (what == 2)
        log_usage(USAGE) ;

    argv++;
    argc--;
    svname = *argv ;

    if (!what) {

        char service_buf[MAXOPTS][INFO_FIELD_MAXLEN] = {
            "Name",
            "Description" ,
            "Version",
            "Logger",
            "Logreal",
            "Logassoc",
            "Dstlog",
            "Depends",
            "Requiredby",
            "Optsdeps",
            "Extdeps",
            "Contents" ,
            "Src" ,
            "Srconf",
            "Live",
            "Runat",
            "Tree",
            "Treename",
            "State",
            "Exec_run" ,
            "Real_exec_run" ,
            "Exec_finish" ,
            "Real_exec_finish" ,
            "Type" ,
            "Ndepends" ,
            "Nrequiredby" ,
            "Noptsdeps" ,
            "Nextdeps" ,
            "Ncontents" ,
            "Down" ,
            "Disen",
            "Real_logger_name" } ;

        resolve_service_t res = RESOLVE_SERVICE_ZERO ;
        resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;
        resolve_service_t lres = RESOLVE_SERVICE_ZERO ;
        resolve_wrapper_t_ref lwres = resolve_set_struct(DATA_SERVICE, &lres) ;

        found = service_intree(&src,svname,tname) ;
        if (found == -1) log_dieu(LOG_EXIT_SYS,"resolve tree source of sv: ",svname) ;
        else if (!found) {
            log_info("no tree exist yet") ;
            goto freed ;
        }
        else if (found > 2) {
            log_die(LOG_EXIT_SYS,svname," is set on different tree -- please use -t options") ;
        }
        else if (found == 1) log_die(LOG_EXIT_SYS,"unknown service: ",svname) ;

        if (!resolve_read(wres,src.s,svname)) log_dieusys(LOG_EXIT_SYS,"read resolve file") ;

        info_field_align(service_buf,fields,field_suffix,MAXOPTS) ;

        info_display_string(fields[0],res.sa.s + res.name) ;
        info_display_string(fields[1],res.sa.s + res.description) ;
        info_display_string(fields[2],res.sa.s + res.version) ;
        info_display_string(fields[3],res.sa.s + res.logger) ;
        info_display_string(fields[4],res.sa.s + res.logreal) ;
        info_display_string(fields[5],res.sa.s + res.logassoc) ;
        info_display_string(fields[6],res.sa.s + res.dstlog) ;
        info_display_string(fields[7],res.sa.s + res.depends) ;
        info_display_string(fields[8],res.sa.s + res.requiredby) ;
        info_display_string(fields[9],res.sa.s + res.optsdeps) ;
        info_display_string(fields[10],res.sa.s + res.extdeps) ;
        info_display_string(fields[11],res.sa.s + res.contents) ;
        info_display_string(fields[12],res.sa.s + res.src) ;
        info_display_string(fields[13],res.sa.s + res.srconf) ;
        info_display_string(fields[14],res.sa.s + res.live) ;
        info_display_string(fields[15],res.sa.s + res.runat) ;
        info_display_string(fields[16],res.sa.s + res.tree) ;
        info_display_string(fields[17],res.sa.s + res.treename) ;
        info_display_string(fields[18],res.sa.s + res.state) ;
        info_display_string(fields[19],res.sa.s + res.exec_run) ;
        info_display_string(fields[20],res.sa.s + res.real_exec_run) ;
        info_display_string(fields[21],res.sa.s + res.exec_finish) ;
        info_display_string(fields[22],res.sa.s + res.real_exec_finish) ;
        info_display_int(fields[23],res.type) ;
        info_display_int(fields[24],res.ndepends) ;
        info_display_int(fields[25],res.nrequiredby) ;
        info_display_int(fields[26],res.noptsdeps) ;
        info_display_int(fields[27],res.nextdeps) ;
        info_display_int(fields[28],res.ncontents) ;
        info_display_int(fields[29],res.down) ;
        info_display_int(fields[30],res.disen) ;

        if (res.logger && logger)
        {
            if (!resolve_read(lwres,src.s,res.sa.s + res.logger)) log_dieusys(111,"read resolve file of: ",res.sa.s + res.logger) ;

            if (buffer_putsflush(buffer_1,"\n") == -1)
                log_dieusys(LOG_EXIT_SYS,"write to stdout") ;

            info_display_string(fields[31],res.sa.s + res.logreal) ;
            info_display_string(fields[0],lres.sa.s + lres.name) ;
            info_display_string(fields[1],lres.sa.s + lres.description) ;
            info_display_string(fields[2],lres.sa.s + lres.version) ;
            info_display_string(fields[3],lres.sa.s + lres.logger) ;
            info_display_string(fields[4],lres.sa.s + lres.logreal) ;
            info_display_string(fields[5],lres.sa.s + lres.logassoc) ;
            info_display_string(fields[6],lres.sa.s + lres.dstlog) ;
            info_display_string(fields[7],lres.sa.s + lres.depends) ;
            info_display_string(fields[8],lres.sa.s + lres.requiredby) ;
            info_display_string(fields[12],lres.sa.s + lres.src) ;
            info_display_string(fields[13],lres.sa.s + lres.srconf) ;
            info_display_string(fields[14],lres.sa.s + lres.live) ;
            info_display_string(fields[15],lres.sa.s + lres.runat) ;
            info_display_string(fields[16],lres.sa.s + lres.tree) ;
            info_display_string(fields[17],lres.sa.s + lres.treename) ;
            info_display_string(fields[15],lres.sa.s + lres.state) ;
            info_display_string(fields[19],lres.sa.s + lres.exec_log_run) ;
            info_display_string(fields[20],lres.sa.s + lres.real_exec_log_run) ;
            info_display_int(fields[23],lres.type) ;
            info_display_int(fields[24],lres.ndepends) ;
            info_display_int(fields[25],lres.nrequiredby) ;
            info_display_int(fields[29],lres.down) ;
            info_display_int(fields[30],lres.disen) ;
        }

        resolve_free(wres) ;
        resolve_free(lwres) ;

    } else {

        char tree_buf[MAXOPTS][INFO_FIELD_MAXLEN] = {
            "Name",
            "Depends" ,
            "Requiredby",
            "Allow",
            "Groups",
            "Contents",
            "Enabled",
            "Current",
            "Ndepends",
            "Nrequiredby",
            "Nallow",
            "Ngroups",
            "Ncontents",
            "Init" ,
            "Disen",
            "Nenabled" } ;

        resolve_tree_t tres = RESOLVE_TREE_ZERO ;
        resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;

        if (!set_ownersysdir(&src, getuid()))
            log_dieusys(LOG_EXIT_SYS, "set owner directory") ;

        found = tree_isvalid(src.s,svname) ;
        if (found < 0)
            log_diesys(LOG_EXIT_SYS, "invalid tree directory") ;

        if (!found)
            log_dieusys(LOG_EXIT_SYS,"find tree: ", svname) ;

        if (!auto_stra(&src, SS_SYSTEM))
            log_die_nomem("stralloc") ;

        if (!resolve_read(wres,src.s,svname))
            log_dieusys(LOG_EXIT_SYS,"read resolve file") ;

        info_field_align(tree_buf,fields,field_suffix,MAXOPTS) ;

        info_display_string(fields[0],tres.sa.s + tres.name) ;
        info_display_string(fields[1],tres.sa.s + tres.depends) ;
        info_display_string(fields[2],tres.sa.s + tres.requiredby) ;
        info_display_string(fields[3],tres.sa.s + tres.allow) ;
        info_display_string(fields[4],tres.sa.s + tres.groups) ;
        info_display_string(fields[5],tres.sa.s + tres.contents) ;
        info_display_string(fields[6],tres.sa.s + tres.enabled) ;
        info_display_string(fields[7],tres.sa.s + tres.current) ;
        info_display_int(fields[8],tres.ndepends) ;
        info_display_int(fields[9],tres.nrequiredby) ;
        info_display_int(fields[10],tres.nallow) ;
        info_display_int(fields[11],tres.ngroups) ;
        info_display_int(fields[12],tres.ncontents) ;
        info_display_int(fields[13],tres.init) ;
        info_display_int(fields[14],tres.disen) ;
        info_display_int(fields[15],tres.nenabled) ;

        resolve_free(wres) ;

    }
    freed:

    stralloc_free(&satree) ;
    stralloc_free(&src) ;
    stralloc_free(&tmp) ;
    return 0 ;
}
