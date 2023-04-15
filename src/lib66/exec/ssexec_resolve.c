/*
 * ssexec_resolve.c
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
#include <66/service.h>
#include <66/info.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/config.h>
#include <66/state.h>

#define MAXOPTS 72

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

static void info_display_service_field(resolve_service_t *res)
{
    info_display_string(fields[0], res->sa.s, res->name, 0) ;
    info_display_string(fields[1], res->sa.s, res->description, 1) ;
    info_display_string(fields[2], res->sa.s, res->version, 1) ;
    info_display_int(fields[3], res->type) ;
    info_display_int(fields[4], res->notify) ;
    info_display_int(fields[5], res->maxdeath) ;
    info_display_int(fields[6], res->earlier) ;
    info_display_string(fields[7], res->sa.s, res->hiercopy, 1) ;
    info_display_string(fields[8], res->sa.s, res->intree, 1) ;
    info_display_string(fields[9], res->sa.s, res->ownerstr, 1) ;
    info_display_int(fields[10], res->owner) ;
    info_display_string(fields[11], res->sa.s, res->treename, 1) ;
    info_display_string(fields[12], res->sa.s, res->user, 1) ;
    info_display_string(fields[13], res->sa.s, res->inmodule, 1) ;

    info_display_string(fields[14], res->sa.s, res->path.home, 1) ;
    info_display_string(fields[15], res->sa.s, res->path.frontend, 1) ;
    info_display_string(fields[16], res->sa.s, res->path.status, 1) ;
    info_display_string(fields[17], res->sa.s, res->path.servicedir, 1) ;

    info_display_string(fields[18], res->sa.s, res->dependencies.depends, 1) ;
    info_display_string(fields[19], res->sa.s, res->dependencies.requiredby, 1) ;
    info_display_string(fields[20], res->sa.s, res->dependencies.optsdeps, 1) ;
    info_display_string(fields[21], res->sa.s, res->dependencies.contents, 1) ;
    info_display_int(fields[22], res->dependencies.ndepends) ;
    info_display_int(fields[23], res->dependencies.nrequiredby) ;
    info_display_int(fields[24], res->dependencies.noptsdeps) ;
    info_display_int(fields[25], res->dependencies.ncontents) ;

    info_display_string(fields[26], res->sa.s, res->execute.run.run, 1) ;
    info_display_string(fields[27], res->sa.s, res->execute.run.run_user, 1) ;
    info_display_string(fields[28], res->sa.s, res->execute.run.build, 1) ;
    info_display_string(fields[29], res->sa.s, res->execute.run.shebang, 1) ;
    info_display_string(fields[30], res->sa.s, res->execute.run.runas, 1) ;
    info_display_string(fields[31], res->sa.s, res->execute.finish.run, 1) ;
    info_display_string(fields[32], res->sa.s, res->execute.finish.run_user, 1) ;
    info_display_string(fields[33], res->sa.s, res->execute.finish.build, 1) ;
    info_display_string(fields[34], res->sa.s, res->execute.finish.shebang, 1) ;
    info_display_string(fields[35], res->sa.s, res->execute.finish.runas, 1) ;
    info_display_int(fields[36], res->execute.timeout.kill) ;
    info_display_int(fields[37], res->execute.timeout.finish) ;
    info_display_int(fields[38], res->execute.timeout.up) ;
    info_display_int(fields[39], res->execute.timeout.down) ;
    info_display_int(fields[40], res->execute.down) ;
    info_display_int(fields[41], res->execute.downsignal) ;

    info_display_string(fields[42], res->sa.s, res->live.livedir, 1) ;
    info_display_string(fields[43], res->sa.s, res->live.scandir, 1) ;
    info_display_string(fields[44], res->sa.s, res->live.statedir, 1) ;
    info_display_string(fields[45], res->sa.s, res->live.eventdir, 1) ;
    info_display_string(fields[46], res->sa.s, res->live.notifdir, 1) ;
    info_display_string(fields[47], res->sa.s, res->live.supervisedir, 1) ;
    info_display_string(fields[48], res->sa.s, res->live.fdholderdir, 1) ;
    info_display_string(fields[49], res->sa.s, res->live.oneshotddir, 1) ;

    info_display_string(fields[50], res->sa.s, res->logger.name, 1) ;
    info_display_string(fields[51], res->sa.s, res->logger.destination, 1) ;
    info_display_int(fields[52], res->logger.backup) ;
    info_display_int(fields[53], res->logger.maxsize) ;
    info_display_int(fields[54], res->logger.timestamp) ;
    info_display_string(fields[55], res->sa.s, res->logger.execute.run.run, 1) ;
    info_display_string(fields[56], res->sa.s, res->logger.execute.run.run_user, 1) ;
    info_display_string(fields[57], res->sa.s, res->logger.execute.run.build, 1) ;
    info_display_string(fields[58], res->sa.s, res->logger.execute.run.shebang, 1) ;
    info_display_string(fields[59], res->sa.s, res->logger.execute.run.runas, 1) ;
    info_display_int(fields[60], res->logger.execute.timeout.kill) ;
    info_display_int(fields[61], res->logger.execute.timeout.finish) ;

    info_display_string(fields[62], res->sa.s, res->environ.env, 1) ;
    info_display_string(fields[63], res->sa.s, res->environ.envdir, 1) ;
    info_display_int(fields[64], res->environ.env_overwrite) ;

    info_display_string(fields[65], res->sa.s, res->regex.configure, 1) ;
    info_display_string(fields[66], res->sa.s, res->regex.directories, 1) ;
    info_display_string(fields[67], res->sa.s, res->regex.files, 1) ;
    info_display_string(fields[68], res->sa.s, res->regex.infiles, 1) ;
    info_display_int(fields[69], res->regex.ndirectories) ;
    info_display_int(fields[70], res->regex.nfiles) ;
    info_display_int(fields[71], res->regex.ninfiles) ;

}

int ssexec_resolve(int argc, char const *const *argv, ssexec_t *info)
{
    int r = 0 ;

    char const *svname = 0 ;
    char atree[SS_MAX_TREENAME + 1] ;

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    argc-- ;
    argv++ ;

    if (argc < 1)
        log_usage(info->usage, "\n", info->help) ;

    svname = *argv ;

    char service_buf[MAXOPTS][INFO_FIELD_MAXLEN] = {
        "name",
        "description" ,
        "version",
        "type",
        "notify",
        "maxdeath",
        "earlier",
        "hiercopy",
        "intree",
        "ownerstr",
        "owner",
        "treename",
        "user" ,
        "inmodule", // 14

        "home",
        "frontend",
        "status", //17
        "servicedir",

        "depends",
        "requiredby",
        "optsdeps",
        "contents",
        "ndepends",
        "nrequiredby",
        "noptsdeps",
        "ncontents", // 25

        "run",
        "run_user",
        "run_build",
        "run_shebang",
        "run_runas",
        "finish",
        "finish_user",
        "finish_build",
        "finish_shebang",
        "finish_runas",
        "timeoutkill",
        "timeoutfinish",
        "timeoutup",
        "timeoutdown",
        "down",
        "downsignal", // 41

        "livedir",
        "scandir",
        "statedir",
        "eventdir",
        "notifdir",
        "supervisedir",
        "fdholderdir",
        "oneshotddir", //49

        "logname" ,
        "logdestination" ,
        "logbackup" ,
        "logmaxsize" ,
        "logtimestamp" ,
        "logrun" ,
        "logrun_user" ,
        "logrun_build" ,
        "logrun_shebang" ,
        "logrun_runas" ,
        "logtimeoutkill",
        "logtimeoutfinish", // 61

        "env",
        "envdir",
        "env_overwrite", // 64

        "configure",
        "directories",
        "files",
        "infiles",
        "ndirectories",
        "nfiles",
        "ninfiles" // 71

    } ;


    r = service_is_g(atree, svname, STATE_FLAGS_ISPARSED) ;
    if (r == -1)
        log_dieu(LOG_EXIT_SYS, "get information of service: ", svname, " -- please a bug report") ;
    else if (!r)
        log_die(LOG_EXIT_USER, svname, " is not parsed -- try to parse it first") ;

    if (!resolve_read_g(wres, info->base.s, svname))
        log_dieusys(LOG_EXIT_SYS, "read resolve file") ;

    info_field_align(service_buf, fields, field_suffix,MAXOPTS) ;

    info_display_service_field(&res) ;

    resolve_free(wres) ;

    return 0 ;
}
