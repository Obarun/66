/*
 * ssexec_env.c
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

    stralloc sa = STRALLOC_ZERO ;
    if (!env_find_current_version(&sa,svconf)) log_dieu(LOG_EXIT_SYS,"find current version") ;
    char bname[sa.len + 1] ;
    if (!ob_basename(bname,sa.s)) log_dieu(LOG_EXIT_SYS,"get basename of: ",sa.s) ;
    stralloc_free(&sa) ;
    return !version_cmp(bname,version,SS_CONFIG_VERSION_NDOT) ? 1 : 0 ;
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
    stralloc sasrc = STRALLOC_ZERO ;

    char *src_version = 0 ;
    char *dst_version = 0 ;

    if (!sastr_clean_string_wdelim(&sasrc,version,DELIM))
        log_dieu(LOG_EXIT_SYS,"clean string: ",version) ;

    unsigned int n = sastr_len(&sasrc) ;
    checkopts(n) ;

    FOREACH_SASTR(&sasrc,pos) {

        if (!pos) src_version = sasrc.s + pos ;
        else dst_version = sasrc.s + pos ;
    }

    if (!env_import_version_file(svname,svconf,src_version,dst_version,svtype))
        log_dieu(LOG_EXIT_SYS,"import configuration file from version: ",src_version," to version: ",dst_version) ;

    stralloc_free(&sasrc) ;
}

static void replace_value_of_key(stralloc *srclist,char const *key)
{
    log_flow() ;

    stralloc sakey = STRALLOC_ZERO ;

    size_t pos = 0 ;

    int start = -1 ,end = -1 ;

    if (!auto_stra(&sakey,key))
        log_die_nomem("stralloc") ;

    if (!environ_get_key_nclean(&sakey,&pos))
        log_die(LOG_EXIT_SYS,"invalid format at key: ",key) ;

    sakey.len-- ;

    if (!auto_stra(&sakey,"="))
        log_die_nomem("stralloc") ;

    start = sastr_find(srclist,sakey.s) ;
    if (start == -1) {
        log_1_warnu("find key: ",sakey.s) ;
        return ;
    }

    end = get_len_until(srclist->s + start,'\n') ;

    if (end == -1)
        log_dieu(LOG_EXIT_SYS,"find end of line") ;

    sakey.len = 0 ;

    if (!stralloc_catb(&sakey,srclist->s + start,end ) ||
    !stralloc_0(&sakey))
        log_die_nomem("stralloc") ;

    if (!strcmp(sakey.s,key))
        return ;

    if (!sastr_replace(srclist,sakey.s,key))
        log_dieu(LOG_EXIT_SYS,"replace: ",sakey.s," by: ",key) ;

    if (!stralloc_0(srclist))
        log_die_nomem("stralloc") ;

    srclist->len-- ;

    stralloc_free(&sakey) ;
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

            write_environ(sv, sa.s + r, src) ;
        }
        else
            log_diesys(LOG_EXIT_SYS,"conflicting format of file: ",tsrc) ;
    }

    stralloc_free(&sa) ;
}

int ssexec_env(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    int r ;
    size_t pos = 0 ;
    stralloc satmp = STRALLOC_ZERO ;
    stralloc sasrc = STRALLOC_ZERO ;
    stralloc saversion = STRALLOC_ZERO ;
    stralloc eversion = STRALLOC_ZERO ;
    stralloc savar = STRALLOC_ZERO ;
    stralloc salist = STRALLOC_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    uint8_t todo = T_UNSET ;

    char const *sv = 0, *svconf = 0, *src = 0, *import = 0 ;

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

                        if (env_check_version(&saversion,l.arg) <= 0)
                            log_dieu(LOG_EXIT_SYS,"check version format") ;

                        break ;

                case 's' :

                        if (env_check_version(&eversion,l.arg) <= 0)
                            log_dieu(LOG_EXIT_SYS,"check version format") ;

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

    if (todo == T_UNSET && !import && !saversion.len && !eversion.len) todo = T_EDIT ;

    r = service_is_g(sv, STATE_FLAGS_ISPARSED) ;
    if (r == -1)
        log_dieusys(LOG_EXIT_SYS, "get information of service: ", sv, " -- please a bug report") ;
    else if (!r || r == STATE_FLAGS_FALSE) {
        log_warn("service: ", sv, " is not parsed -- try to parse it first using '66 parse ", sv, "'") ;
        goto freed ;
    }

    if (resolve_read_g(wres, info->base.s, sv) <= 0)
        log_dieusys(LOG_EXIT_SYS,"read resolve file of: ", sv) ;

    if (!res.environ.envdir) {
        log_1_warn(sv," do not have configuration file") ;
        goto freed ;
    }

    svconf = res.sa.s + res.environ.envdir ;

    if (saversion.len)
    {
        size_t conflen = strlen(svconf) ;
        char sym[conflen + SS_SYM_VERSION_LEN + 1] ;
        auto_strings(sym,svconf,SS_SYM_VERSION) ;

        if (!env_append_version(&saversion,svconf,saversion.s))
            log_dieu(LOG_EXIT_ZERO,"append version") ;

        r = scan_mode(saversion.s,S_IFDIR) ;
        if (r == -1 || !r) log_dieusys(LOG_EXIT_USER,"find the versioned directory: ",saversion.s) ;

        if (!atomic_symlink(saversion.s,sym,"ssexec_env"))
            log_warnu_return(LOG_EXIT_ZERO,"symlink: ",sym," to: ",saversion.s) ;

        log_info("symlink switched successfully to version: ",saversion.s) ;
    }

    if (import)
        do_import(sv,svconf,import,res.type) ;

    if (eversion.len)
    {
        if (!env_append_version(&sasrc,svconf,eversion.s))
            log_dieu(LOG_EXIT_SYS,"append version") ;

        src = sasrc.s ;
    }
    else
    {
        if (!auto_stra(&sasrc,svconf,SS_SYM_VERSION)) log_die_nomem("stralloc") ;
        if (sareadlink(&satmp,sasrc.s) == -1) log_dieusys(LOG_EXIT_SYS,"readlink: ",sasrc.s) ;
        if (!stralloc_0(&satmp) ||
        !stralloc_copy(&sasrc,&satmp)) log_die_nomem("stralloc") ;
        src = sasrc.s ;
    }

    satmp.len = 0 ;

    switch(todo)
    {
        case T_VLIST:
            {
                char const *exclude[2] = { SS_SYM_VERSION + 1, 0 } ;
                if (!sastr_dir_get(&satmp,svconf,exclude,S_IFDIR))
                    log_dieu(LOG_EXIT_SYS,"get versioned directory of: ",svconf) ;
            }
            for (pos = 0 ; pos < satmp.len; pos += strlen(satmp.s + pos) + 1)
            {
                if (buffer_puts(buffer_1, svconf) < 0)
                    log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
                if (buffer_puts(buffer_1, "/") < 0)
                    log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
                if (buffer_puts(buffer_1, satmp.s + pos) < 0)
                    log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
                if (check_current_version(svconf,satmp.s + pos))
                    if (buffer_putsflush(buffer_1, " current") < 0)
                        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
                if (buffer_putsflush(buffer_1, "\n") < 0)
                    log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
            }
            break ;

        case T_LIST:
            {
                char const *exclude[2] = { SS_SYM_VERSION + 1, 0 } ;
                if (!sastr_dir_get(&satmp,src,exclude,S_IFREG))
                    log_dieu(LOG_EXIT_SYS,"get versioned directory at: ",src) ;
            }
            for (pos = 0 ; pos < satmp.len; pos += strlen(satmp.s + pos) + 1)
            {
                salist.len = 0 ;
                char *name = satmp.s + pos ;
                if (!file_readputsa(&salist,src,name)) log_dieusys(LOG_EXIT_SYS,"read: ",src,"/",name) ;
                if (!stralloc_0(&salist)) log_die_nomem("stralloc") ;
                log_info("contents of file: ",src,"/",name,"\n",salist.s) ;
            }
            break ;

        case T_REPLACE:

            /** the user configuration file may not exist yet
             * We read the upstream file if it's the case and write
             * the change to the user file */
            write_user_env_file(src,sv) ;

            if (!file_readputsa(&salist,src,sv))
                log_dieusys(LOG_EXIT_SYS,"read: ",src,"/",sv) ;

            FOREACH_SASTR(&savar,pos) {

                char *key = savar.s + pos ;

                replace_value_of_key(&salist,key) ;
            }
            if (!auto_stra(&satmp,src,"/",sv)) log_die_nomem("stralloc") ;

            if (!openwritenclose_unsafe(satmp.s,salist.s,salist.len))
                log_dieusys(LOG_EXIT_SYS,"write file: ",satmp.s) ;
            break ;

        case T_EDIT:

            write_user_env_file(src,sv) ;

            run_editor(src, sv) ;

        /** Can't happens */
        default: break ;
    }

    freed:
        stralloc_free(&satmp) ;
        stralloc_free(&sasrc) ;
        stralloc_free(&saversion) ;
        stralloc_free(&eversion) ;
        stralloc_free(&savar) ;
        stralloc_free(&salist) ;
        resolve_free(wres) ;

    return 0 ;
}
