/*
 * ssexec_configure.c
 *
 * Copyright (c) 2019 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>//getenv
#include <unistd.h>//_exit,access

#include <oblibs/log.h>
#include <oblibs/exec.h>
#include <oblibs/opt.h>
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/strbuf.h>
#include <oblibs/sbl.h>
#include <oblibs/lexer.h>
#include <oblibs/environ.h>
#include <oblibs/stream.h>

#include <66/ssexec.h>
#include <66/utils.h>
#include <66/config.h>
#include <66/environ.h>
#include <66/constants.h>
#include <66/resolve.h>
#include <66/write.h>
#include <66/state.h>
#include <66/service.h>
#include <66/symlink.h>

static char const *EDITOR = 0 ;

enum tasks_e
{
    T_UNSET = 0 ,
    T_EDIT ,
    T_VLIST ,
    T_LIST ,
    T_REPLACE
} ;

#define MAXOPTS 3
#define checkopts(n) if (n >= MAXOPTS) log_die(LOG_EXIT_USER, "too many versions number")
#define DELIM ','

static uint8_t check_current_version(char const *svconf,char const *version)
{
    log_flow() ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    if (!env_find_current_version(&sa,svconf)) log_dieu(LOG_EXIT_SYS,"find current version") ;
    char bname[sa.len + 1] ;
    if (!ob_basename(bname,sa.s)) log_dieu(LOG_EXIT_SYS,"get basename of: ",sa.s) ;

    return !version_compare(bname,version) ? 1 : 0 ;
}

static void run_editor(char const *src, char const *sv)
{
    log_flow() ;

    size_t srclen = strlen(src), svlen = strlen(sv) ;
    char tsrc[srclen + 1 + svlen + 1] ;

    auto_strings(tsrc,src,"/",sv) ;

    if (!EDITOR) {

        EDITOR = getenv("EDITOR") ;

        if (!EDITOR) {

            log_die(LOG_EXIT_SYS,"EDITOR is not set at the environment variable -- please use the -e option to specify the editor to use e.g. 66 configure -e nano <service>.") ;
        }
    }
    char const *const newarg[3] = { EDITOR, tsrc, 0 } ;
    exec_path_die(newarg[0], newarg, (char const *const *)environ) ;
}

static void do_import(char const *svname, char const *svconf, char const *version, int svtype)
{
    log_flow() ;

    size_t pos = 0 ;
    _alloc_sbl_(stk, strlen(version) + 1) ;

    char *src_version = 0 ;
    char *dst_version = 0 ;

    if (!lexer_trim_with_delim(&stk,version,DELIM))
        log_dieu(LOG_EXIT_SYS,"clean string: ",version) ;

    checkopts(sbl_count(&stk)) ;

    src_version = stk.s ;
    pos = strlen(stk.s) + 1 ;
    dst_version = stk.s + pos ;

    if (!env_import_version_file(svname,svconf,src_version,dst_version,svtype))
        log_dieu(LOG_EXIT_SYS,"import configuration file from version: ",src_version," to version: ",dst_version) ;
}

static void write_user_env_file(char const *src, char const *sv)
{
    size_t srclen = strlen(src), svlen = strlen(sv) ;
    int r ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    char tsrc[srclen + 2 + svlen + 1] ;

    auto_strings(tsrc,src,"/",sv) ;

    errno = 0 ;

    if (access(tsrc, F_OK) < 0) {

        if (errno == ENOENT) {

            auto_strings(tsrc,src,"/.",sv) ;

            if (!strbuf_read_file(&sa,tsrc))
                log_dieusys(LOG_EXIT_SYS,"read environment file from: ",tsrc) ;

            r = str_contain(sa.s,"[ENDWARN]") ;
            if (r == -1)
                log_die(LOG_EXIT_SYS,"invalid upstream configuration file! Do you have modified it? Tries to parse the service again.") ;

            r++; // remove the last \n
            if (!write_environ(sv, sa.s + r, src))
                log_dieu(LOG_EXIT_SYS, "write environment") ;
        }
        else
            log_diesys(LOG_EXIT_SYS,"conflicting format of file: ",tsrc) ;
    }

}

static opt_t const opts_configure[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",     .arg = OPT_NONE,                             .help = "print this help" },
    { .id = 'c',         .shortname = 'c', .longname = "current",  .arg = OPT_REQUIRED, .argname = "number",    .help = "set version to use as default" },
    { .id = 's',         .shortname = 's', .longname = "specific", .arg = OPT_REQUIRED, .argname = "number",    .help = "specifies the version to handle" },
    { .id = 'V',         .shortname = 'V', .longname = "versions", .arg = OPT_NONE,                             .help = "lists available versioned configuration directories of the service" },
    { .id = 'L',         .shortname = 'L', .longname = "list",     .arg = OPT_NONE,                             .help = "lists the environment variables of the service" },
    { .id = 'r',         .shortname = 'r', .longname = "replace",  .arg = OPT_REQUIRED, .argname = "key=value", .help = "replace the value of the key" },
    { .id = 'e',         .shortname = 'e', .longname = "editor",   .arg = OPT_REQUIRED, .argname = "editor",    .help = "edit the file with editor" },
    { .id = 'i',         .shortname = 'i', .longname = "import",   .arg = OPT_REQUIRED, .argname = "src,dst",   .help = "import configuration files from src version to dst version" },
} ;

static strbuf cfg_satmp = STRBUF_ZERO ;
static strbuf cfg_savar = STRBUF_ZERO ;
static uint8_t opt_todo = T_UNSET ;
static uint8_t opt_current = 0 ;
static char const *opt_import = 0 ;

static int on_configure(int id, char const *arg, void *data)
{
    (void)data ;

    switch (id) {

        case 'c' :

            if (cfg_satmp.len)
                log_die(LOG_EXIT_USER, "-c and -s options are mutually exclusive") ;

            if (!auto_strbuf(&cfg_satmp, arg))
                log_die_nomem("strbuf") ;

            opt_current++ ;
            break ;

        case 's' :

            if (cfg_satmp.len)
                log_die(LOG_EXIT_USER, "-c and -s options are mutually exclusive") ;

            if (!auto_strbuf(&cfg_satmp, arg))
                log_die_nomem("strbuf") ;

            break ;

        case 'V' :

            if (opt_todo != T_UNSET)
                log_die(LOG_EXIT_USER, "options -V, -L and -r are mutually exclusive") ;
            opt_todo = T_VLIST ;
            break ;

        case 'L' :

            if (opt_todo != T_UNSET)
                log_die(LOG_EXIT_USER, "options -V, -L and -r are mutually exclusive") ;
            opt_todo = T_LIST ;
            break ;

        case 'r' :

            if (!sbl_add(&cfg_savar, arg))
                log_die_nomem("strbuf") ;

            if (opt_todo != T_UNSET && opt_todo != T_REPLACE)
                log_die(LOG_EXIT_USER, "options -V, -L and -r are mutually exclusive") ;
            opt_todo = T_REPLACE ;
            break ;

        case 'e' :

            EDITOR = arg ;
            break ;

        case 'i' :

            opt_import = arg ;
            break ;
    }

    return 0 ;
}

opt_cmd_t const cmd_configure = {
    .name = "66 configure",
    .help = "manage environment service files and its contents",
    .operands = "service",
    .opts = opts_configure,
    .nopts = OPT_COUNT(opts_configure),
    .on_option = &on_configure,
    .fn = &ssexec_configure,
} ;

int ssexec_configure(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    int r ;
    size_t pos = 0 ;
    _cleanup_strbuf_ strbuf src = STRBUF_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    /* drain option state into locals, then reset the statics for re-entrancy.
     * the two strbufs are moved (ownership transferred to the auto-freed locals)
     * and the statics emptied, so a nested re-dispatch starts clean. */
    _cleanup_strbuf_ strbuf satmp = cfg_satmp ;
    _cleanup_strbuf_ strbuf savar = cfg_savar ;
    cfg_satmp = (strbuf)STRBUF_ZERO ;
    cfg_savar = (strbuf)STRBUF_ZERO ;
    uint8_t todo = opt_todo, current = opt_current ;
    char const *sv = 0, *svconf = 0, *import = opt_import ;
    opt_todo = T_UNSET ;
    opt_current = 0 ;
    opt_import = 0 ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing service argument") ;

    sv = argv[0] ;

    if (todo == T_UNSET && !import && !current) todo = T_EDIT ;

    r = service_is_g(sv, STATE_FLAGS_ISPARSED) ;
    if (r == -1)
        log_dieusys(LOG_EXIT_SYS, "get information of service: ", sv, " -- please a bug report") ;
    else if (!r || r == STATE_FLAGS_FALSE) {
        log_die(LOG_EXIT_SYS, "service: ", sv, " is not parsed -- try to parse it first using '66 parse ", sv, "'") ;
    }

    if (resolve_read_g(wres, info->base.s, sv) <= 0)
        log_dieusys(LOG_EXIT_SYS,"read resolve file of: ", sv) ;

    if (!res.environ.envdir) {
        log_1_warn(sv," do not have configuration file") ;
        resolve_free(wres) ;
        return 0 ;
    }

    if (!env_get_destination(&src, &res))
        log_dieusys(LOG_EXIT_SYS, "get current environment version") ;

    svconf = res.sa.s + res.environ.envdir ;

    if (import) {
        do_import(sv,svconf,import,res.type) ;
        resolve_free(wres) ;
        return 0 ;
    }

    if (satmp.len) {

        src.len = 0 ;
        if (!env_append_version(&src, svconf, satmp.s))
            log_dieu(LOG_EXIT_ZERO, "append version") ;

        if (current) {

            size_t conflen = strlen(svconf) ;
            char sym[conflen + SS_SYM_VERSION_LEN + 1] ;
            auto_strings(sym, svconf, SS_SYM_VERSION) ;

            if (!symlink_atomic(src.s, sym))
                log_warnu_return(LOG_EXIT_ZERO, "symlink: ", sym, " to: ", src.s) ;

            log_info("Symlink switched successfully to version: ", src.s) ;

            resolve_free(wres) ;
            return 0 ;
        }

        satmp.len = 0 ;
    }

    resolve_free(wres) ;

    switch(todo)
    {
        case T_VLIST:
            {
                char const *exclude[2] = { SS_SYM_VERSION + 1, 0 } ;
                if (!sbl_dir_get(&satmp, svconf, exclude, S_IFDIR))
                    log_dieu(LOG_EXIT_SYS, "get versioned directory of: ", svconf) ;

                pos = 0 ;
                FOREACH_SBL(&satmp, pos) {

                    if (!ostream_puts(ostream_1, svconf) ||
                        !ostream_puts(ostream_1, "/") ||
                        !ostream_puts(ostream_1, satmp.s + pos))
                        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
                    if (check_current_version(svconf, satmp.s + pos)) {
                        if (!ostream_putflush(ostream_1, " current", 8))
                            log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
                    }
                    if (!ostream_putflush(ostream_1, "\n", 1))
                        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
                }
            }
            break ;

        case T_LIST:
            {
                char const *exclude[2] = { SS_SYM_VERSION + 1, 0 } ;
                if (!sbl_dir_get(&satmp, src.s, exclude, S_IFREG))
                    log_dieu(LOG_EXIT_SYS, "get versioned directory at: ", src.s) ;

                pos = 0 ;
                FOREACH_SBL(&satmp, pos) {

                    char *name = satmp.s + pos ;
                    _alloc_strbuf_(file, src.len + strlen(name) + 2) ;
                    auto_strings(file.s, src.s, "/", name) ;
                    size_t filen = file_get_size(file.s) ;
                    _alloc_strbuf_(list, filen + 1) ;

                    if (!strbuf_read_file(&list, file.s))
                        log_dieusys(LOG_EXIT_SYS,"read: ", file.s) ;

                    log_info("Contents of file: ", file.s, "\n", list.s) ;
                }
            }
            break ;

        case T_REPLACE:

            {
                /** the user configuration file may not exist yet
                 * We read the upstream file if it's the case and write
                 * the change to the user file */
                write_user_env_file(src.s, sv) ;

                _alloc_strbuf_(file, strlen(src.s) + strlen(sv) + 2) ;
                _cleanup_strbuf_ strbuf env = STRBUF_ZERO ;

                auto_strings(file.s, src.s, "/", sv) ;

                if (!environ_merge_file(&env, file.s))
                    log_dieusys(LOG_EXIT_SYS, "merge environment file: ", file.s) ;

                if (!environ_merge_environ(&env, &savar))
                    log_dieusys(LOG_EXIT_SYS, "merge environment from command line") ;

                if (!environ_rebuild(&env))
                    log_dieusys(LOG_EXIT_SYS, "rebuild environment") ;

                if (!file_write(file.s, env.s, env.len))
                    log_dieusys(LOG_EXIT_SYS,"write file: ", file.s) ;
            }
            break ;

        case T_EDIT:

            write_user_env_file(src.s,sv) ;

            run_editor(src.s, sv) ;

            break ;

        /** Can't happens */
        default: break ;
    }

    return 0 ;
}
