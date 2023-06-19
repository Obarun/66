/*
 * regex_configure.c
 *
 * Copyright (c) 2018-2022 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <sys/types.h>
#include <string.h>
#include <pwd.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/sastr.h>
#include <oblibs/environ.h>

#include <skalibs/stralloc.h>
#include <skalibs/types.h>
#include <skalibs/env.h>
#include <skalibs/djbunix.h>
#include <skalibs/bytestr.h>

#include <66/module.h>
#include <66/resolve.h>
#include <66/info.h>
#include <66/environ.h>
#include <66/write.h>

void regex_configure(resolve_service_t *res, ssexec_t *info, char const *path, char const *name)
{
    log_flow() ;

    int wstat, r ;
    pid_t pid ;
    size_t clen = res->regex.configure > 0 ? 1 : 0 ;
    size_t pathlen = strlen(path), n ;

    stralloc env = STRALLOC_ZERO ;

    char const *newargv[2 + clen] ;
    unsigned int m = 0 ;

    char pwd[pathlen + 1 + SS_MODULE_CONFIG_DIR_LEN + 1] ;
    auto_strings(pwd, path, "/", SS_MODULE_CONFIG_DIR + 1) ;

    char config_script[pathlen + 1 + SS_MODULE_CONFIG_DIR_LEN + 1 + SS_MODULE_CONFIG_SCRIPT_LEN + 1] ;
    auto_strings(config_script, path, "/", SS_MODULE_CONFIG_DIR + 1, "/", SS_MODULE_CONFIG_SCRIPT) ;

    r = scan_mode(config_script, S_IFREG) ;
    if (r > 0)
    {
        /** export ssexec_t info value on the environment */
        {
            char verbo[UINT_FMT];
            verbo[uid_fmt(verbo, VERBOSITY)] = 0 ;
            if (!auto_stra(&env, \
            "MOD_NAME=", name, "\n", \
            "MOD_BASE=", res->sa.s + res->path.home, "\n", \
            "MOD_LIVE=", info->live.s, "\n", \
            "MOD_SCANDIR=", info->scandir.s, "\n", \
            "MOD_TREENAME=", res->sa.s + res->treename, "\n", \
            "MOD_OWNER=", res->sa.s + res->ownerstr, "\n", \
            "MOD_COLOR=", info->opt_color ? "1" : "0", "\n", \
            "MOD_VERBOSITY=", verbo, "\n", \
            "MOD_MODULE_DIR=", path, "\n", \
            "MOD_SKEL_DIR=", SS_SKEL_DIR, "\n", \
            "MOD_SERVICE_SYSDIR=", SS_SERVICE_SYSDIR, "\n", \
            "MOD_SERVICE_ADMDIR=", SS_SERVICE_ADMDIR, "\n", \
            "MOD_SERVICE_ADMCONFDIR=", SS_SERVICE_ADMCONFDIR, "\n", \
            "MOD_SCRIPT_SYSDIR=", SS_SCRIPT_SYSDIR, "\n", \
            "MOD_USER_DIR=", SS_USER_DIR, "\n", \
            "MOD_SERVICE_USERDIR=", SS_SERVICE_USERDIR, "\n", \
            "MOD_SERVICE_USERCONFDIR=", SS_SERVICE_USERCONFDIR, "\n", \
            "MOD_SCRIPT_USERDIR=", SS_SCRIPT_USERDIR, "\n"))
                log_dieu(LOG_EXIT_SYS, "append environment variables") ;
        }

        /** environment is not mandatory */
        if (res->environ.env > 0)
        {
            stralloc oenv = STRALLOC_ZERO ;
            stralloc dst = STRALLOC_ZERO ;
            char name[strlen(res->sa.s + res->name) + 2] ;
            auto_strings(name, ".", res->sa.s + res->name) ;

            if (!env_prepare_for_write(&dst, &oenv, res))
                log_dieu(LOG_EXIT_SYS, "prepare environment") ;

            write_environ(name, oenv.s, dst.s) ;

            /** Reads all files from the directory */
            if (!environ_clean_envfile(&env, dst.s))
                log_dieu(LOG_EXIT_SYS, "read environment") ;

            if (!environ_remove_unexport(&env, env.s, env.len))
                log_dieu(LOG_EXIT_SYS, "remove exclamation mark from environment variables") ;

            stralloc_free(&oenv) ;
            stralloc_free(&dst) ;
        }

        if (!sastr_split_string_in_nline(&env))
            log_dieu(LOG_EXIT_SYS, "rebuild environment") ;

        n = env_len((const char *const *)environ) + 1 + byte_count(env.s,env.len,'\0') ;
        char const *newenv[n + 1] ;

        if (!env_merge (newenv, n ,(const char *const *)environ,env_len((const char *const *)environ), env.s, env.len))
            log_dieu(LOG_EXIT_SYS, "build environment") ;

        if (chdir(pwd) < 0)
            log_dieu(LOG_EXIT_SYS, "chdir to: ", pwd) ;

        m = 0 ;
        newargv[m++] = config_script ;

        if (res->regex.configure > 0)
            newargv[m++] = res->sa.s + res->regex.configure ;

        newargv[m++] = 0 ;

        log_info("launch configure script of module: ", name) ;

        pid = child_spawn0(newargv[0], newargv, newenv) ;

        if (waitpid_nointr(pid, &wstat, 0) < 0)
            log_dieusys(LOG_EXIT_SYS, "wait for: ", config_script) ;

        if (wstat)
            log_dieu(LOG_EXIT_SYS, "run: ", config_script) ;
    }

    stralloc_free(&env) ;
}
