/*
 * ssexec_env.c
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
#include <stdlib.h>//getenv
#include <unistd.h>//_exit,access

#include <oblibs/obgetopt.h>
#include <oblibs/log.h>
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/sastr.h>
#include <oblibs/environ.h>

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
#include <66/parser.h>
#include <66/environ.h>
#include <66/constants.h>
#include <66/resolve.h>

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

static void run_editor(char const *src, char const *const *envp)
{
    log_flow() ;

    char *editor = getenv("EDITOR") ;
    if (!editor) {
        editor = getenv("SUDO_USER") ;
        if (editor) log_dieu(LOG_EXIT_SYS,"get EDITOR with sudo command -- please try to use the -E sudo option e.g. sudo -E 66-env -e <service>") ;
        else log_dieusys(LOG_EXIT_SYS,"get EDITOR") ;
    }
    char const *const newarg[3] = { editor, src, 0 } ;
    xexec_ae (newarg[0],newarg,envp) ;
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

int ssexec_env(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
    int r ;
    size_t pos = 0 ;
    stralloc satmp = STRALLOC_ZERO ;
    stralloc sasrc = STRALLOC_ZERO ;
    stralloc saversion = STRALLOC_ZERO ;
    stralloc eversion = STRALLOC_ZERO ;
    stralloc savar = STRALLOC_ZERO ;
    stralloc salist = STRALLOC_ZERO ;
    ss_resolve_t res = RESOLVE_ZERO ;

    uint8_t todo = T_UNSET ;

    char const *sv = 0, *svconf = 0, *src = 0, *treename = 0, *import = 0 ;

    {
        subgetopt_t l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = getopt_args(argc,argv, ">c:s:VLr:ei:", &l) ;
            if (opt == -1) break ;
            if (opt == -2) log_die(LOG_EXIT_USER,"options must be set first") ;
            switch (opt)
            {
                case 'c' :

                        if (env_check_version(&saversion,l.arg) <= 0)
                            log_dieu(LOG_EXIT_SYS,"check version format") ;

                        break ;

                case 's' :

                        if (env_check_version(&eversion,l.arg) <= 0)
                            log_dieu(LOG_EXIT_SYS,"check version format") ;

                        break ;

                case 'V' :

                        if (todo != T_UNSET) log_usage(usage_env) ;
                        todo = T_VLIST ;

                        break ;
                case 'L' :

                        if (todo != T_UNSET) log_usage(usage_env) ;
                        todo = T_LIST ;

                        break ;

                case 'r' :

                        if (!sastr_add_string(&savar,l.arg))
                            log_die_nomem("stralloc") ;

                        if (todo != T_UNSET && todo != T_REPLACE) log_usage(usage_env) ;
                        todo = T_REPLACE ;

                        break ;

                case 'e' :

                        if (todo != T_UNSET) log_usage(usage_env) ;
                        todo = T_EDIT ;

                        break ;

                case 'i' :

                        import = l.arg ;

                        break ;

                default :

                    log_usage(usage_env) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 1) log_usage(usage_env) ;
    sv = argv[0] ;

    if (todo == T_UNSET && !import && !saversion.len && !eversion.len) todo = T_EDIT ;

    treename = !info->opt_tree ? 0 : info->treename.s ;

    r = ss_resolve_svtree(&sasrc,sv,treename) ;
    if (r == -1) log_dieu(LOG_EXIT_SYS,"resolve tree source of sv: ",sv) ;
    else if (!r) {
        log_info("no tree exist yet") ;
        goto freed ;
    }
    else if (r > 2) {
        log_die(LOG_EXIT_SYS,sv," is set on different tree -- please use -t options") ;
    }
    else if (r == 1) log_die(LOG_EXIT_SYS,"unknown service: ",sv, !info->opt_tree ? " in current tree: " : " in tree: ", info->treename.s) ;

    if (!ss_resolve_read(&res,sasrc.s,sv))
        log_dieusys(LOG_EXIT_SYS,"read resolve file of: ",sv) ;

    if (!res.srconf) {
        log_1_warn(sv," do not have configuration file") ;
        goto freed ;
    }

    svconf = res.sa.s + res.srconf ;
    sasrc.len = 0 ;

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

            if (!sastr_dir_get(&satmp,svconf,SS_SYM_VERSION + 1,S_IFDIR))
                log_dieu(LOG_EXIT_SYS,"get versioned directory of: ",svconf) ;
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

            if (!sastr_dir_get(&satmp,src,SS_SYM_VERSION + 1,S_IFREG))
                log_dieu(LOG_EXIT_SYS,"get versioned directory at: ",src) ;
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

            if (!file_readputsa(&salist,src,sv)) log_dieusys(LOG_EXIT_SYS,"read: ",src,"/",sv) ;

            FOREACH_SASTR(&savar,pos) {

                char *key = savar.s + pos ;

                replace_value_of_key(&salist,key) ;
            }
            if (!auto_stra(&satmp,src,"/",sv)) log_die_nomem("stralloc") ;

            if (!openwritenclose_unsafe(satmp.s,salist.s,salist.len))
                log_dieusys(LOG_EXIT_SYS,"write file: ",satmp.s) ;
            break ;

        case T_EDIT:

            {
                size_t srclen = strlen(src), svlen = strlen(sv) ;
                int r ;
                char tsrc[srclen + 1 + svlen +1] ;

                auto_strings(tsrc,src,"/",sv) ;

                errno = 0 ;
                if (access(tsrc, F_OK) < 0) {

                    if (errno == ENOENT) {

                        salist.len = 0 ;
                        stralloc sa = STRALLOC_ZERO ;

                        if (!auto_stra(&salist,src,"/.",sv))
                            log_die_nomem("stralloc") ;

                        if (!file_readputsa_g(&sa,salist.s))
                            log_dieusys(LOG_EXIT_SYS,"read environment file from: ",salist.s) ;

                        r = str_contain(sa.s,"[ENDWARN]") ;
                        if (r == -1)
                            log_die(LOG_EXIT_SYS,"invalid upstream configuration file! Do you have modified it? Tries to enable it again.") ;

                        if (!write_env(sv,sa.s + r,src))
                            log_dieusys(LOG_EXIT_SYS,"copy: ",salist.s," to: ",tsrc);

                        stralloc_free(&sa) ;
                    }
                    else
                        log_diesys(LOG_EXIT_SYS,"conflicting format of file: ",tsrc) ;

                }
                run_editor(tsrc,envp) ;
            }
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
        ss_resolve_free(&res) ;

    return 0 ;
}
