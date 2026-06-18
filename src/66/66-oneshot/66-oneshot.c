/*
 * 66-oneshot.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 * */

#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdint.h>
#include <sys/wait.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/string.h>
#include <oblibs/types.h>

#include <66/oneshot.h>
#include <66/constants.h>

static opt_t const opts[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",    .arg = OPT_NONE,                          .help = "print this help" },
    { .id = 'v',         .shortname = 'v', .longname = "verbose", .arg = OPT_REQUIRED, .argname = "number", .help = "increase/decrease verbosity" },
} ;

static opt_cmd_t const cmd = {
    .name = "66-oneshot",
    .operands = "socket up|down /run/66/state/<uid>/<service>",
    .opts = opts,
    .nopts = OPT_COUNT(opts),
} ;

int main(int argc, char const *const *argv)
{
    PROG = "66-oneshot" ;
    {
        opt_scan_t st = OPT_SCAN_ZERO ;

        for (;;)
        {
            int o = opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) ;
            if (o == OPT_END) break ;

            switch (o) {
                case OPT_ID_HELP : return opt_emit_help(cmd.name, &cmd) ;
                case 'v' :
                    if (!u32_scan_strict(st.arg, &VERBOSITY))
                        return opt_emit_usage(cmd.name, &cmd) ;
                    break ;
                default : return opt_emit_error(cmd.name, &cmd, o, &st) ;
            }
        }
        argc -= st.ind ; argv += st.ind ;
    }

    if (argc < 3)
        return opt_emit_usage(cmd.name, &cmd) ;

    char const *socket = argv[0] ;
    char const *op = argv[1] ;
    char const *servicedir = argv[2] ;

    if (op[0] != 'd' && op[0] != 'u')
        log_die(LOG_EXIT_USER, "only up or down signals are allowed") ;

    uint8_t down = op[0] == 'd' ;

    size_t len = strlen(servicedir) ;

    if (len >= SS_MAX_PATH_LEN)
        flog_die(LOG_EXIT_USER, "service directory is too long -- it cannot exceed: %zu", (size_t)SS_MAX_PATH_LEN) ;

    if (servicedir[0] != '/')
        log_die(LOG_EXIT_USER, "service directory must be an absolute path: ", servicedir) ;

    char script[len + 1 + 6 + 1] ;
    auto_strings(script, servicedir, "/", down ? "finish" : "run") ;

    if (access(script, F_OK) < 0) {
        /* a oneshot with no down script has nothing to bring down */
        if (down)
            _exit(0) ;
        log_dieusys(LOG_EXIT_SYS, "find: ", script) ;
    }

    oneshot_client_t c ;
    if (!oneshot_client_init(&c, socket))
        log_dieu(LOG_EXIT_SYS, "connect to oneshot daemon: ", socket) ;

    /* no client-side guard timer: the start/stop timeout is owned by the
     * service manager, which kills us on expiry, which the daemon turns into
     * killing the running script */
    int got = oneshot_run(&c, down, servicedir, 0) ;
    uint8_t status = c.status ;
    uint32_t wstat = c.wstat ;

    oneshot_client_end(&c) ;

    if (!got || status != ONESHOT_OK)
        log_die(LOG_EXIT_SYS, "run ", down ? "down" : "up", " script of: ", servicedir, status != ONESHOT_OK ? " -- " : "", status != ONESHOT_OK ? oneshot_status_str(status) : "") ;

    /* mirror the script's own termination so the manager's child watcher reads
     * the genuine status (4-byte wstat is preserved end to end) */
    if (WIFSIGNALED(wstat)) {
        int sig = WTERMSIG(wstat) ;
        signal(sig, SIG_DFL) ; // be sure that the signal is not ignored
        raise(sig) ; // send it signal to itself by kill it.
        // never reached
        _exit(128 + sig) ;
    }

    _exit(WEXITSTATUS(wstat)) ;
}
