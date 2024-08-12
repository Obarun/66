/*
 * ssexec_configure.c
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
#include <stdlib.h>//getenv
#include <unistd.h>//_exit,access

#include <oblibs/log.h>
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/sastr.h>
#include <oblibs/stack.h>
#include <oblibs/environ.h>

#include <skalibs/sgetopt.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/buffer.h>
#include <skalibs/diuint32.h>
#include <skalibs/djbunix.h>
#include <skalibs/unix-transactional.h>//atomic_symlink
#include <skalibs/exec.h>

#include <66/ssexec.h>
#include <66/utils.h>
#include <66/config.h>
#include <66/environ.h>
#include <66/constants.h>
#include <66/resolve.h>
#include <66/write.h>
#include <66/state.h>

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

    _alloc_sa_(sa) ;
    if (!env_find_current_version(&sa,svconf)) log_dieu(LOG_EXIT_SYS,"find current version") ;
    char bname[sa.len + 1] ;
    if (!ob_basename(bname,sa.s)) log_dieu(LOG_EXIT_SYS,"get basename of: ",sa.s) ;

    return !version_cmp(bname,version,SS_SERVICE_VERSION_NDOT) ? 1 : 0 ;
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
    xexec_ae (newarg[0],newarg, (char const *const *)environ) ;
}

static void do_import(char const *svname, char const *svconf, char const *version, int svtype)
{
    log_flow() ;

    size_t pos = 0 ;
    _alloc_stk_(stk, strlen(version) + 1) ;

    char *src_version = 0 ;
    char *dst_version = 0 ;

    if (!lexer_trim_with_delim(&stk,version,DELIM))
        log_dieu(LOG_EXIT_SYS,"clean string: ",version) ;

    checkopts(stk.count) ;

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
    stralloc sa = STRALLOC_ZERO ;
    char tsrc[srclen + 2 + svlen + 1] ;

    auto_strings(tsrc,src,"/",sv) ;

    errno = 0 ;

    if (access(tsrc, F_OK) < 0) {

        if (errno == ENOENT) {

            auto_strings(tsrc,src,"/.",sv) ;

            if (!file_readputsa_g(&sa,tsrc))
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

    stralloc_free(&sa) ;
}

int ssexec_configure(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    int r ;
    size_t pos = 0 ;
    _alloc_sa_(satmp) ;
    _alloc_sa_(src) ;
    _alloc_sa_(savar) ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    uint8_t todo = T_UNSET, current = 0 ;

    char const *sv = 0, *svconf = 0, *import = 0 ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = subgetopt_r(argc, argv, OPTS_ENV, &l) ;
            if (opt == -1) break ;

            switch (opt)
            {
                case 'h' :

                    info_help(info->help, info->usage) ;
                    return 0 ;

                case 'c' :

                        if (satmp.len)
                            log_die(LOG_EXIT_USER, "-c and -s options are mutually exclusive") ;

                        if (!auto_stra(&satmp, l.arg))
                            log_die_nomem("stralloc") ;

                        current++ ;

                        break ;

                case 's' :

                        if (satmp.len)
                            log_die(LOG_EXIT_USER, "-c and -s options are mutually exclusive") ;

                        if (!auto_stra(&satmp, l.arg))
                            log_die_nomem("stralloc") ;

                        break ;

                case 'V' :

                        if (todo != T_UNSET) log_usage(info->usage, "\n", info->help) ;
                        todo = T_VLIST ;

                        break ;
                case 'L' :

                        if (todo != T_UNSET) log_usage(info->usage, "\n", info->help) ;
                        todo = T_LIST ;

                        break ;

                case 'r' :

                        if (!sastr_add_string(&savar,l.arg))
                            log_die_nomem("stralloc") ;

                        if (todo != T_UNSET && todo != T_REPLACE) log_usage(info->usage, "\n", info->help) ;
                        todo = T_REPLACE ;

                        break ;

                case 'e' :

                        EDITOR = l.arg ;
                        break ;

                case 'i' :

                        import = l.arg ;

                        break ;

                default :

                    log_usage(info->usage, "\n", info->help) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 1) log_usage(info->usage, "\n", info->help) ;
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

            if (!atomic_symlink(src.s, sym, "ssexec_configure"))
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
                if (!sastr_dir_get(&satmp, svconf, exclude, S_IFDIR))
                    log_dieu(LOG_EXIT_SYS, "get versioned directory of: ", svconf) ;

                pos = 0 ;
                FOREACH_SASTR(&satmp, pos) {

                    if (buffer_puts(buffer_1, svconf) < 0 ||
                        buffer_puts(buffer_1, "/") < 0 ||
                        buffer_puts(buffer_1, satmp.s + pos) < 0)
                        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
                    if (check_current_version(svconf, satmp.s + pos)) {
                        if (buffer_putsflush(buffer_1, " current") < 0)
                            log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
                    }
                    if (buffer_putsflush(buffer_1, "\n") < 0)
                        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
                }
            }
            break ;

        case T_LIST:
            {
                char const *exclude[2] = { SS_SYM_VERSION + 1, 0 } ;
                if (!sastr_dir_get(&satmp, src.s, exclude, S_IFREG))
                    log_dieu(LOG_EXIT_SYS, "get versioned directory at: ", src.s) ;

                pos = 0 ;
                FOREACH_SASTR(&satmp, pos) {

                    char *name = satmp.s + pos ;
                    _alloc_stk_(file, src.len + strlen(name) + 2) ;
                    auto_strings(file.s, src.s, "/", name) ;
                    size_t filen = file_get_size(file.s) ;
                    _alloc_stk_(list, filen + 1) ;

                    if (!stack_read_file(&list, file.s))
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

                _alloc_stk_(file, strlen(src.s) + strlen(sv) + 2) ;
                _alloc_sa_(env) ;

                auto_strings(file.s, src.s, "/", sv) ;

                if (!environ_merge_file(&env, file.s))
                    log_dieusys(LOG_EXIT_SYS, "merge environment file: ", file.s) ;

                if (!environ_merge_environ(&env, &savar))
                    log_dieusys(LOG_EXIT_SYS, "merge environment from command line") ;

                if (!environ_rebuild(&env))
                    log_dieusys(LOG_EXIT_SYS, "rebuild environment") ;

                if (!openwritenclose_unsafe(file.s, env.s, env.len))
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
