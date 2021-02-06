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

int main (int argc, char const *const *argv, char const *const *envp)
{
    int r = 0, unexport = 0, insist = 1 ;
    size_t pos = 0, nvar = 0 ;
    char const *path = 0, *file = 0 ;
    char tpath[MAXENV + 1], tfile[MAXENV+1] ;
    stralloc again = STRALLOC_ZERO ;
    stralloc modifs = STRALLOC_ZERO ;
    stralloc dst = STRALLOC_ZERO ;
    stralloc key = STRALLOC_ZERO ;
    stralloc val = STRALLOC_ZERO ;

    exlsn_t info = EXLSN_ZERO ;

    PROG = "execl-envfile" ;
    {
        subgetopt_t l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = subgetopt_r(argc,argv, "hv:l", &l) ;
            if (opt == -1) break ;
            switch (opt)
            {
                case 'h' :  info_help(); return 0 ;
                case 'v' :  if (!uint0_scan(l.arg, &VERBOSITY)) log_usage(USAGE) ; break ;
                case 'l' :  insist = 0 ; break ;
                default :   log_usage(USAGE) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }
    if (argc < 2) log_usage(USAGE) ;

    path = *argv ;
    argv++;
    argc--;

    r = scan_mode(path,S_IFREG) ;
    if (r > 0) {
        if (!ob_basename(tfile,path))
            log_dieu(LOG_EXIT_SYS,"get file name of: ",path) ;

        file = tfile ;

        if (!ob_dirname(tpath,path))
            log_dieu(LOG_EXIT_SYS,"get parent path of: ",path) ;

        path = tpath ;
    }

    if (!r) {

        if (insist) {

            log_dieu(LOG_EXIT_SYS,"get file from: ",path) ;

        } else {

            log_warn("get file from: ",path) ;
            xexec_ae(argv[0],argv,envp) ;
        }
    }

    if (path[0] == '.')
    {
        if (!dir_beabsolute(tpath,path)) {

            if (insist) {

                log_dieusys(LOG_EXIT_SYS,"find relative path: ",path) ;

            } else {

                log_warn("find relative path: ",path) ;
                xexec_ae(argv[0],argv,envp) ;
            }
        }
        path = tpath ;
    }

    if (file)
    {
        if (!file_readputsa(&again,path,file)) {

            if (insist) {

                log_dieusys(LOG_EXIT_SYS,"read file: ",path,file) ;

            } else {

                log_warn("read file: ",path,file) ;
                xexec_ae(argv[0],argv,envp) ;
            }
        }

        if (!environ_get_clean_env(&again) ||
            !environ_remove_unexport(&modifs,&again))
                log_dieu(LOG_EXIT_SYS,"prepare modified environment of: ",file) ;

        modifs.len-- ;

        if (!sastr_split_string_in_nline(&modifs))
            log_dieu(LOG_EXIT_SYS,"build environment line of: ",file) ;

        if (!auto_stra(&dst,again.s,"\n"))
            log_die_nomem("stralloc") ;

        nvar = sastr_nelement(&again) ;
        if (nvar > MAXVAR) log_dieusys(LOG_EXIT_SYS,"to many variables in file: ",path,file) ;

        if (again.len > MAXENV)
            log_die(LOG_EXIT_SYS,"environment string too long") ;
    }
    else
    {
        if (!environ_clean_envfile(&modifs,path)) {

            if (insist) {

                log_dieu(LOG_EXIT_SYS,"clean environment file of: ",path) ;

            } else {
                log_warn("clean environment file of: ",path) ;
                xexec_ae(argv[0],argv,envp) ;
            }
        }

        if (!stralloc_copy(&dst,&modifs))
               log_die_nomem("stralloc") ;

        if (!environ_remove_unexport(&modifs,&modifs))
            log_dieu(LOG_EXIT_SYS,"remove exclamation mark from environment variables") ;

        if (!sastr_split_string_in_nline(&modifs))
            log_dieu(LOG_EXIT_SYS,"build environment line") ;
    }

    stralloc_free(&again) ;

    /** be able to freed the stralloc before existing */
    char tmp[modifs.len+1] ;
    memcpy(tmp,modifs.s,modifs.len) ;
    tmp[modifs.len] = 0 ;

    size_t n = env_len(envp) + 1 + byte_count(modifs.s,modifs.len,'\0') ;
    char const *newenv[n + 1] ;
    if (!env_merge (newenv, n ,envp,env_len(envp),tmp, modifs.len)) log_dieusys(LOG_EXIT_SYS,"build environment") ;

    if (!sastr_split_string_in_nline(&dst))
        log_dieusys(LOG_EXIT_SYS,"split line") ;

    pos = 0 ;

    while (pos < dst.len) {

        unexport = 0 ;
        key.len = val.len = 0 ;
        if (!stralloc_copy(&key,&dst) ||
        !stralloc_copy(&val,&dst)) log_die_nomem("stralloc") ;

        if (!environ_get_key_nclean(&key,&pos)) log_dieusys(LOG_EXIT_SYS,"get key from line: ",key.s + pos) ;
        pos-- ;// retrieve the '=' character
        if (!environ_get_val(&val,&pos)) log_dieusys(LOG_EXIT_SYS,"get value from line: ",val.s + pos) ;

        char *uval = val.s ;
        if (val.s[0] == VAR_UNEXPORT)
        {
            uval = val.s+1 ;
            unexport = 1 ;
        }

        if(sastr_cmp(&info.vars,key.s) == -1)
            if (!environ_substitute(key.s,uval,&info,newenv,unexport))
                log_dieu(LOG_EXIT_SYS,"substitute value of: ",key.s," by: ",uval) ;
    }
    stralloc_free(&key) ;
    stralloc_free(&val) ;
    stralloc_free(&dst) ;
    stralloc_free(&modifs) ;

    if (!env_string (&modifs, argv, (unsigned int) argc)) log_dieu(LOG_EXIT_SYS,"make environment string") ;
    r = el_substitute (&dst, modifs.s, modifs.len, info.vars.s, info.values.s,
        genalloc_s (elsubst_t const, &info.data),genalloc_len (elsubst_t const, &info.data)) ;
    if (r < 0) log_dieusys(LOG_EXIT_SYS,"el_substitute") ;
    else if (!r) _exit(0) ;

    stralloc_free(&modifs) ;

    char const *v[r + 1] ;
    if (!env_make (v, r ,dst.s, dst.len)) log_dieusys(LOG_EXIT_SYS,"make environment") ;
    v[r] = 0 ;

    mexec_fm (v, newenv, env_len(newenv),info.modifs.s,info.modifs.len) ;
}
