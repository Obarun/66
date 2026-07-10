/*
 * 66-svctl.c
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

/* Low-level, resolve-independent control primitive: write raw command bytes to a
 * service's supervise/control fifo. It reads nothing -- no resolve, no name lookup
 * -- it just opens <servicedir>/supervise/control and writes. This is the escape
 * hatch when the resolve database is unusable, and the primitive the system scripts
 * use; the friendly, resolve-aware path stays `66 <verb>`. Lives in libexec on
 * purpose, off the user command surface.
 *
 * Each flag maps to one byte of the 66-supervise control alphabet
 * ("abqhkti12pcyrlPCKoduDUxOQ"); flags accumulate in command-line order (clustering
 * works: -dx = down then exit). Option long names are kept identical to `66 signal`
 * (ssexec_signal.c) so the two tools read the same.
 *
 * With -w, it also waits for the service to reach the wanted state, natively, via
 * the event module (svc_send_daemon over <servicedir>/event) -- still resolve-free.
 * Without -w it is pure launch-and-forget. */

#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/types.h>
#include <oblibs/string.h>

#include <66/svc.h>
#include <66/event.h>
#include <66/constants.h>

#define DATAMAX 64

/* signal number -> control byte, restricted to the user-available signals (same
 * table as ssexec_signal.c). */
static char const cmdsig[NSIG] = {
    [SIGALRM] = 'a',
    [SIGABRT] = 'b',
    [SIGQUIT] = 'q',
    [SIGHUP] = 'h',
    [SIGKILL] = 'k',
    [SIGTERM] = 't',
    [SIGINT] = 'i',
    [SIGUSR1] = '1',
    [SIGUSR2] = '2',
    [SIGSTOP] = 'p',
    [SIGCONT] = 'c',
    [SIGWINCH] = 'y'
} ;

/* dedicated ids for options whose control byte differs from the short letter, or
 * which have no short form. Kept out of the ASCII range like OPT_ID_HELP. */
enum {
    OPT_ID_GROUP_STOP = 257,
    OPT_ID_GROUP_CONT,
    OPT_ID_GROUP_KILL,
} ;

static opt_t const opts[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",   .arg = OPT_NONE, .help = "print this help" },

    /* signals to the supervised process */
    { .id = 'a', .shortname = 'a', .longname = "alarm",     .arg = OPT_NONE, .help = "send a SIGALRM signal" },
    { .id = 'b', .shortname = 'b', .longname = "abort",     .arg = OPT_NONE, .help = "send a SIGABRT signal" },
    { .id = 'q', .shortname = 'q', .longname = "quit",      .arg = OPT_NONE, .help = "send a SIGQUIT signal" },
    { .id = 'h', .shortname = 'H', .longname = "hangup",    .arg = OPT_NONE, .help = "send a SIGHUP signal" },
    { .id = 'k', .shortname = 'k', .longname = "kill",      .arg = OPT_NONE, .help = "send a SIGKILL signal" },
    { .id = 't', .shortname = 't', .longname = "term",      .arg = OPT_NONE, .help = "send a SIGTERM signal" },
    { .id = 'i', .shortname = 'i', .longname = "interrupt", .arg = OPT_NONE, .help = "send a SIGINT signal" },
    { .id = '1', .shortname = '1', .longname = "usr1",      .arg = OPT_NONE, .help = "send a SIGUSR1 signal" },
    { .id = '2', .shortname = '2', .longname = "usr2",      .arg = OPT_NONE, .help = "send a SIGUSR2 signal" },
    { .id = 'p', .shortname = 'p', .longname = "stop",      .arg = OPT_NONE, .help = "send a SIGSTOP signal" },
    { .id = 'c', .shortname = 'c', .longname = "cont",      .arg = OPT_NONE, .help = "send a SIGCONT signal" },
    { .id = 'y', .shortname = 'y', .longname = "winch",     .arg = OPT_NONE, .help = "send a SIGWINCH signal" },
    { .id = 's', .shortname = 's', .longname = "signal",    .arg = OPT_REQUIRED, .argname = "signal", .help = "send signal to the supervised process by signal name or its number" },
    { .id = 'r', .shortname = 'r', .longname = "restart",   .arg = OPT_NONE, .help = "restart service by sending it a signal (default SIGTERM)" },
    { .id = 'l', .shortname = 'l', .longname = "reload",    .arg = OPT_NONE, .help = "reload service by sending it a signal (default SIGHUP)" },

    /* signals to the whole process group of the supervised process (long-only) */
    { .id = OPT_ID_GROUP_STOP, .shortname = 0, .longname = "stop-group", .arg = OPT_NONE, .help = "send a SIGSTOP to the process group" },
    { .id = OPT_ID_GROUP_CONT, .shortname = 0, .longname = "cont-group", .arg = OPT_NONE, .help = "send a SIGCONT to the process group" },
    { .id = OPT_ID_GROUP_KILL, .shortname = 0, .longname = "kill-group", .arg = OPT_NONE, .help = "send a SIGKILL to the process group" },

    /* state commands */
    { .id = 'o', .shortname = 'o', .longname = "once",         .arg = OPT_NONE, .help = "once. Equivalent to '-uO'" },
    { .id = 'd', .shortname = 'd', .longname = "down",         .arg = OPT_NONE, .help = "send a SIGTERM signal then a SIGCONT signal" },
    { .id = 'D', .shortname = 'D', .longname = "down-keep",    .arg = OPT_NONE, .help = "bring down service and avoid to be bring it up automatically" },
    { .id = 'u', .shortname = 'u', .longname = "up",           .arg = OPT_NONE, .help = "bring up service" },
    { .id = 'U', .shortname = 'U', .longname = "up-restart",   .arg = OPT_NONE, .help = "bring up service and ensure that service can be restarted automatically" },
    { .id = 'x', .shortname = 'x', .longname = "exit",         .arg = OPT_NONE, .help = "bring down the service and propagate to its supervisor" },
    { .id = 'O', .shortname = 'O', .longname = "once-at-most", .arg = OPT_NONE, .help = "mark the service to run once at most" },
    { .id = 'Q', .shortname = 'Q', .longname = "once-at-most-down", .arg = OPT_NONE, .help = "once at most, and bring down the service" },

    /* wait for the wanted state (resolve-free, via the event module) */
    { .id = 'w', .shortname = 'w', .longname = "wait",    .arg = OPT_REQUIRED, .argname = "uUdDrR",     .help = "do not exit until the service reaches the wanted state" },
    { .id = 'T', .shortname = 'T', .longname = "timeout", .arg = OPT_REQUIRED, .argname = "milliseconds", .help = "with -w, fail after this delay (0 = wait forever, default)" },
} ;

static opt_cmd_t const cmd = {
    .name = "66-svctl",
    .operands = "servicedir...",
    .epilog =
        "Writes raw supervise control bytes directly to servicedir/supervise/control.\n"
        "It does not read the resolve database: pass the service directory itself.\n"
        "Flags accumulate in order (e.g. -dx writes 'd' then 'x').\n"
        "It should not be used directly because it bypasses the safeguards.\n"
        "\n"
        "values for -w (do not exit until the service reaches the wanted state):\n"
        "    u  the service is up\n"
        "    U  the service is up and ready and has notified readiness\n"
        "    d  the service is down\n"
        "    D  the service is down and ready to be brought up and has notified readiness\n"
        "    r  the service has been started or restarted\n"
        "    R  the service has been started or restarted and has notified readiness",
    .opts = opts,
    .nopts = OPT_COUNT(opts),
} ;

/* map the -w letter to a wanted event state */
static event_t wait_to_event(char c)
{
    switch (c) {
        case 'u' : return EVENT_UP ;
        case 'U' : return EVENT_UP_READY ;
        case 'd' : return EVENT_DOWN ;
        case 'D' : return EVENT_DOWN_READY ;
        case 'r' : return EVENT_RESTART ;
        case 'R' : return EVENT_RESTART_READY ;
        default :  return EVENT_UP ;
    }
}

static event_t downgrade_if_no_notif(char const *dir, event_t wanted)
{
    if (wanted != EVENT_UP_READY && wanted != EVENT_DOWN_READY && wanted != EVENT_RESTART_READY)
        return wanted ;

    char fn[strlen(dir) + 1 + SS_NOTIFICATION_LEN + 1] ;
    auto_strings(fn, dir, "/", SS_NOTIFICATION) ;

    if (!access(fn, F_OK))
        return wanted ;

    log_warn(fn, " not present - ignoring request for readiness notification") ;
    if (wanted == EVENT_UP_READY) return EVENT_UP ;
    if (wanted == EVENT_DOWN_READY) return EVENT_DOWN ;
    return EVENT_RESTART ;
}

int main (int argc, char const *const *argv)
{
    char data[DATAMAX + 1] ;
    size_t dlen = 0 ;
    char wait = 0 ;
    unsigned int timeout = 0 ;
    int e = 0 ;

    PROG = "66-svctl" ;

    {
        opt_scan_t st = OPT_SCAN_ZERO ;

        for (;;)
        {
            int o = opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) ;
            if (o == OPT_END) break ;

            char byte = 0 ;

            switch (o)
            {
                case OPT_ID_HELP : return opt_emit_help(cmd.name, &cmd) ;
                case OPT_UNKNOWN :
                case OPT_MISSARG : return opt_emit_error(cmd.name, &cmd, o, &st) ;

                case 'w' :
                    if (!memchr("uUdDrR", st.arg[0], 6) || st.arg[1])
                        log_die(LOG_EXIT_USER, "invalid value for -w -- expected one of u/U/d/D/r/R") ;
                    wait = st.arg[0] ;
                    continue ;

                case 'T' :
                    if (!u32_scan_strict(st.arg, &timeout))
                        log_die(LOG_EXIT_USER, "invalid timeout: ", st.arg) ;
                    continue ;

                case 's' :
                {
                    int sig ;
                    if (!sig_parse(st.arg, &sig))
                        log_die(LOG_EXIT_USER, "invalid signal: ", st.arg) ;
                    if (sig < 0 || sig >= NSIG || !cmdsig[sig])
                        log_die(LOG_EXIT_USER, st.arg, " is not in the list of user-available signals") ;
                    byte = cmdsig[sig] ;
                    break ;
                }

                case OPT_ID_GROUP_STOP : byte = 'P' ; break ;
                case OPT_ID_GROUP_CONT : byte = 'C' ; break ;
                case OPT_ID_GROUP_KILL : byte = 'K' ; break ;

                default : byte = (char)o ; break ;
            }

            if (dlen >= DATAMAX)
                log_die(LOG_EXIT_USER, "too many commands") ;
            data[dlen++] = byte ;
        }
        argc -= st.ind ; argv += st.ind ;
    }

    if (!argc || !dlen)
        return opt_emit_usage(cmd.name, &cmd) ;

    data[dlen] = 0 ;

    for (int i = 0 ; i < argc ; i++) {

        if (wait) {
            event_t wanted = downgrade_if_no_notif(argv[i], wait_to_event(wait)) ;
            svc_send_daemon(argv[i], data, STATUS_WHO_USER, wanted, (int)timeout) ;
        }
        else if (!svc_control_send(argv[i], data, dlen, STATUS_WHO_USER))
            e = LOG_EXIT_SYS ;
    }

    return e ;
}
