/*
 * execl-runas.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
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
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <limits.h>

#include <oblibs/log.h>
#include <oblibs/exec.h>
#include <oblibs/opt.h>

static opt_t const opts[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help", .arg = OPT_NONE, .help = "print this help" },
} ;

static opt_cmd_t const cmd = {
    .name = "execl-runas",
    .operands = "account prog...",
    .opts = opts,
    .nopts = OPT_COUNT(opts),
} ;

static int readgroups(char const *name, gid_t *tab, unsigned int max)
{
    unsigned int n = 0 ;
    for (;;)
    {
        struct group *gr ;
        char **member ;
        errno = 0 ;
        if (n >= max)
            break ;
        gr = getgrent() ;
        if (!gr)
            break ;

        for (member = gr->gr_mem ; *member ; member++) {
            if (!strcmp(name, *member))
                 break ;
        }

        if (*member) tab[n++] = gr->gr_gid ;
    }

    endgrent() ;

    return errno ? -1 : (int)n ;
}

static int setgroups_and_gid(gid_t g, size_t n, gid_t const *tab)
{
    size_t i = 1 ;
    if (!n)
        return setgroups(1, &g) ;

    if (tab[0] == g)
        return setgroups(n, tab) ;

    for (; i < n ; i++) {
        if (tab[i] == g)
            break ;
    }

    if (i < n) {

        gid_t newtab[n] ;
        newtab[0] = g ;
        memcpy(newtab + 1, tab, i * sizeof(gid_t)) ;
        memcpy(newtab + i + 1, tab + i + 1, (n - i - 1) * sizeof(gid_t)) ;
        return setgroups(n, newtab) ;

    }

    gid_t newtab[n + 1] ;
    newtab[0] = g ;
    memcpy(newtab + 1, tab, n * sizeof(gid_t)) ;

    return setgroups(n + 1, newtab) ;
}

int main (int argc, char const *const *argv, char const *const *envp)
{
    char const *account ;
    struct passwd *pw ;
    uid_t uid ;
    gid_t gid, tab[NGROUPS_MAX] ;
    int n ;

    PROG = "execl-runas" ;

    {
        opt_scan_t st = OPT_SCAN_ZERO ;

        for (;;)
        {
            int o = opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) ;
            if (o == OPT_END) break ;
            switch (o)
            {
                case OPT_ID_HELP :  return opt_emit_help(cmd.name, &cmd) ;
                default :           return opt_emit_error(cmd.name, &cmd, o, &st) ;
            }
        }
        argc -= st.ind ; argv += st.ind ;
    }

    if (argc < 2) return opt_emit_usage(cmd.name, &cmd) ;

    account = argv[0] ;

    errno = 0 ;
    pw = getpwnam(account) ;
    if (!pw) {
        if (!errno) errno = ESRCH ;
        log_dieusys(LOG_EXIT_SYS, "get account: ", account) ;
    }

    uid = pw->pw_uid ;
    gid = pw->pw_gid ;

    n = readgroups(account, tab, NGROUPS_MAX) ;
    if (n < 0)
        log_dieusys(LOG_EXIT_SYS, "get supplementary groups for: ", account) ;

    if (setgroups_and_gid(gid, (size_t)n, tab) < 0)
        log_dieusys(LOG_EXIT_SYS, "set supplementary group list") ;

    if (setgid(gid) < 0)
        log_dieusys(LOG_EXIT_SYS, "setgid") ;

    if (setuid(uid) < 0)
        log_dieusys(LOG_EXIT_SYS, "setuid") ;

    argv++ ;

    exec_path_die(argv[0], argv, envp) ;
}
