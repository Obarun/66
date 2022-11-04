/*
 * ssexec_svctl.c
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
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>//access
#include <stdlib.h>//malloc, free

#include <oblibs/obgetopt.h>
#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/string.h>
#include <oblibs/directory.h>
#include <oblibs/graph.h>
#include <oblibs/sastr.h>

#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/bytestr.h>
#include <skalibs/selfpipe.h>
#include <skalibs/iopause.h>
#include <skalibs/tai.h>
#include <skalibs/types.h>
#include <skalibs/sig.h>//sig_ignore

#include <s6/supervise.h>//s6_svstatus_t
#include <s6/ftrigr.h>
#include <s6/ftrigw.h>

#include <66/utils.h>
#include <66/constants.h>
#include <66/svc.h>
#include <66/ssexec.h>
#include <66/resolve.h>
#include <66/state.h>
#include <66/service.h>
#include <66/enum.h>
#include <66/graph.h>

#define FLAGS_STARTING 1 // 1 starting not really up
#define FLAGS_STOPPING (1 << 1) // 2 stopping not really down
#define FLAGS_UP (1 << 2) // 4 really up
#define FLAGS_DOWN (1 << 3) // 8 really down
#define FLAGS_BLOCK (1 << 4) // 16 all deps are not up/down
#define FLAGS_UNBLOCK (1 << 5) // 32 all deps are up/down
#define FLAGS_FATAL (1 << 6) // 64 process crashed

#define DATASIZE 63

static unsigned int napid = 0 ;
static unsigned int npid = 0 ;

static resolve_service_t_ref pares = 0 ;
static unsigned int *pareslen = 0 ;
static char updown[4] = "-w \0" ;
static uint8_t opt_updown = 0 ;
static char data[DATASIZE + 1] = "-" ;
static unsigned int datalen = 1 ;
static uint8_t reloadmsg = 0 ;

typedef struct pidservice_s pidservice_t, *pidservice_t_ref ;
struct pidservice_s
{
    int pipe[2] ;
    pid_t pid ;
    int aresid ; // id at array ares
    unsigned int vertex ; // id at graph_hash_t struct
    uint8_t state ;
    int nedge ;
    unsigned int edge[SS_MAX_SERVICE + 1] ; // array of id at graph_hash_t struct
    int nnotif ;
    /** id at graph_hash_t struct of depends/requiredby service
     * to notify when a service is started/stopped */
    unsigned int notif[SS_MAX_SERVICE + 1] ;
} ;
#define PIDSERVICE_ZERO { { -1, -1 }, -1, -1, 0, 0, 0, { 0 } }

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

static pidservice_t pidservice_init(unsigned int len)
{
    log_flow() ;

    pidservice_t pids = PIDSERVICE_ZERO ;

    if (len > SS_MAX_SERVICE)
        log_die(LOG_EXIT_SYS, "too many services") ;

    graph_array_init_single(pids.edge, len) ;

    return pids ;
}

static int pidservice_get_id(pidservice_t *apids, unsigned int id)
{
    log_flow() ;

    unsigned int pos = 0 ;

    for (; pos < napid ; pos++) {
        if (apids[pos].vertex == id)
            return (unsigned int) pos ;
    }
    return -1 ;
}

void notify(pidservice_t *apids, unsigned int pos, char const *sig, unsigned int what)
{
    log_flow() ;

    unsigned int i = 0, idx = 0 ;
    char fmt[UINT_FMT] ;
    uint8_t flag = what ? FLAGS_DOWN : FLAGS_UP ;

    for (; i < apids[pos].nnotif ; i++) {

        for (idx = 0 ; idx < napid ; idx++) {

            if (apids[pos].notif[i] == apids[idx].vertex && !FLAGS_ISSET(apids[idx].state, flag))  {

                size_t nlen = uint_fmt(fmt, apids[pos].aresid) ;
                fmt[nlen] = 0 ;
                size_t len = nlen + 1 + 2 ;
                char s[len + 1] ;
                auto_strings(s, fmt, ":", sig, "@") ;

                log_trace("sends notification ", sig, " to: ", pares[apids[idx].aresid].sa.s + pares[apids[idx].aresid].name, " from: ", pares[apids[pos].aresid].sa.s + pares[apids[pos].aresid].name) ;

                if (write(apids[idx].pipe[1], s, strlen(s)) < 0)
                    log_dieusys(LOG_EXIT_SYS, "send notif to: ", pares[apids[idx].aresid].sa.s + pares[apids[idx].aresid].name) ;
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
    char const *name = pares[apids[pos].aresid].sa.s + pares[apids[pos].aresid].name ;
    char const *base = pares[apids[pos].aresid].sa.s + pares[apids[pos].aresid].path.home ;
    char const *scandir = pares[apids[pos].aresid].sa.s + pares[apids[pos].aresid].live.scandir ;
    size_t scandirlen = strlen(scandir) ;
    char file[scandirlen +  6] ;

    auto_strings(file, scandir, "/down") ;

    uint8_t flag = what ? FLAGS_DOWN : FLAGS_UP ;

    if (success) {

        if (pares[apids[pos].aresid].type == TYPE_CLASSIC) {

            fd = open_trunc(file) ;
            if (fd < 0)
                log_dieusys(LOG_EXIT_SYS, "create file: ", scandir) ;
            fd_close(fd) ;
        }

        notify(apids, pos, "F", what) ;

        fmt[uint_fmt(fmt, exitcode)] = 0 ;

        log_1_warn("Unable to ", reloadmsg ? "reload" : what ? "stop" : "start", " service: ", name, " -- exited with signal: ", fmt) ;

        FLAGS_SET(apids[pos].state, FLAGS_BLOCK|FLAGS_FATAL) ;

    } else {

        if (!state_messenger(base, name, STATE_FLAGS_ISUP, what ? STATE_FLAGS_FALSE : STATE_FLAGS_TRUE))
            log_dieusys(LOG_EXIT_SYS, "send message to state of: ", name) ;

        if (!pares[apids[pos].aresid].execute.down && pares[apids[pos].aresid].type == TYPE_CLASSIC) {

            if (!what) {

                if (!access(scandir, F_OK)) {
                    log_trace("delete down file: ", file) ;
                    if (unlink(file) < 0 && errno != ENOENT)
                        log_warnusys("delete down file: ", file) ;
                }

            } else {

                fd = open_trunc(file) ;
                if (fd < 0)
                    log_dieusys(LOG_EXIT_SYS, "create file: ", file) ;
                fd_close(fd) ;
            }
        }

        notify(apids, pos, what ? "D" : "U", what) ;

        FLAGS_CLEAR(apids[pos].state, FLAGS_BLOCK) ;
        FLAGS_SET(apids[pos].state, flag|FLAGS_UNBLOCK) ;

        log_info("Successfully ", reloadmsg ? "reloaded" : what ? "stopped" : "started", " service: ", name) ;
    }
}

static void pidservice_init_array(unsigned int *list, unsigned int listlen, pidservice_t *apids, graph_t *g, resolve_service_t *ares, unsigned int areslen, ssexec_t *info, uint8_t requiredby, uint32_t flag) {

    log_flow() ;

    int r = 0 ;
    unsigned int pos = 0 ;

    for (; pos < listlen ; pos++) {

        pidservice_t pids = pidservice_init(g->mlen) ;

        char *name = g->data.s + genalloc_s(graph_hash_t,&g->hash)[list[pos]].vertex ;

        pids.aresid = service_resolve_array_search(ares, areslen, name) ;

        if (pids.aresid < 0)
            log_dieu(LOG_EXIT_SYS,"find ares id of: ", name, " -- please make a bug reports") ;

        if (FLAGS_ISSET(flag, STATE_FLAGS_TOPROPAGATE)) {

            pids.nedge = graph_matrix_get_edge_g_sorted_list(pids.edge, g, name, requiredby, 1) ;

            if (pids.nedge < 0)
                log_dieu(LOG_EXIT_SYS,"get sorted ", requiredby ? "required by" : "dependency", " list of service: ", name) ;

            pids.nnotif = graph_matrix_get_edge_g_sorted_list(pids.notif, g, name, !requiredby, 1) ;

            if (pids.nnotif < 0)
                log_dieu(LOG_EXIT_SYS,"get sorted ", !requiredby ? "required by" : "dependency", " list of service: ", name) ;

        }

        pids.vertex = graph_hash_vertex_get_id(g, name) ;

        if (pids.vertex < 0)
            log_dieu(LOG_EXIT_SYS, "get vertex id -- please make a bug report") ;

        if (ares[pids.aresid].type == TYPE_ONESHOT) {

                ss_state_t ste = STATE_ZERO ;

                if (!state_read(&ste, ares[pids.aresid].sa.s + ares[pids.aresid].path.home, name))
                    log_dieusys(LOG_EXIT_SYS, "read state file of: ", name) ;

                if (ste.isup == STATE_FLAGS_TRUE)
                    FLAGS_SET(pids.state, FLAGS_UP) ;
                else
                    FLAGS_SET(pids.state, FLAGS_DOWN) ;

        } else {

            s6_svstatus_t status ;

            r = s6_svstatus_read(ares[pids.aresid].sa.s + ares[pids.aresid].live.scandir, &status) ;

            pid_t pid = !r ? 0 : status.pid ;

            if (pid > 0) {

                FLAGS_SET(pids.state, FLAGS_UP) ;
            }
            else
                FLAGS_SET(pids.state, FLAGS_DOWN) ;
        }

        apids[pos] = pids ;
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
                    int wstat ;
                    pid_t r = wait_nohang(&wstat) ;

                    if (r < 0) {

                        if (errno = ECHILD)
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

                        } else {

                            ok = WIFSIGNALED(wstat) ? WTERMSIG(wstat) : WEXITSTATUS(wstat) ;
                            announce(pos, apids, what, 1, ok) ;

                            kill_all(apids) ;
                            break ;
                        }

                        npid-- ;
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

static int doit(pidservice_t *sv, unsigned int what, unsigned int deadline)
{
    log_flow() ;

    uint8_t type = pares[sv->aresid].type ;

    pid_t pid ;
    int wstat ;

    if (type == TYPE_MODULE || type == TYPE_BUNDLE)
        /**
         * Those type are not real services. Passing here with
         * this kind of service means that the dependencies
         * of the service was passed anyway. So, we can consider it as
         * already up/down.
         * */
         return 0 ;

    if (type == TYPE_CLASSIC) {

        char *scandir = pares[sv->aresid].sa.s + pares[sv->aresid].live.scandir ;

        if (updown[2] == 'U' || updown[2] == 'D' || updown[2] == 'R') {

            if (!pares[sv->aresid].notify)
                updown[2] = updown[2] == 'U' ? 'u' : updown[2] == 'D' ? 'd' : updown[2] == 'R' ? 'r' : updown[2] ;

        }

        char tfmt[UINT32_FMT] ;
        tfmt[uint_fmt(tfmt, deadline)] = 0 ;

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


    }

    char *sa = pares[sv->aresid].sa.s ;
    char *name = sa + pares[sv->aresid].name ;
    size_t namelen = strlen(name) ;
    char *tree = pares[sv->aresid].sa.s + pares[sv->aresid].path.tree ;
    size_t treelen = strlen(tree) ;
    unsigned int timeout = 0 ;
    if (!what)
        timeout = pares[sv->aresid].execute.timeout.up ;
    else
        timeout = pares[sv->aresid].execute.timeout.down ;

    char script[treelen + SS_SVDIRS_LEN + SS_SVC_LEN + 1 + namelen + 7 + 1] ;
    auto_strings(script, tree, SS_SVDIRS, SS_SVC, "/", name) ;

    char tfmt[UINT32_FMT] ;
    tfmt[uint_fmt(tfmt, timeout)] = 0 ;

    char *oneshotdir = pares[sv->aresid].sa.s + pares[sv->aresid].live.oneshotddir ;
    char *scandir = pares[sv->aresid].sa.s + pares[sv->aresid].live.scandir ;
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
    newargv[m++] = script ;
    newargv[m++] = 0 ;

    log_trace("sending ", !what ? "up" : "down", " to: ", scandir) ;

    pid = child_spawn0(newargv[0], newargv, (char const *const *) environ) ;

    if (waitpid_nointr(pid, &wstat, 0) < 0)
        log_warnusys_return(LOG_EXIT_ZERO, "wait for s6-sudo") ;

    if (!WIFSIGNALED(wstat) && !WEXITSTATUS(wstat))
        return WEXITSTATUS(wstat) ;
    else
        return WIFSIGNALED(wstat) ? WTERMSIG(wstat) : WEXITSTATUS(wstat) ;
}

static int check_action(pidservice_t *apids, unsigned int pos, unsigned int receive, unsigned int what)
{
    unsigned int p = char2enum[receive] ;
    unsigned char action = actions[what][p] ;

    switch(action) {

        case SERVICE_ACTION_GOTIT:
            FLAGS_SET(apids[pos].state, (!what ? FLAGS_UP : FLAGS_DOWN)) ;
            return 1 ;

        case SERVICE_ACTION_FATAL:
            FLAGS_SET(apids[pos].state, FLAGS_FATAL) ;
            return -1 ;

        case SERVICE_ACTION_WAIT:
            return 0 ;

        case SERVICE_ACTION_UNKNOWN:
        default:
            log_die(LOG_EXIT_ZERO,"invalid action -- please make a bug report") ;
    }

}

static int async_deps(pidservice_t *apids, unsigned int i, unsigned int what, ssexec_t *info, graph_t *graph, unsigned int deadline)
{
    log_flow() ;

    int r ;
    unsigned int pos = 0, id = 0, ilog = 0, idx = 0 ;
    char buf[(UINT_FMT*2)*SS_MAX_SERVICE + 1] ;

    tain dead ;
    tain_from_millisecs(&dead, deadline) ;
    tain_now_set_stopwatch_g() ;
    tain_add_g(&dead, &dead) ;

    iopause_fd x = { .fd = apids[i].pipe[0], .events = IOPAUSE_READ, 0 } ;

    unsigned int n = apids[i].nedge ;
    unsigned int visit[n] ;

    graph_array_init_single(visit, n) ;

    while (pos < n) {

        r = iopause_g(&x, 1, &dead) ;

        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "iopause") ;

        if (!r) {
            errno = ETIMEDOUT ;
            log_dieusys(LOG_EXIT_SYS,"time out", pares[apids[i].aresid].sa.s + pares[apids[i].aresid].name) ;
        }

        if (x.revents & IOPAUSE_READ) {

            memset(buf, 0, sizeof(buf)) ;
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
                 *      index_of_the_ares_array_of_the_service_dependency:signal_receive
                 *
                 * typically:
                 *      - 10:D
                 *      - 30:u
                 *      - ...
                 *
                 * Split it and check the signal receive.*/
                int sep = get_len_until(line, ':') ;
                if (sep < 0)
                    log_die(LOG_EXIT_SYS, "received bad signal format -- please make a bug report") ;

                char c = line[sep + 1] ;
                line[sep] = 0 ;

                if (!uint0_scan(line, &id))
                    log_dieusys(LOG_EXIT_SYS, "retrieve service number -- please make a bug report") ;

                ilog = id ;

                log_trace(pares[apids[i].aresid].sa.s + pares[apids[i].aresid].name, " acknowledges: ", &c, " from: ", pares[ilog].sa.s + pares[ilog].name) ;

                if (!visit[pos]) {

                    id = pidservice_get_id(apids, id) ;
                    if (id < 0)
                        log_dieu(LOG_EXIT_SYS, "get apidservice id -- please make a bug report") ;

                    id = check_action(apids, id, c, what) ;
                    if (id < 0)
                        log_die(LOG_EXIT_SYS, "service dependency: ", pares[ilog].sa.s + pares[ilog].name, " of: ", pares[apids[i].aresid].sa.s + pares[apids[i].aresid].name," crashed") ;

                    if (!id)
                        continue ;

                    visit[pos++]++ ;
                }
            }
        }
        next:

    }

    return 1 ;
}

static int async(pidservice_t *apids, unsigned int i, unsigned int what, ssexec_t *info, graph_t *graph, unsigned int deadline)
{
    log_flow() ;

    int e = 0 ;

    char *name = graph->data.s + genalloc_s(graph_hash_t,&graph->hash)[apids[i].vertex].vertex ;

    log_info("beginning of the process of: ", name) ;

    if (FLAGS_ISSET(apids[i].state, (!what ? FLAGS_DOWN : FLAGS_UP))) {

        if (!FLAGS_ISSET(apids[i].state, FLAGS_BLOCK)) {

            FLAGS_SET(apids[i].state, FLAGS_BLOCK) ;

            if (apids[i].nedge)
                if (!async_deps(apids, i, what, info, graph, deadline))
                    log_warnu_return(LOG_EXIT_ZERO, !what ? "start" : "stop", " dependencies of service: ", name) ;

            e = doit(&apids[i], what, deadline) ;

        } else {

            log_info("Skipping service: ", name, " -- already in ", what ? "stopping" : "starting", " process") ;

            notify(apids, i, what ? "d" : "u", what) ;

        }

    } else {

        /** do not notify here, the handle will make it for us */
        log_info("Skipping service: ", name, " -- already ", what ? "down" : "up") ;

    }

    return e ;
}

static int waitit(pidservice_t *apids, unsigned int what, graph_t *graph, unsigned int deadline, ssexec_t *info)
{
    log_flow() ;

    unsigned int e = 0, pos = 0 ;
    int r ;
    pid_t pid ;
    pidservice_t apidservicetable[napid] ;
    pidservice_t_ref apidservice = apidservicetable ;

    tain dead ;
    tain_from_millisecs(&dead, deadline) ;
    tain_now_set_stopwatch_g() ;
    tain_add_g(&dead, &dead) ;

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

    for (pos = 0 ; pos < napid ; pos++) {

        pid = fork() ;

        if (pid < 0)
            log_dieusys(LOG_EXIT_SYS, "fork") ;

        if (!pid) {

            selfpipe_finish() ;

            close(apidservice[pos].pipe[1]) ;

            e = async(apidservice, pos, what, info, graph, deadline) ;

            goto end ;
        }

        apidservice[pos].pid = pid ;

        close(apidservice[pos].pipe[0]) ;

        npid++ ;
    }

    while (npid) {

        r = iopause_g(&x, 1, &dead) ;

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
    end:
        for (pos = 0 ; pos < napid ; pos++) {
            close(apidservice[pos].pipe[1]) ;
            close(apidservice[pos].pipe[0]) ;
        }

        return e ;
}

int ssexec_svctl(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    int r ;
    // what = 0 -> up signal
    uint8_t what = 0, requiredby = 0 ;
    static unsigned int deadline = 3000 ;
    graph_t graph = GRAPH_ZERO ;

    unsigned int areslen = 0, list[SS_MAX_SERVICE] ;
    resolve_service_t ares[SS_MAX_SERVICE] ;

    /*
     * STATE_FLAGS_TOPROPAGATE = 0
     * do not send signal to the depends/requiredby of the service.
     *
     * STATE_FLAGS_TOPROPAGATE = 1
     * send signal to the depends/requiredby of the service
     *
     * When we come from 66-start/stop tool we always want to
     * propagate the signal. But we may need/want to send a e.g. SIGHUP signal
     * to a specific service without interfering on its depends/requiredby services
     *
     * Also, we only deal with already supervised service. This tool is the signal sender,
     * it not intended to sanitize the state of the services.
     *
     * */
    uint32_t gflag = STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_ISSUPERVISED|STATE_FLAGS_WANTUP ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = subgetopt_r(argc,argv, OPTS_SVCTL, &l) ;
            if (opt == -1) break ;
            //if (opt == -2) log_die(LOG_EXIT_USER,"options must be set first") ;

            switch (opt) {

                case 'a' :
                case 'b' :
                case 'q' :
                case 'h' :
                case 'k' :
                case 't' :
                case 'i' :
                case '1' :
                case '2' :
                case 'p' :
                case 'c' :
                case 'y' :
                case 'r' :
                case 'o' :
                case 'd' :
                case 'u' :
                case 'x' :
                case 'O' :

                    if (datalen >= DATASIZE)
                        log_die(LOG_EXIT_USER, "too many arguments") ;

                    data[datalen++] = opt ;
                    break ;

                case 'w' :

                    if (!memchr("dDuUrR", l.arg[0], 6))
                        log_usage(usage_svctl) ;

                    updown[2] = l.arg[0] ;
                    opt_updown = 1 ;
                    break ;

                case 'P':
                    FLAGS_CLEAR(gflag, STATE_FLAGS_TOPROPAGATE) ;
                    break ;

                default :
                    log_usage(usage_svctl) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 1)
        log_usage(usage_svctl) ;

    if (!datalen)
        log_die(LOG_EXIT_USER, "too few arguments") ;

    if (info->timeout)
        deadline = info->timeout ;

    if (data[1] != 'u')
        what = 1 ;

    if (data[1] == 'r')
        reloadmsg++ ;

    if (what) {

        requiredby = 1 ;
        FLAGS_SET(gflag, STATE_FLAGS_WANTUP) ;
        FLAGS_CLEAR(gflag, STATE_FLAGS_WANTDOWN) ;
    }

    if ((svc_scandir_ok(info->scandir.s)) != 1)
        log_diesys(LOG_EXIT_SYS,"scandir: ", info->scandir.s," is not running") ;

    /** build the graph of the entire system.*/
    graph_build_service(&graph, ares, &areslen, info, gflag) ;

    if (!graph.mlen)
        log_die(LOG_EXIT_USER, "services selection is not supervised -- initiate its first") ;

    for (; *argv ; argv++) {

        int aresid = service_resolve_array_search(ares, areslen, *argv) ;
        if (aresid < 0)
            log_die(LOG_EXIT_USER, "service: ", *argv, " not available -- did you parsed it?") ;

        unsigned int l[graph.mlen], c = 0, pos = 0 ;

        /** find dependencies of the service from the graph, do it recursively */
        c = graph_matrix_get_edge_g_sorted_list(l, &graph, *argv, requiredby, 1) ;

        /** append to the list to deal with */
        for (; pos < c ; pos++)
            list[napid + pos] = l[pos] ;

        napid += c ;

        list[napid++] = aresid ;
    }

    pidservice_t apids[napid] ;

    pares = ares ;
    pareslen = &areslen ;

    pidservice_init_array(list, napid, apids, &graph, ares, areslen, info, requiredby, gflag) ;

    r = waitit(apids, what, &graph, deadline, info) ;

    graph_free_all(&graph) ;

    service_resolve_array_free(ares, areslen) ;

    return r ;
}
