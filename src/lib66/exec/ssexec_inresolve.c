/*
 * ssexec_inresolve.c
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
#include <66/service.h>
#include <66/info.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/config.h>
#include <66/state.h>

#define MAXOPTS 84

static wchar_t const field_suffix[] = L" :" ;
static char fields[INFO_NKEY][INFO_FIELD_MAXLEN] = {{ 0 }} ;

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
    info_display_string(fields[16], res->sa.s, res->path.tree, 1) ;
    info_display_string(fields[17], res->sa.s, res->path.status, 1) ;

    info_display_string(fields[18], res->sa.s, res->dependencies.depends, 1) ;
    info_display_string(fields[19], res->sa.s, res->dependencies.requiredby, 1) ;
    info_display_string(fields[20], res->sa.s, res->dependencies.optsdeps, 1) ;
    info_display_int(fields[21], res->dependencies.ndepends) ;
    info_display_int(fields[22], res->dependencies.nrequiredby) ;
    info_display_int(fields[23], res->dependencies.noptsdeps) ;

    info_display_string(fields[24], res->sa.s, res->execute.run.run, 1) ;
    info_display_string(fields[25], res->sa.s, res->execute.run.run_user, 1) ;
    info_display_string(fields[26], res->sa.s, res->execute.run.build, 1) ;
    info_display_string(fields[27], res->sa.s, res->execute.run.shebang, 1) ;
    info_display_string(fields[28], res->sa.s, res->execute.run.runas, 1) ;
    info_display_string(fields[29], res->sa.s, res->execute.finish.run, 1) ;
    info_display_string(fields[30], res->sa.s, res->execute.finish.run_user, 1) ;
    info_display_string(fields[31], res->sa.s, res->execute.finish.build, 1) ;
    info_display_string(fields[32], res->sa.s, res->execute.finish.shebang, 1) ;
    info_display_string(fields[33], res->sa.s, res->execute.finish.runas, 1) ;
    info_display_int(fields[34], res->execute.timeout.kill) ;
    info_display_int(fields[35], res->execute.timeout.finish) ;
    info_display_int(fields[36], res->execute.timeout.up) ;
    info_display_int(fields[37], res->execute.timeout.down) ;
    info_display_int(fields[38], res->execute.down) ;
    info_display_int(fields[39], res->execute.downsignal) ;

    info_display_string(fields[40], res->sa.s, res->live.livedir, 1) ;
    info_display_string(fields[41], res->sa.s, res->live.scandir, 1) ;
    info_display_string(fields[42], res->sa.s, res->live.statedir, 1) ;
    info_display_string(fields[43], res->sa.s, res->live.eventdir, 1) ;
    info_display_string(fields[44], res->sa.s, res->live.notifdir, 1) ;
    info_display_string(fields[45], res->sa.s, res->live.supervisedir, 1) ;
    info_display_string(fields[46], res->sa.s, res->live.fdholderdir, 1) ;
    info_display_string(fields[47], res->sa.s, res->live.oneshotddir, 1) ;

    info_display_string(fields[48], res->sa.s, res->logger.name, 1) ;
    info_display_string(fields[49], res->sa.s, res->logger.destination, 1) ;
    info_display_int(fields[50], res->logger.backup) ;
    info_display_int(fields[51], res->logger.maxsize) ;
    info_display_int(fields[52], res->logger.timestamp) ;
    info_display_string(fields[53], res->sa.s, res->logger.execute.run.run, 1) ;
    info_display_string(fields[54], res->sa.s, res->logger.execute.run.run_user, 1) ;
    info_display_string(fields[55], res->sa.s, res->logger.execute.run.build, 1) ;
    info_display_string(fields[56], res->sa.s, res->logger.execute.run.shebang, 1) ;
    info_display_string(fields[57], res->sa.s, res->logger.execute.run.runas, 1) ;
    info_display_int(fields[58], res->logger.execute.timeout.kill) ;
    info_display_int(fields[59], res->logger.execute.timeout.finish) ;

    info_display_string(fields[60], res->sa.s, res->environ.env, 1) ;
    info_display_string(fields[61], res->sa.s, res->environ.envdir, 1) ;
    info_display_int(fields[62], res->environ.env_overwrite) ;

    info_display_string(fields[63], res->sa.s, res->regex.configure, 1) ;
    info_display_string(fields[64], res->sa.s, res->regex.directories, 1) ;
    info_display_string(fields[65], res->sa.s, res->regex.files, 1) ;
    info_display_string(fields[66], res->sa.s, res->regex.infiles, 1) ;
    info_display_int(fields[67], res->regex.ndirectories) ;
    info_display_int(fields[68], res->regex.nfiles) ;
    info_display_int(fields[69], res->regex.ninfiles) ;

}

int ssexec_inresolve(int argc, char const *const *argv, ssexec_t *info)
{
    int found = 0, what = 0 ;
    uint8_t master = 0 ;

    stralloc sa = STRALLOC_ZERO ;
    char const *svname = 0 ;
    char const *treename = info->treename.s ;
    char atree[SS_MAX_TREENAME + 1] ;

    argc-- ;
    argv++ ;

    if (argc < 2) log_usage(usage_inresolve) ;

    what = parse_what(*argv) ;
    if (what == 2)
        log_usage(usage_inresolve) ;

    argv++;
    argc--;
    svname = *argv ;

    if (!set_ownersysdir(&sa, getuid()))
        log_dieu(LOG_EXIT_SYS, "set owner directory") ;

    if (!what) {

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
            "tree",
            "status", //18

            "depends",
            "requiredby",
            "optsdeps",
            "ndepends",
            "nrequiredby",
            "noptsdeps", // 24

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
            "downsignal", // 40

            "livedir",
            "scandir",
            "statedir",
            "eventdir",
            "notifdir",
            "supervisedir",
            "fdholderdir",
            "oneshotddir", //48

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
            "logtimeoutfinish", // 60

            "env",
            "envdir",
            "env_overwrite", // 63

            "configure",
            "directories",
            "files",
            "infiles",
            "ndirectories",
            "nfiles",
            "ninfiles", // 70

            "classic",
            "bundle",
            "oneshot",
            "module",
            "enabled",
            "disabled",
            "contents",
            "nclassic",
            "nbundle",
            "noneshot",
            "nmodule",
            "nenabled",
            "ndisabled",
            "ncontents" // 84
        } ;

        resolve_wrapper_t_ref wres = 0 ;
        resolve_service_t res = RESOLVE_SERVICE_ZERO ;
        resolve_service_master_t mres = RESOLVE_SERVICE_MASTER_ZERO ;

        if (!strcmp(svname, SS_MASTER + 1)) {

            master = 1 ;
            wres = resolve_set_struct(DATA_SERVICE_MASTER, &mres) ;

        } else {

            wres = resolve_set_struct(DATA_SERVICE, &res) ;
        }


        if (!master) {

            found = service_is_g(atree, svname, STATE_FLAGS_ISPARSED) ;
            if (found == -1)
                log_dieu(LOG_EXIT_SYS, "get information of service: ", svname, " -- please a bug report") ;
            else if (!found)
                log_die(LOG_EXIT_USER, "unknown service: ", svname) ;

            if (!resolve_read_g(wres, sa.s, svname))
                log_dieusys(LOG_EXIT_SYS,"read resolve file") ;

        } else {

            char solve[sa.len + SS_SYSTEM_LEN + 1 + strlen(treename) + SS_SVDIRS_LEN + 1] ;
            auto_strings(solve, sa.s, SS_SYSTEM, "/", treename, SS_SVDIRS) ;

            if (!resolve_read(wres, solve, SS_MASTER + 1))
                log_dieusys(LOG_EXIT_SYS,"read resolve file") ;
        }

        info_field_align(service_buf, fields, field_suffix,MAXOPTS) ;

        if (!master) {

            info_display_service_field(&res) ;

        } else {

                info_display_string(fields[0], mres.sa.s, mres.name, 0) ;
                info_display_string(fields[70], mres.sa.s, mres.classic, 1) ;
                info_display_string(fields[71], mres.sa.s, mres.bundle, 1) ;
                info_display_string(fields[72], mres.sa.s, mres.oneshot, 1) ;
                info_display_string(fields[73], mres.sa.s, mres.module, 1) ;
                info_display_string(fields[74], mres.sa.s, mres.enabled, 1) ;
                info_display_string(fields[75], mres.sa.s, mres.disabled, 1) ;
                info_display_string(fields[76], mres.sa.s, mres.contents, 1) ;

                info_display_int(fields[77], mres.nclassic) ;
                info_display_int(fields[78], mres.nbundle) ;
                info_display_int(fields[79], mres.noneshot) ;
                info_display_int(fields[80], mres.nmodule) ;
                info_display_int(fields[81], mres.nenabled) ;
                info_display_int(fields[82], mres.ndisabled) ;
                info_display_int(fields[83], mres.ncontents) ;
        }

        resolve_free(wres) ;

    } else {

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
            "disen", // 13
            // Master
            "enabled",
            "current",
            "contents",
            "nenabled",
            "ncontents" // 18
        } ;

        resolve_wrapper_t_ref wres = 0 ;
        resolve_tree_t tres = RESOLVE_TREE_ZERO ;
        resolve_tree_master_t mres = RESOLVE_TREE_MASTER_ZERO ;

        if (!strcmp(argv[0], SS_MASTER + 1)) {

            master = 1 ;
            wres = resolve_set_struct(DATA_TREE_MASTER, &mres) ;

        } else {

            wres = resolve_set_struct(DATA_TREE, &tres) ;
        }

        found = tree_isvalid(sa.s, svname) ;
        if (found < 0)
            log_diesys(LOG_EXIT_SYS, "invalid tree directory") ;

        if (!found)
            log_dieusys(LOG_EXIT_SYS, "find tree: ", svname) ;

        if (!resolve_read_g(wres, sa.s, svname))
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
            info_display_int(fields[12], tres.disen) ;

        } else {

            info_display_string(fields[0], mres.sa.s, mres.name, 1) ;
            info_display_string(fields[3], mres.sa.s, mres.allow, 1) ;
            info_display_string(fields[13], mres.sa.s, mres.enabled, 1) ;
            info_display_string(fields[14], mres.sa.s, mres.current, 1) ;
            info_display_string(fields[15], mres.sa.s, mres.contents, 1) ;
            info_display_int(fields[16], mres.nenabled) ;
            info_display_int(fields[17], mres.ncontents) ;
        }

        resolve_free(wres) ;

    }

    stralloc_free(&sa) ;

    return 0 ;
}
