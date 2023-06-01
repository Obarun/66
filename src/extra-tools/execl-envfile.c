/*
 * execl-envfile.c
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

static void parse_env_var(stralloc *result, char const *line) ;

static void die_or_exec(char const *path, uint8_t insist, char const *const *argv, char const *const *envp)
{
    if (insist)
        log_dieusys(LOG_EXIT_SYS, "get environment from: ", path) ;
    else
        log_warnusys("get environment from: ", path) ;

    xexec_ae(argv[0], argv, envp) ;
}

static void substitute_env_var(stralloc *result, char const *mkey, char const *key, char const *regex)
{
    stralloc tmp = STRALLOC_ZERO ;

    if (!stralloc_copy(&tmp, result) ||
        !stralloc_0(&tmp))
            log_die_nomem("stralloc") ;

    if (!sastr_rebuild_in_nline(&tmp))
        log_dieu(LOG_EXIT_SYS,"rebuild line") ;

    char *by = getenv(key) ;

    if (!by) {

            /** check if key and mkey are the same
             * variable to avoid a variable calling itself.
             * we accept e.g ${PATH}=${PATH} only if PATH
             * is set on the environnement */
             if (!strcmp(mkey,key))
                log_die(LOG_EXIT_USER,"recursive call of variable: ",regex) ;

            /** if the key doesn't exist we let
             * the variable as it A.K.A. ${VAR}  */
            if (!environ_get_val_of_key(&tmp, key))
                return ;

            if (!stralloc_0(&tmp))
                log_die_nomem("stralloc") ;

            by = tmp.s ;
    }

    /** recursive call at variable definition.
     * parse first the recursive variable*/
    if (str_contain(by,"${") >= 0) {

        char tmp[strlen(key) + 1 + strlen(by) + 1] ;
        auto_strings(tmp,key,"=",by) ;

        parse_env_var(result, tmp) ;
    }

    if (!sastr_replace_all(result, regex, by))
        log_dieu(LOG_EXIT_SYS,"replace: ", regex, " by: ", by) ;

    stralloc_free(&tmp) ;
}

void parse_env_var(stralloc *result, char const *line)
{
    size_t spos = 0, pos = 0 ;
    stralloc subs = STRALLOC_ZERO ;

    /** be sure to deal with key=value */
    spos = get_sep_before(line,'=','\n') ;
    if (spos<= 0)
        log_dieu(LOG_EXIT_SYS,"get value from line: ", line) ;

    spos++ ; //remove the = character

    /** master key -- see comment at substitute_env_var */
    char mkey[spos + 1] ;
    memcpy(mkey,line,spos - 1) ;
    mkey[spos - 1] = 0 ;

    if (!auto_stra(&subs, line + spos))
        log_die_nomem("stralloc") ;

    if (!sastr_split_element_in_nline(&subs))
        log_dieu(LOG_EXIT_SYS,"split line") ;

    {
        FOREACH_SASTR(&subs, pos) {

            char *line = subs.s + pos ;
            size_t len = strlen(line) ;
            char key[len + 1] ;

            ssize_t rstart = 0 ;

            /** deal with multiple ${} on the
             * same line */
            while (rstart < len) {

                ssize_t r = get_len_until(line + rstart, '$') ;

                if (r == -1)
                    break ;

                rstart += r ;

                ssize_t start = rstart + 1 ;

                if (line[start] == '{') {

                    ssize_t rend = get_len_until(line + start , '}') ;

                    if (rend == -1) {
                        log_warn("unmatched { at line: ", line) ;
                        return ;
                    }

                    ssize_t end = rend - 1 ;
                    start++ ;

                    memcpy(key, line + start, end) ;
                    key[end] = 0 ;

                    char regex[rend + 3] ;
                    memcpy(regex,line + rstart, rend + 2) ;
                    regex[rend + 2] = 0 ;

                    substitute_env_var(result, mkey, key, regex) ;

                    rstart += rstart + rend + 2 ;

                } else {

                    log_warn("ignoring variable at line: ", line," -- missing {}" ) ;
                    rstart += rstart + 1 ;
                    return ;
                }
            }
        }
    }
    stralloc_free(&subs) ;
}

int main (int argc, char const *const *argv, char const *const *envp)
{

    int r = 0 ;
    uint8_t unexport = 0, insist = 1 ;
    size_t pos = 0, nvar = 0 ;
    char const *path = 0 ;
    char tpath[MAXENV + 1], tfile[MAXENV+1] ;
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

        if (!ob_basename(tfile, path))
            log_dieu(LOG_EXIT_SYS, "get basename of: ", path) ;

        if (!ob_dirname(tpath, path))
            log_dieu(LOG_EXIT_SYS, "get dirname of: ", path) ;

        if (!file_readputsa(&modifs, tpath, tfile))
            die_or_exec(path, insist, argv, envp) ;

        if (!environ_get_clean_env(&modifs))
            log_dieu(LOG_EXIT_SYS, "clean environment of: ", path) ;

        if (!auto_stra(&sa, modifs.s))
            log_die_nomem("stralloc") ;

        if (!environ_remove_unexport(&modifs, modifs.s, modifs.len))
            log_dieu(LOG_EXIT_SYS, "remove exclamation mark from: ", path) ;

        if (!sastr_split_string_in_nline(&modifs))
            log_dieu(LOG_EXIT_SYS, "build environment line of: ", path) ;

        nvar = sastr_nelement(&modifs) ;
        if (nvar > MAXVAR)
            log_dieusys(LOG_EXIT_SYS, "to many variables in file: ", path) ;

        if (modifs.len > MAXENV)
            log_die(LOG_EXIT_SYS, "environment string too long") ;

    } else if (S_ISDIR(st.st_mode)) {

        if (!environ_clean_envfile(&modifs, path))
            die_or_exec(path, insist, argv, envp) ;

        if (!stralloc_copy(&sa, &modifs))
            log_die_nomem("stralloc") ;

        if (!environ_remove_unexport(&modifs, modifs.s, modifs.len))
            log_dieu(LOG_EXIT_SYS, "remove exclamation mark from environment variables") ;

        if (!sastr_split_string_in_nline(&modifs))
            log_dieu(LOG_EXIT_SYS, "build environment line") ;

    } else {

        errno = EINVAL ;
        log_diesys(LOG_EXIT_USER, "invalid format for path: ", path) ;
    }

    {
        pos = 0 ;
        size_t len = modifs.len ;
        char t[len + 1] ;

        sastr_to_char(t, &modifs) ;

        for (pos = 0 ; pos < len ; pos += strlen(t + pos) + 1)
            parse_env_var(&modifs, t + pos) ;

        if (!sastr_split_string_in_nline(&modifs))
            log_dieu(LOG_EXIT_SYS,"split string") ;
    }

    /** be able to freed the stralloc before existing */
    char tmp[modifs.len + 1] ;
    sastr_to_char(tmp, &modifs) ;

    size_t tmplen = modifs.len, n = env_len(envp) + 1 + byte_count(modifs.s, modifs.len, '\0') ;
    char const *newenv[n + 1] ;

    if (!env_merge(newenv, n , envp, env_len(envp), tmp, tmplen))
        log_dieusys(LOG_EXIT_SYS, "build environment") ;

    {
        /** keep variable in environment if ! do not exist*/
        size_t tmplen = sa.len ;
        pos = 0 ;

        char tmp[sa.len + 1] ;

        sastr_to_char(tmp, &sa) ;

        while (pos < tmplen) {

            unexport = 0 ;
            sa.len = 0 ;

            if (!auto_stra(&sa, tmp))
                log_die_nomem("stralloc") ;

            if (!environ_get_key_nclean(&sa, &pos))
                log_dieu(LOG_EXIT_SYS, "get key from line: ", sa.s + pos) ;

            if (!sa.len)
                continue ;

            char tkey[strlen(sa.s) + 1] ;
            auto_strings(tkey, sa.s) ;

            sa.len = 0 ;

            if (!auto_stra(&sa, tmp))
                log_die_nomem("stralloc") ;

            pos-- ;// retrieve the '=' character
            if (!environ_get_val(&sa, &pos))
                log_dieu(LOG_EXIT_SYS, "get value from line: ", sa.s + pos) ;

            char *uval = sa.s ;

            if (sa.s[0] == VAR_UNEXPORT) {

                uval = sa.s + 1 ;
                unexport = 1 ;
            }

            if(sastr_cmp(&info.vars, tkey) == -1)
                if (!environ_substitute(tkey, uval, &info, newenv, unexport))
                    log_dieu(LOG_EXIT_SYS, "substitute value of: ", tkey, " by: ", uval) ;
        }
    }

    sa.len = 0 ;
    modifs.len = 0 ;

    if (!env_string(&modifs, argv, (unsigned int)argc))
        log_dieu(LOG_EXIT_SYS, "make environment string") ;

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
