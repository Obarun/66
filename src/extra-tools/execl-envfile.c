/*
 * execl-envfile.c
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
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include <oblibs/string.h>
#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>
#include <oblibs/files.h>
#include <oblibs/environ.h>
#include <oblibs/sastr.h>
#include <oblibs/lexer.h>

#include <skalibs/bytestr.h>
#include <skalibs/stralloc.h>
#include <skalibs/env.h>
#include <skalibs/djbunix.h>
#include <skalibs/env.h>
#include <skalibs/sgetopt.h>
#include <skalibs/exec.h>

#include <execline/execline.h>

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
    char tpath[MAXENV + 1] ;
    struct stat st ;
    stralloc modifs = STRALLOC_ZERO ;
    stralloc sa = STRALLOC_ZERO ;
    exlsn_t info = EXLSN_ZERO ;

    PROG = "execl-envfile" ;
    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;) {

            int opt = subgetopt_r(argc, argv, "hv:l:", &l) ;
            if (opt == -1)
                break ;

            switch (opt) {

                case 'h' :

                    info_help();
                    return 0 ;

                case 'v' :

                    if (!uint0_scan(l.arg, &VERBOSITY))
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

        if (!environ_merge_file(&modifs, path))
            die_or_exec(path, insist, argv, envp) ;

    } else if (S_ISDIR(st.st_mode)) {

        if (!environ_merge_dir(&modifs, path))
            die_or_exec(path, insist, argv, envp) ;
    } else {

        errno = EINVAL ;
        log_diesys(LOG_EXIT_USER, "invalid format for path: ", path) ;
    }

    if (!environ_substitute(&modifs, &info))
        log_dieusys(LOG_EXIT_SYS, "substitue environment variables") ;

    if (!environ_clean_unexport(&modifs))
        log_dieusys(LOG_EXIT_SYS, "remove exclamation mark from environment") ;

    /** be able to freed the stralloc before existing */
    char tmp[modifs.len + 1] ;
    sastr_to_char(tmp, &modifs) ;

    size_t tmplen = modifs.len, n = env_len(envp) + 1 + byte_count(modifs.s, modifs.len, '\0') ;
    char const *newenv[n + 1] ;

    if (!env_merge(newenv, n , envp, env_len(envp), tmp, tmplen))
        log_dieusys(LOG_EXIT_SYS, "build environment") ;

    modifs.len = 0 ;

    if (!environ_merge_arguments(&modifs, argv))
        log_dieu(LOG_EXIT_SYS, "merge environment with arguments") ;

    r = el_substitute(&sa, modifs.s, modifs.len, info.vars.s, info.values.s,
        genalloc_s(elsubst_t const, &info.data), genalloc_len(elsubst_t const, &info.data)) ;

    stralloc_free(&modifs) ;

    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "el_substitute") ;
    else if (!r) {
        stralloc_free(&sa) ;
        _exit(0) ;
    }

    char const *v[r + 1] ;
    if (!env_make (v, r , sa.s, sa.len))
        log_dieusys(LOG_EXIT_SYS, "make environment") ;

    v[r] = 0 ;

    mexec_fm(v, newenv, env_len(newenv), info.modifs.s, info.modifs.len) ;
}
