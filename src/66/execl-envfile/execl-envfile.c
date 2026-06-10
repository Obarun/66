/*
 * execl-envfile.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#include <oblibs/log.h>
#include <oblibs/environ.h>
#include <oblibs/directory.h>
#include <oblibs/strbuf.h>
#include <oblibs/sbl.h>
#include <oblibs/subst.h>
#include <oblibs/types.h>

#include <skalibs/exec.h>
#include <skalibs/sgetopt.h>

#include <66/config.h>

#define USAGE "execl-envfile [ -h ] [ -v verbosity ] [ -l ] src prog"

static inline void info_help (void)
{
  static char const *help =
"\n"
"options :\n"
"   -h: print this help\n"
"   -v: increase/decrease verbosity\n"
"   -l: loose\n"
;

    log_info(USAGE,"\n",help) ;
}

static void die_or_exec(char const *path, uint8_t insist, char const *const *argv, char const *const *envp)
{
    if (insist)
        log_dieu(LOG_EXIT_SYS, "get environment from: ", path) ;
    else
        log_warnu("get environment from: ", path) ;

    xexec_ae(argv[0], argv, envp) ;
}

/** allowing to import uniquely one specific key
 * would be a good feature.
 * */

int main (int argc, char const *const *argv, char const *const *envp)
{

    int r = 0 ;
    uint8_t insist = 1 ;
    char const *path = 0 ;
    char tpath[SS_MAX_PATH + 1] ;
    struct stat st ;
    strbuf env = STRBUF_ZERO ;
    strbuf cmdline = STRBUF_ZERO ;
    subst_t info = SUBST_ZERO ;

    PROG = "execl-envfile" ;
    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;) {

            int opt = subgetopt_r(argc, argv, "hv:l", &l) ;
            if (opt == -1)
                break ;

            switch (opt) {

                case 'h' :

                    info_help();
                    return 0 ;

                case 'v' :

                    if (!u32_scan_strict(l.arg, &VERBOSITY))
                        log_usage(USAGE) ;

                    break ;

                case 'l' :

                    insist = 0 ;
                    break ;

                default :

                    log_usage(USAGE) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 2)
        log_usage(USAGE) ;

    path = *argv ;
    argv++;
    argc--;

    if (path[0] == '.') {

        if (!dir_beabsolute(tpath, path))
            die_or_exec(path, insist, argv, envp) ;

        path = tpath ;
    }

    r = stat(path, &st) ;
    if (r < 0)
        die_or_exec(path, insist, argv, envp) ;

    if (S_ISREG(st.st_mode)) {

        if (!environ_merge_file(&env, path))
            die_or_exec(path, insist, argv, envp) ;

    } else if (S_ISDIR(st.st_mode)) {

        if (!environ_merge_dir(&env, path))
            die_or_exec(path, insist, argv, envp) ;
    } else {

        errno = EINVAL ;
        log_diesys(LOG_EXIT_USER, "invalid format for path: ", path) ;
    }

    // substitute variable inside the environment
    if (!environ_substitute(&env, &info))
        log_dieusys(LOG_EXIT_SYS, "substitue environment variables") ;

    // remove exclamation mark
    if (!environ_clean_unexport(&env))
        log_dieusys(LOG_EXIT_SYS, "remove exclamation mark from environment") ;

    // create new environment merging the default one
    // with the variable found at file/directory
    size_t elen = environ_length(envp) ;
    size_t n = elen + 1 + sbl_count(&env) ;
    char const *nenvp[n + 1] ;

    if (!environ_merge(nenvp, n , envp, elen, env.s, env.len))
        log_dieusys(LOG_EXIT_SYS, "build environment") ;

    // import execline script
    strbuf sa = STRBUF_ZERO ;
    if (!environ_import_arguments(&sa, argv, argc))
        log_dieusys(LOG_EXIT_SYS, "import arguments to environment") ;

    // substitute variable inside the execline script
    r = subst(&cmdline, sa.s, sa.len, &info) ;

    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "el_substitute") ;
    else if (!r) {
        strbuf_free(&cmdline) ;
        strbuf_free(&env) ;
        strbuf_free(&sa) ;
        subst_free(&info) ;
        _exit(0) ;
    }
    strbuf_free(&sa) ;

    char const *nargv[r + 1] ;
    if (!environ_make(nargv, r, cmdline.s, cmdline.len))
        log_dieusys(LOG_EXIT_SYS, "make environment") ;

    // end of el_substandrun_str

    xmexec_em(nargv, nenvp, info.modifs.s, info.modifs.len) ;
}
