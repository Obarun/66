/*
 * svc_launch.c
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
#include <stdint.h>
#include <unistd.h> // access, unlink
#include <errno.h>
#include <signal.h>
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/environ.h>

#include <skalibs/types.h>
#include <skalibs/tai.h>
#include <skalibs/selfpipe.h>
#include <skalibs/djbunix.h>
#include <skalibs/cspawn.h>
#include <skalibs/genalloc.h>
#include <skalibs/iopause.h>
#include <skalibs/sig.h>//sig_ignore

#include <66/ssexec.h>
#include <66/constants.h>
#include <66/service.h>
#include <66/enum.h>
#include <66/state.h>
#include <66/svc.h>

static unsigned int napid = 0 ;
static unsigned int npid = 0 ;

static char data[DATASIZE + 1] ;
static char updown[4] ;
static uint8_t opt_updown = 0 ;
static uint8_t reloadmsg = 0 ;
static uint8_t PROPAGATE = 1 ;
static ssexec_t_ref PINFO = 0 ;

typedef enum fifo_e fifo_t, *fifo_t_ref ;
enum fifo_e
{
    FIFO_u = 0,
    FIFO_U,
    FIFO_d,
    FIFO_D,
    FIFO_F,
    FIFO_b,
    FIFO_B
} ;

typedef enum service_action_e service_action_t, *service_action_t_ref ;
enum service_action_e
{
    SERVICE_ACTION_GOTIT = 0,
    SERVICE_ACTION_WAIT,
    SERVICE_ACTION_FATAL,
    SERVICE_ACTION_UNKNOWN
} ;

static const unsigned char actions[2][7] = {
    // u U d D F b B
    { SERVICE_ACTION_WAIT, SERVICE_ACTION_GOTIT, SERVICE_ACTION_UNKNOWN, SERVICE_ACTION_UNKNOWN, SERVICE_ACTION_FATAL, SERVICE_ACTION_WAIT, SERVICE_ACTION_WAIT }, // !what -> up
    { SERVICE_ACTION_UNKNOWN, SERVICE_ACTION_UNKNOWN, SERVICE_ACTION_WAIT, SERVICE_ACTION_GOTIT, SERVICE_ACTION_FATAL, SERVICE_ACTION_WAIT, SERVICE_ACTION_WAIT } // what -> down

} ;

//  convert signal into enum number
static const unsigned int char2enum[128] =
{
    0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , //8
    0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , //16
    0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , //24
    0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , //32
    0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , //40
    0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , //48
    0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , //56
    0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , //64
    0 ,  0 ,  FIFO_B ,  0 ,  FIFO_D ,  0 ,  FIFO_F ,  0 , //72
    0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , //80
    0 ,  0 ,  0 ,  0 ,  0 ,  FIFO_U,   0 ,  0 , //88
    0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , //96
    0 ,  0 ,  FIFO_b ,  0 ,  FIFO_d ,  0 ,  0 ,  0 , //104
    0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , //112
    0 ,  0 ,  0 ,  0 ,  0 ,  FIFO_u ,  0 ,  0 , //120
    0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0   //128
} ;

static inline void kill_all(pidservice_t *apids)
{
    log_flow() ;

    unsigned int j = napid ;
    while (j--) kill(apids[j].pid, SIGKILL) ;
}

static int check_action(pidservice_t *apids, unsigned int pos, unsigned int receive, unsigned int what)
{
    unsigned int p = char2enum[receive] ;
    unsigned char action = actions[what][p] ;

    switch(action) {

        case SERVICE_ACTION_GOTIT:
            FLAGS_SET(apids[pos].state, (!what ? SVC_FLAGS_UP : SVC_FLAGS_DOWN)) ;
            return 1 ;

        case SERVICE_ACTION_FATAL:
            FLAGS_SET(apids[pos].state, SVC_FLAGS_FATAL) ;
            return -1 ;

        case SERVICE_ACTION_WAIT:
            return 0 ;

        case SERVICE_ACTION_UNKNOWN:
        default:
            log_die(LOG_EXIT_ZERO,"invalid action -- please make a bug report") ;
    }

}

static void notify(pidservice_t *apids, unsigned int pos, char const *sig, unsigned int what)
{
    log_flow() ;

    unsigned int i = 0, idx = 0 ;
    char fmt[UINT_FMT] ;
    uint8_t flag = what ? SVC_FLAGS_DOWN : SVC_FLAGS_UP ;

    for (; i < apids[pos].nnotif ; i++) {

        for (idx = 0 ; idx < napid ; idx++) {

            if (apids[pos].notif[i] == apids[idx].vertex && !FLAGS_ISSET(apids[idx].state, flag))  {

                size_t nlen = uint_fmt(fmt, pos) ;
                fmt[nlen] = 0 ;
                size_t len = nlen + 1 + 2 ;
                char s[len + 1] ;
                auto_strings(s, fmt, ":", sig, "@") ;

                log_trace("sends notification ", sig, " to: ", apids[idx].res->sa.s + apids[idx].res->name, " from: ", apids[pos].res->sa.s + apids[pos].res->name) ;

                if (write(apids[idx].pipe[1], s, strlen(s)) < 0)
                    log_dieusys(LOG_EXIT_SYS, "send notif to: ", apids[idx].res->sa.s + apids[idx].res->name) ;
            }
        }
    }
}

/**
 * @what: up or down
 * @success: 0 fail, 1 win
 * */
static void announce(unsigned int pos, pidservice_t *apids, unsigned int what, unsigned int success, unsigned int exitcode)
{
    log_flow() ;

    int fd = 0 ;
    char fmt[UINT_FMT] ;
    char const *name = apids[pos].res->sa.s + apids[pos].res->name ;
    char const *scandir = apids[pos].res->sa.s + apids[pos].res->live.scandir ;
    size_t scandirlen = strlen(scandir) ;
    char file[scandirlen +  6] ;

    auto_strings(file, scandir, "/down") ;

    uint8_t flag = what ? SVC_FLAGS_DOWN : SVC_FLAGS_UP ;

    if (success) {

        if (apids[pos].res->type == TYPE_CLASSIC) {

            fd = open_trunc(file) ;
            if (fd < 0)
                log_dieusys(LOG_EXIT_SYS, "create file: ", scandir) ;
            fd_close(fd) ;
        }

        fmt[uint_fmt(fmt, exitcode)] = 0 ;

        log_1_warn("unable to ", reloadmsg == 1 ? "restart" : reloadmsg > 1 ? "reload" : what ? "stop" : "start", " service: ", name, " -- exited with signal: ", fmt) ;

        notify(apids, pos, "F", what) ;

        FLAGS_SET(apids[pos].state, SVC_FLAGS_BLOCK|SVC_FLAGS_FATAL) ;

    } else {

        if (!state_messenger(apids[pos].res, STATE_FLAGS_ISUP, \
                                                        data[1] == 'a' || \
                                                        data[1] == 'h' || \
                                                        data[1] == 'U' || \
                                                        data[1] == 'r' \
                                                        ? STATE_FLAGS_TRUE : what ? STATE_FLAGS_FALSE : STATE_FLAGS_TRUE))
            log_dieu(LOG_EXIT_SYS, "send message to state of: ", name) ;

        if (!apids[pos].res->execute.down && apids[pos].res->type == TYPE_CLASSIC) {

            if (!what) {

                if (!access(scandir, F_OK)) {
                    log_trace("delete down file: ", file) ;
                    if (unlink(file) < 0 && errno != ENOENT)
                        log_warnusys("delete down file: ", file) ;
                }

            } else {

                fd = open_trunc(file) ;
                /** The directory and file may not exist. Typically,
                 * a service inside a module can not match the state
                 * of the module and may occur in case of crash of a
                 * previous user command invocation.
                 * Where are on stop process, so do not crash
                 * for a corrupted service.
                 * At start process, the live directory will be made
                 * from scratch anyway.*/
                if (fd < 0 && errno != ENOENT)
                    log_dieusys(LOG_EXIT_SYS, "create file: ", file) ;
                fd_close(fd) ;
            }
        }

        log_info("Successfully ", reloadmsg == 1 ? "restarted" : reloadmsg > 1 ? "reloaded" : what ? "stopped" : "started", " service: ", name) ;

        notify(apids, pos, what ? "D" : "U", what) ;

        FLAGS_CLEAR(apids[pos].state, SVC_FLAGS_BLOCK) ;
        FLAGS_SET(apids[pos].state, flag|SVC_FLAGS_UNBLOCK) ;
    }
}

static int handle_signal(pidservice_t *apids, unsigned int what, graph_t *graph, ssexec_t *info)
{
    log_flow() ;

    int ok = 0 ;

    for (;;) {

        int s = selfpipe_read() ;
        switch (s) {

            case -1 : log_dieusys(LOG_EXIT_SYS,"selfpipe_read") ;
            case 0 : return ok ;
            case SIGCHLD :

                for (;;) {

                    unsigned int pos = 0 ;
                    int wstat = 0 ;
                    pid_t r = wait_nohang(&wstat) ;

                    if (r < 0) {

                        if (errno == ECHILD)
                            break ;
                        else
                            log_dieusys(LOG_EXIT_SYS,"wait for children") ;

                    } else if (!r) break ;

                    for (; pos < napid ; pos++)
                        if (apids[pos].pid == r)
                            break ;

                    if (pos < napid) {

                        if (!WIFSIGNALED(wstat) && !WEXITSTATUS(wstat)) {

                            announce(pos, apids, what, 0, 0) ;
                            npid-- ;

                        } else {

                            ok = WIFSIGNALED(wstat) ? WTERMSIG(wstat) : WEXITSTATUS(wstat) ;
                            announce(pos, apids, what, 1, ok) ;
                            npid-- ;
                            kill_all(apids) ;
                            break ;
                        }
                    }
                }
                break ;
            case SIGTERM :
            case SIGKILL :
            case SIGINT :
                    log_1_warn("received SIGINT, aborting transaction") ;
                    kill_all(apids) ;
                    ok = 111 ;
                    break ;
            default : log_die(LOG_EXIT_SYS, "unexpected data in selfpipe") ;
        }
    }

    return ok ;
}

unsigned int compute_timeout(resolve_service_t *res, unsigned int what)
{
    unsigned int timeout = 0 ;

    if (PINFO->opt_timeout) {

        timeout = PINFO->timeout ;

    } else if (!what) {

        if (res->execute.timeout.start)
            timeout = res->execute.timeout.start ;

    } else {

        if (res->execute.timeout.stop)
            timeout = res->execute.timeout.stop ;
    }

    return timeout ;

}

static int doit(pidservice_t *apids, unsigned int napid, unsigned int idx, unsigned int what, tain *deadline)
{
    log_flow() ;

    uint8_t type = apids[idx].res->type ;

    pid_t pid ;
    int wstat ;

    char tfmt[UINT32_FMT] ;

    unsigned int timeout = 0 ;

    timeout = compute_timeout(apids[idx].res, what) ;

    tfmt[uint_fmt(tfmt, timeout)] = 0 ;

    if (type == TYPE_CLASSIC) {

        char *scandir = apids[idx].res->sa.s + apids[idx].res->live.scandir ;

        if (updown[2] == 'U' || updown[2] == 'D' || updown[2] == 'R') {

            if (!apids[idx].res->notify)
                updown[2] = updown[2] == 'U' ? 'u' : updown[2] == 'D' ? 'd' : updown[2] == 'R' ? 'r' : updown[2] ;

        }

        char const *newargv[8] ;
        unsigned int m = 0 ;

        newargv[m++] = "s6-svc" ;
        newargv[m++] = data ;

        if (opt_updown)
            newargv[m++] = updown ;

        newargv[m++] = "-T" ;
        newargv[m++] = tfmt ;
        newargv[m++] = "--" ;
        newargv[m++] = scandir ;
        newargv[m++] = 0 ;

        log_trace("sending ", opt_updown ? newargv[2] : "", opt_updown ? " " : "", data, " to: ", scandir) ;

        pid = child_spawn0(newargv[0], newargv, (char const *const *) environ) ;

        if (waitpid_nointr(pid, &wstat, 0) < 0)
            log_warnusys_return(LOG_EXIT_ZERO, "wait for s6-svc") ;

        if (!WIFSIGNALED(wstat) && !WEXITSTATUS(wstat))
            return WEXITSTATUS(wstat) ;
        else
            return WIFSIGNALED(wstat) ? WTERMSIG(wstat) : WEXITSTATUS(wstat) ;

    } else if (type == TYPE_ONESHOT) {

        char *servicedir = apids[idx].res->sa.s + apids[idx].res->live.servicedir ;
        char *oneshotdir = apids[idx].res->sa.s + apids[idx].res->live.oneshotddir ;
        char *scandir = apids[idx].res->sa.s + apids[idx].res->live.scandir ;
        char oneshot[strlen(oneshotdir) + 2 + 1] ;
        auto_strings(oneshot, oneshotdir, "/s") ;

        char const *newargv[11] ;
        unsigned int m = 0 ;
        newargv[m++] = "s6-sudo" ;
        newargv[m++] = VERBOSITY >= 4 ? "-vel0" : "-el0" ;
        newargv[m++] = "-t" ;
        newargv[m++] = "30000" ;
        newargv[m++] = "-T" ;
        newargv[m++] = tfmt ;
        newargv[m++] = "--" ;
        newargv[m++] = oneshot ;
        newargv[m++] = !what ? "up" : "down" ;
        newargv[m++] = servicedir ;
        newargv[m++] = 0 ;

        log_trace("sending ", !what ? "start" : "stop", " to: ", scandir) ;

        pid = child_spawn0(newargv[0], newargv, (char const *const *) environ) ;

        if (waitpid_nointr(pid, &wstat, 0) < 0)
            log_warnusys_return(LOG_EXIT_ZERO, "wait for s6-sudo") ;

        if (!WIFSIGNALED(wstat) && !WEXITSTATUS(wstat)) {

            if (data[1] == 'r') {
                /** oneshot service are not handled automatically by
                 * s6-supervise. Signal is restart, so let it down first
                 * and force to bring it up again .*/

                log_trace("sending up to: ", scandir) ;

                char const *newargv[11] ;
                unsigned int m = 0 ;
                newargv[m++] = "s6-sudo" ;
                newargv[m++] = VERBOSITY >= 4 ? "-vel0" : "-el0" ;
                newargv[m++] = "-t" ;
                newargv[m++] = "30000" ;
                newargv[m++] = "-T" ;
                newargv[m++] = tfmt ;
                newargv[m++] = "--" ;
                newargv[m++] = oneshot ;
                newargv[m++] = "up" ;
                newargv[m++] = servicedir ;
                newargv[m++] = 0 ;

                pid = child_spawn0(newargv[0], newargv, (char const *const *) environ) ;

                if (waitpid_nointr(pid, &wstat, 0) < 0)
                    log_warnusys_return(LOG_EXIT_ZERO, "wait for s6-sudo") ;

                if (WIFSIGNALED(wstat) && WEXITSTATUS(wstat))
                    return WIFSIGNALED(wstat) ? WTERMSIG(wstat) : WEXITSTATUS(wstat) ;
            }

            return WEXITSTATUS(wstat) ;

        } else {

            return WIFSIGNALED(wstat) ? WTERMSIG(wstat) : WEXITSTATUS(wstat) ;
        }

    } else if (type == TYPE_MODULE) {

        return svc_compute_ns(apids[idx].res, what, PINFO, updown, opt_updown, reloadmsg, data, PROPAGATE, apids, napid) ;
    }

    /* should be never reached*/
    return 0 ;
}

static int async_deps(struct resolve_hash_s **hres, pidservice_t *apids, unsigned int i, unsigned int what, ssexec_t *info, tain *deadline)
{
    log_flow() ;

    int r ;
    unsigned int pos = 0, id = 0, idx = 0 ;
    char buf[(UINT_FMT*2)*SS_MAX_SERVICE + 1] ;

    tain dead ;
    tain_now_set_stopwatch_g() ;
    tain_add_g(&dead, deadline) ;

    iopause_fd x = { .fd = apids[i].pipe[0], .events = IOPAUSE_READ, 0 } ;

    unsigned int n = apids[i].nedge ;
    unsigned int visit[n + 1] ;

    memset(visit, 0, (n + 1) * sizeof(unsigned int));

    log_trace("waiting dependencies for: ", apids[i].res->sa.s + apids[i].res->name) ;

    while (pos < n) {

        r = iopause_g(&x, 1, &dead) ;

        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "iopause") ;

        if (!r) {
            errno = ETIMEDOUT ;
            log_dieusys(LOG_EXIT_SYS,"timed out", apids[i].res->sa.s + apids[i].res->name) ;
        }

        if (x.revents & IOPAUSE_READ) {

            memset(buf, 0, ((UINT_FMT*2)*SS_MAX_SERVICE + 1) * sizeof(char)) ;
            r = read(apids[i].pipe[0], buf, sizeof(buf)) ;
            if (r < 0)
                log_dieu(LOG_EXIT_SYS, "read from pipe") ;
            buf[r] = 0 ;

            idx = 0 ;

            while (r != -1) {
                /** The buf might contain multiple signal coming
                 * from the dependencies if they finished before
                 * the start of this read process. Check every
                 * signal received.*/
                r = get_len_until(buf + idx, '@') ;

                if (r < 0)
                    /* no more signal */
                    goto next ;

                char line[r + 1] ;
                memcpy(line, buf + idx, r) ;
                line[r] = 0 ;

                idx += r + 1 ;

                /**
                 * the received string have the format:
                 *      apids_array_id:signal_receive
                 *
                 * typically:
                 *      - 10:D
                 *      - 8:u
                 *      - ...
                 *
                 * Split it and check the signal receive.*/
                int sep = get_len_until(line, ':') ;
                if (sep < 0)
                    log_die(LOG_EXIT_SYS, "received bad signal format -- please make a bug report") ;

                unsigned int c = line[sep + 1] ;
                char pc[2] = { c, 0 } ;
                line[sep] = 0 ;

                if (!uint0_scan(line, &id))
                    log_dieusys(LOG_EXIT_SYS, "retrieve service number -- please make a bug report") ;

                log_trace(apids[i].res->sa.s + apids[i].res->name, " acknowledges: ", pc, " from: ", apids[id].res->sa.s + apids[id].res->name) ;

                if (!visit[pos]) {

                    id = check_action(apids, id, c, what) ;
                    if (id < 0)
                        log_die(LOG_EXIT_SYS, "service dependency: ", apids[id].res->sa.s + apids[id].res->name, " of: ", apids[i].res->sa.s + apids[i].res->name," crashed") ;

                    if (!id)
                        continue ;

                    visit[pos++] ;
                }
            }
        }
        next:

    }

    return 1 ;
}

static int async(struct resolve_hash_s **hres, pidservice_t *apids, unsigned int napid, unsigned int i, unsigned int what, ssexec_t *info, graph_t *graph, tain *deadline)
{
    log_flow() ;

    int e = 0 ;

    char *name = graph->data.s + genalloc_s(graph_hash_t,&graph->hash)[apids[i].vertex].vertex ;

    log_trace("Initiating process of: ", name) ;

    if (FLAGS_ISSET(apids[i].state, (!what ? SVC_FLAGS_DOWN : SVC_FLAGS_UP))) {

        if (!FLAGS_ISSET(apids[i].state, SVC_FLAGS_BLOCK)) {

            FLAGS_SET(apids[i].state, SVC_FLAGS_BLOCK) ;

            if (apids[i].nedge)
                if (!async_deps(hres, apids, i, what, info, deadline))
                    log_warnu_return(LOG_EXIT_SYS, !what ? "start" : "stop", " dependencies of service: ", name) ;

            e = doit(apids, napid, i, what, deadline) ;

        } else {

            log_warn("skipping service: ", name, " -- already in ", what ? "stopping" : "starting", " process") ;
            notify(apids, i, what ? "d" : "u", what) ;
        }

    } else {

        /** do not notify here, the handle will make it for us */
        log_warn("skipping service: ", name, " -- already ", what ? "down" : "up") ;

    }

    return e ;
}

int svc_launch(pidservice_t *apids, unsigned int len, uint8_t what, graph_t *graph, struct resolve_hash_s **hres, ssexec_t *info, char const *rise, uint8_t rise_opt, uint8_t msg, char const *signal, uint8_t propagate)
{
    log_flow() ;

    unsigned int e = 0, pos = 0 ;
    int r ;
    pid_t pid ;
    pidservice_t apidservicetable[len] ;
    pidservice_t_ref apidservice = apidservicetable ;
    tain deadline ;

    npid = 0 ;
    PINFO = info ;
    PROPAGATE = propagate ;
    napid = len ;
    auto_strings(updown, rise) ;
    opt_updown = rise_opt ;
    reloadmsg = msg ;
    auto_strings(data, signal) ;

    if (info->opt_timeout)
        tain_from_millisecs(&deadline, info->timeout) ;
    else
        deadline = tain_infinite_relative ;

    int spfd = selfpipe_init() ;

    if (spfd < 0)
        log_dieusys(LOG_EXIT_SYS, "selfpipe_init") ;

    if (!selfpipe_trap(SIGCHLD) ||
        !selfpipe_trap(SIGINT) ||
        !selfpipe_trap(SIGKILL) ||
        !selfpipe_trap(SIGTERM) ||
        !sig_altignore(SIGPIPE))
            log_dieusys(LOG_EXIT_SYS, "selfpipe_trap") ;

    iopause_fd x = { .fd = spfd, .events = IOPAUSE_READ, .revents = 0 } ;

    for (; pos < napid ; pos++) {

        apidservice[pos] = apids[pos] ;

        if (pipe(apidservice[pos].pipe) < 0)
            log_dieusys(LOG_EXIT_SYS, "pipe");

    }

    tain_now_set_stopwatch_g() ;
    tain_add_g(&deadline, &deadline) ;

    for (pos = 0 ; pos < napid ; pos++) {

        pid = fork() ;

        if (pid < 0)
            log_dieusys(LOG_EXIT_SYS, "fork") ;

        if (!pid) {

            selfpipe_finish() ;

            close(apidservice[pos].pipe[1]) ;

            e = async(hres, apidservice, napid, pos, what, info, graph, &deadline) ;

            goto end ;
        }

        apidservice[pos].pid = pid ;

        close(apidservice[pos].pipe[0]) ;

        npid++ ;
    }

    while (npid) {

        r = iopause_g(&x, 1, &deadline) ;

        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "iopause") ;

        if (!r) {
            errno = ETIMEDOUT ;
            log_diesys(LOG_EXIT_SYS,"time out") ;
        }

        if (x.revents & IOPAUSE_READ) {
            e = handle_signal(apidservice, what, graph, info) ;

            if (e)
                break ;
        }
    }

    selfpipe_finish() ;

    for (pos = 0 ; pos < napid ; pos++) {
        close(apidservice[pos].pipe[1]) ;
        close(apidservice[pos].pipe[0]) ;
    }

    end:
        return e ;
}
