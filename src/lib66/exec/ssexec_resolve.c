/*
 * ssexec_resolve.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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
#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/sastr.h>
#include <oblibs/string.h>
#include <oblibs/types.h>

#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/lolstdio.h>
#include <skalibs/buffer.h>
#include <skalibs/sgetopt.h>

#include <66/resolve.h>
#include <66/ssexec.h>
#include <66/service.h>
#include <66/info.h>
#include <66/constants.h>
#include <66/config.h>
#include <66/state.h>

#define MAXOPTS 77

static wchar_t const field_suffix[] = L" :" ;
static char fields[INFO_NKEY][INFO_FIELD_MAXLEN] = {{ 0 }} ;

static void info_display_string(char const *field, char const *str, uint32_t element, uint8_t check)
{
    info_display_field_name(field) ;

    if (check && !element) {

        if (!bprintf(buffer_1,"%s%s", log_color->warning, "None"))
            log_dieu(LOG_EXIT_SYS, "write to stdout") ;

    } else {

        if (!buffer_puts(buffer_1, str + element))
            log_dieu(LOG_EXIT_SYS, "write to stdout") ;
    }

    if (buffer_putsflush(buffer_1, "\n") == -1)
        log_dieu(LOG_EXIT_SYS, "write to stdout") ;


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

static void info_display_service_field(resolve_service_t *res)
{
    uint8_t m = 0 ;
    info_display_string(fields[m++], res->sa.s, res->name, 0) ;
    info_display_string(fields[m++], res->sa.s, res->description, 1) ;
    info_display_string(fields[m++], res->sa.s, res->version, 1) ;
    info_display_int(fields[m++], res->type) ;
    info_display_int(fields[m++], res->notify) ;
    info_display_int(fields[m++], res->maxdeath) ;
    info_display_int(fields[m++], res->earlier) ;
    info_display_string(fields[m++], res->sa.s, res->hiercopy, 1) ;
    info_display_string(fields[m++], res->sa.s, res->intree, 1) ;
    info_display_string(fields[m++], res->sa.s, res->ownerstr, 1) ;
    info_display_int(fields[m++], res->owner) ;
    info_display_string(fields[m++], res->sa.s, res->treename, 1) ;
    info_display_string(fields[m++], res->sa.s, res->user, 1) ;
    info_display_string(fields[m++], res->sa.s, res->inns, 1) ;
    info_display_int(fields[m++], res->enabled) ;
    info_display_int(fields[m++], res->islog) ;

    info_display_string(fields[m++], res->sa.s, res->path.home, 1) ;
    info_display_string(fields[m++], res->sa.s, res->path.frontend, 1) ;
    info_display_string(fields[m++], res->sa.s, res->path.servicedir, 1) ;

    info_display_string(fields[m++], res->sa.s, res->dependencies.depends, 1) ;
    info_display_string(fields[m++], res->sa.s, res->dependencies.requiredby, 1) ;
    info_display_string(fields[m++], res->sa.s, res->dependencies.optsdeps, 1) ;
    info_display_string(fields[m++], res->sa.s, res->dependencies.contents, 1) ;
    info_display_int(fields[m++], res->dependencies.ndepends) ;
    info_display_int(fields[m++], res->dependencies.nrequiredby) ;
    info_display_int(fields[m++], res->dependencies.noptsdeps) ;
    info_display_int(fields[m++], res->dependencies.ncontents) ;

    info_display_string(fields[m++], res->sa.s, res->execute.run.run, 1) ;
    info_display_string(fields[m++], res->sa.s, res->execute.run.run_user, 1) ;
    info_display_string(fields[m++], res->sa.s, res->execute.run.build, 1) ;
    info_display_string(fields[m++], res->sa.s, res->execute.run.runas, 1) ;
    info_display_string(fields[m++], res->sa.s, res->execute.finish.run, 1) ;
    info_display_string(fields[m++], res->sa.s, res->execute.finish.run_user, 1) ;
    info_display_string(fields[m++], res->sa.s, res->execute.finish.build, 1) ;
    info_display_string(fields[m++], res->sa.s, res->execute.finish.runas, 1) ;
    info_display_int(fields[m++], res->execute.timeout.start) ;
    info_display_int(fields[m++], res->execute.timeout.stop) ;
    info_display_int(fields[m++], res->execute.down) ;
    info_display_int(fields[m++], res->execute.downsignal) ;

    info_display_string(fields[m++], res->sa.s, res->live.livedir, 1) ;
    info_display_string(fields[m++], res->sa.s, res->live.status, 1) ;
    info_display_string(fields[m++], res->sa.s, res->live.servicedir, 1) ;
    info_display_string(fields[m++], res->sa.s, res->live.scandir, 1) ;
    info_display_string(fields[m++], res->sa.s, res->live.statedir, 1) ;
    info_display_string(fields[m++], res->sa.s, res->live.eventdir, 1) ;
    info_display_string(fields[m++], res->sa.s, res->live.notifdir, 1) ;
    info_display_string(fields[m++], res->sa.s, res->live.supervisedir, 1) ;
    info_display_string(fields[m++], res->sa.s, res->live.fdholderdir, 1) ;
    info_display_string(fields[m++], res->sa.s, res->live.oneshotddir, 1) ;

    info_display_string(fields[m++], res->sa.s, res->logger.name, 1) ;
    info_display_int(fields[m++], res->logger.backup) ;
    info_display_int(fields[m++], res->logger.maxsize) ;
    info_display_int(fields[m++], res->logger.timestamp) ;
    info_display_int(fields[m++], res->logger.want) ;
    info_display_string(fields[m++], res->sa.s, res->logger.execute.run.run, 1) ;
    info_display_string(fields[m++], res->sa.s, res->logger.execute.run.run_user, 1) ;
    info_display_string(fields[m++], res->sa.s, res->logger.execute.run.build, 1) ;
    info_display_string(fields[m++], res->sa.s, res->logger.execute.run.runas, 1) ;
    info_display_int(fields[m++], res->logger.execute.timeout.start) ;
    info_display_int(fields[m++], res->logger.execute.timeout.stop) ;

    info_display_string(fields[m++], res->sa.s, res->environ.env, 1) ;
    info_display_string(fields[m++], res->sa.s, res->environ.envdir, 1) ;
    info_display_int(fields[m++], res->environ.env_overwrite) ;

    info_display_string(fields[m++], res->sa.s, res->regex.configure, 1) ;
    info_display_string(fields[m++], res->sa.s, res->regex.directories, 1) ;
    info_display_string(fields[m++], res->sa.s, res->regex.files, 1) ;
    info_display_string(fields[m++], res->sa.s, res->regex.infiles, 1) ;
    info_display_int(fields[m++], res->regex.ndirectories) ;
    info_display_int(fields[m++], res->regex.nfiles) ;
    info_display_int(fields[m++], res->regex.ninfiles) ;

    info_display_int(fields[m++], res->io.fdin.type) ;
    info_display_string(fields[m++],res->sa.s, res->io.fdin.destination, 1) ;
    info_display_int(fields[m++], res->io.fdout.type) ;
    info_display_string(fields[m++], res->sa.s, res->io.fdout.destination, 1) ;
    info_display_int(fields[m++], res->io.fderr.type) ;
    info_display_string(fields[m++], res->sa.s, res->io.fderr.destination, 1) ;
    info_display_string(fields[m], res->sa.s, res->rversion, 1) ;

}

int ssexec_resolve(int argc, char const *const *argv, ssexec_t *info)
{
    int r = 0 ;

    char const *svname = 0 ;

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    char service_buf[MAXOPTS][INFO_FIELD_MAXLEN] = {
        "name",
        "description" ,
        "version",
        "type",
        "notify",
        "maxdeath",
        "earlier",
        "copyfrom",
        "intree",
        "ownerstr",
        "owner",
        "treename",
        "user" ,
        "inns",
        "enabled",
        "islog",

        "home",
        "frontend",
        "servicedir",

        "depends",
        "requiredby",
        "optsdeps",
        "contents",
        "ndepends",
        "nrequiredby",
        "noptsdeps",
        "ncontents",

        "run",
        "run_user",
        "run_build",
        "run_runas",
        "finish",
        "finish_user",
        "finish_build",
        "finish_runas",
        "timeoutstart",
        "timeoutstop",
        "down",
        "downsignal",

        "livedir",
        "status",
        "servicedir",
        "scandir",
        "statedir",
        "eventdir",
        "notifdir",
        "supervisedir",
        "fdholderdir",
        "oneshotddir",

        "logname" ,
        "logbackup" ,
        "logmaxsize" ,
        "logtimestamp" ,
        "logwant",
        "logrun" ,
        "logrun_user" ,
        "logrun_build" ,
        "logrun_runas" ,
        "logtimeoutstart",
        "logtimeoutstop",

        "env",
        "envdir",
        "env_overwrite",

        "configure",
        "directories",
        "files",
        "infiles",
        "ndirectories",
        "nfiles",
        "ninfiles",

        "stdintype",
        "stdindest",
        "stdouttype",
        "stdoutdest",
        "stderrtype",
        "stderrdest",
        "rversion",
    } ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = subgetopt_r(argc, argv, OPTS_RESOLVE, &l) ;
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

    if (argc < 1)
        log_usage(info->usage, "\n", info->help) ;

    svname = *argv ;

    r = service_is_g(svname, STATE_FLAGS_ISPARSED) ;
    if (r == -1)
        log_dieu(LOG_EXIT_SYS, "get information of service: ", svname, " -- please a bug report") ;
    else if (!r || r == STATE_FLAGS_FALSE)
        log_die(LOG_EXIT_USER, "service: ", svname, " is not parsed -- try to parse it first using '66 parse ", svname, "'") ;

    if (resolve_read_g(wres, info->base.s, svname) <= 0)
        log_dieusys(LOG_EXIT_SYS, "read resolve file") ;

    info_field_align(service_buf, fields, field_suffix,MAXOPTS) ;

    info_display_service_field(&res) ;

    resolve_free(wres) ;

    return 0 ;
}
