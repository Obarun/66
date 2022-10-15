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







#include <stdio.h>





#define FLAGS_STARTING 1 // 1 starting not really up
#define FLAGS_STOPPING (1 << 1) // 2 stopping not really down
#define FLAGS_UP (1 << 2) // 4 really up
#define FLAGS_DOWN (1 << 3) // 8 really down
#define FLAGS_BLOCK (1 << 4) // 16 all deps are not up/down
#define FLAGS_UNBLOCK (1 << 5) // 32 all deps are up/down
#define FLAGS_FATAL (1 << 6) // 64 process crashed

#define DATASIZE 63

static ftrigr_t FIFO = FTRIGR_ZERO ;
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
    pid_t pid ;
    uint16_t ids ;
    int aresid ; // id at array ares
    unsigned int vertex ; // id at graph_hash_t struct
    uint8_t state ;
    int nedge ;
    unsigned int *edge ; // array of id at graph_hash_t struct
} ;
#define PIDSERVICE_ZERO { 0, 0, -1, 0, 0, 0, 0 }

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
    { SERVICE_ACTION_WAIT, SERVICE_ACTION_GOTIT, SERVICE_ACTION_UNKNOWN, SERVICE_ACTION_UNKNOWN, SERVICE_ACTION_FATAL, SERVICE_ACTION_WAIT, SERVICE_ACTION_GOTIT }, // !what -> up
    { SERVICE_ACTION_UNKNOWN, SERVICE_ACTION_UNKNOWN, SERVICE_ACTION_WAIT, SERVICE_ACTION_GOTIT, SERVICE_ACTION_FATAL, SERVICE_ACTION_WAIT, SERVICE_ACTION_GOTIT } // what -> down

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

static pidservice_t pidservice_init(unsigned int len)
{
    log_flow() ;

    pidservice_t pids = PIDSERVICE_ZERO ;

    pids.edge = (unsigned int *)malloc(len * sizeof(unsigned int)) ;

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

static void pidservice_free(pidservice_t *pids)
{
    log_flow() ;

    free(pids->edge) ;
}

static void pidservice_array_free(pidservice_t *apids,  unsigned int len)
{
    log_flow() ;

    size_t pos = 0 ;

    for(; pos < len ; pos++)
        pidservice_free(&apids[pos]) ;
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
            /**
             * This is should be recursive as long as the user ask for the propagation.
             * So, call graph_matrix_get_edge_g_sorted_list() with recursive mode.
             * This is overkill if we come from other 66 tools. Its call with the complete
             * service selection to deal with.
             * No choice here, we want a complete 66-svctl independence
             * */
            pids.nedge = graph_matrix_get_edge_g_sorted_list(pids.edge, g, name, requiredby, 1) ;

            if (pids.nedge < 0)
                log_dieu(LOG_EXIT_SYS,"get sorted ", requiredby ? "required by" : "dependency", " list of tree: ", name) ;
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

static void pidservice_init_fifo(pidservice_t *apids, graph_t *graph, ssexec_t *info, unsigned int deadline)
{
    log_flow() ;

    unsigned int pos = 0 ;
    gid_t gid ;

    tain dead ;
    tain_from_millisecs(&dead, deadline) ;
    tain_now_set_stopwatch_g() ;
    tain_add_g(&dead, &dead) ;

    if (!yourgid(&gid, info->owner))
        log_dieusys(LOG_EXIT_SYS, "get gid") ;

    if (!ftrigr_startf_g(&FIFO, &dead))
        log_dieusys(LOG_EXIT_SYS, "ftrigr_startf") ;

    for (; pos < napid ; pos++) {

        char *notifdir = pares[apids[pos].aresid].sa.s +  pares[apids[pos].aresid].live.notifdir ;

        if (!dir_create_parent(notifdir, 0700))
            log_dieusys(LOG_EXIT_SYS, "create directory: ", notifdir) ;

        if (!ftrigw_fifodir_make(notifdir, gid, 0))
            log_dieusys(LOG_EXIT_SYS, "make fifo directory: ", notifdir) ;

        /** may already exist, cleans it */
        if (!ftrigw_clean(notifdir))
            log_dieusys(LOG_EXIT_SYS, "clean fifo directory: ", notifdir) ;

        apids[pos].ids = ftrigr_subscribe_g(&FIFO, notifdir, "[uUdDFbB]", FTRIGR_REPEAT, &dead) ;

        if (!apids[pos].ids)
            log_dieusys(LOG_EXIT_SYS, "subcribe to: ", notifdir) ;
    }
}

static inline void kill_all(pidservice_t *apids)
{
    log_flow() ;

    unsigned int j = napid ;
    while (j--) kill(apids[j].pid, SIGKILL) ;
}

/**
 * @what: up or down
 * @success: 0 fail, 1 win
 * */
static void announce(pidservice_t *apids, unsigned int what, unsigned int success, unsigned int exitcode)
{
    log_flow() ;

    int fd = 0 ;
    char const *notifdir = pares[apids->aresid].sa.s + pares[apids->aresid].live.notifdir ;
    char const *name = pares[apids->aresid].sa.s + pares[apids->aresid].name ;
    char const *base = pares[apids->aresid].sa.s + pares[apids->aresid].path.home ;
    char const *scandir = pares[apids->aresid].sa.s + pares[apids->aresid].live.scandir ;
    size_t scandirlen = strlen(scandir) ;
    char file[scandirlen +  6] ;

    auto_strings(file, scandir, "/down") ;

    uint8_t flag = what ? FLAGS_DOWN : FLAGS_UP ;

    if (success) {

        FLAGS_SET(apids->state, FLAGS_BLOCK|FLAGS_FATAL) ;

        fd = open_trunc(file) ;
        if (fd < 0)
            log_dieusys(LOG_EXIT_SYS, "create file: ", scandir) ;
        fd_close(fd) ;

        log_trace("sends notification F to: ", notifdir) ;
        if (ftrigw_notify(notifdir, 'F') < 0)
            log_dieusys(LOG_EXIT_SYS, "notifies event directory: ", notifdir) ;

        char fmt[UINT_FMT] ;
        fmt[uint_fmt(fmt, exitcode)] = 0 ;

        log_1_warn("Unable to ", reloadmsg ? "reload" : what ? "stop" : "start", " service: ", name, " -- exited with signal: ", fmt) ;



    } else {

        if (!state_messenger(base, name, STATE_FLAGS_ISUP, what ? STATE_FLAGS_FALSE : STATE_FLAGS_TRUE))
            log_dieusys(LOG_EXIT_SYS, "send message to state of: ", name) ;

        FLAGS_SET(apids->state, flag|FLAGS_UNBLOCK) ;

        if (!pares[apids->aresid].execute.down && pares[apids->aresid].type == TYPE_CLASSIC) {

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

        log_trace("sends notification ", what ? "D" : "U", " to: ", notifdir) ;
        if (ftrigw_notify(notifdir, what ? 'D' : 'U') < 0)
            log_dieusys(LOG_EXIT_SYS, "notifies event directory: ", notifdir) ;

        log_info("Successfully ", reloadmsg ? "reloaded" : what ? "stopped" : "started", " service: ", name) ;

    }

    tain dead ;
    tain_from_millisecs(&dead, 3000) ;
    tain_now_set_stopwatch_g() ;
    tain_add_g(&dead, &dead) ;

    if (!ftrigr_unsubscribe_g(&FIFO, apids->ids, &dead))
        log_dieusys(LOG_EXIT_SYS, "unsubcribe: ", name) ;
}

static int handle_signal(pidservice_t *apids, unsigned int what, graph_t *graph, ssexec_t *info)
{
    log_flow() ;

    int ok = 1 ;

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

                            announce(&apids[pos], what, 0, 0) ;

                        } else {

                            ok = 0 ;
                            announce(&apids[pos], what, 1, WIFSIGNALED(wstat) ? WTERMSIG(wstat) : WEXITSTATUS(wstat)) ;
                        }

                        npid-- ;
                    }
                }
                break ;
            case SIGTERM :
            case SIGINT :
                    log_1_warn("received SIGINT, aborting tree transition") ;
                    kill_all(apids) ;
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
    int wstat, e = 0 ;

    if (type == TYPE_MODULE || type == TYPE_BUNDLE)
        /**
         * Those type are not real services. Passing here with
         * this kind of service means that the dependencies
         * of the service was made anyway. So, we can consider it as
         * already up/down.
         * */
         return 1 ;

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

        log_info("sending ", opt_updown ? newargv[2] : "", opt_updown ? " " : "", data, " to: ", scandir) ;

        pid = child_spawn0(newargv[0], newargv, (char const *const *) environ) ;

        if (waitpid_nointr(pid, &wstat, 0) < 0)
            log_warnusys_return(LOG_EXIT_ZERO, "wait for s6-svc") ;

        if (!WIFSIGNALED(wstat) && !WEXITSTATUS(wstat))
            e = 1 ;

    } else if (type == TYPE_ONESHOT) {

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

        log_info("sending ", !what ? "up" : "down", " to: ", scandir) ;

        pid = child_spawn0(newargv[0], newargv, (char const *const *) environ) ;

        if (waitpid_nointr(pid, &wstat, 0) < 0)
            log_warnusys_return(LOG_EXIT_ZERO, "wait for s6-sudo") ;


        if (!WIFSIGNALED(wstat) && !WEXITSTATUS(wstat))
            e = 1 ;
    }

    return e ;
}

static int async_deps(pidservice_t *apids, unsigned int i, unsigned int what, ssexec_t *info, graph_t *graph, unsigned int deadline)
{
    log_flow() ;

    int e = 0, r ;
    unsigned int pos = 0 ;

    tain dead ;
    tain_from_millisecs(&dead, deadline) ;
    tain_now_set_stopwatch_g() ;
    tain_add_g(&dead, &dead) ;

    iopause_fd x = { .fd = ftrigr_fd(&FIFO), .events = IOPAUSE_READ } ;
    stralloc sa = STRALLOC_ZERO ;

    while (apids[i].nedge) {

        /** TODO: the pidvertex_get_id() function make a loop
         * through the apids array to find the corresponding
         * index of the edge at the apids array.
         * This is clearly a waste of time and should be optimized. */
        unsigned int id = pidservice_get_id(apids, apids[i].edge[pos]) ;

        if (id < 0)
            log_dieu(LOG_EXIT_SYS, "get apidservice id -- please make a bug report") ;

        r = iopause_g(&x, 1, &dead) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "iopause") ;
        if (!r)
            log_die(LOG_EXIT_SYS,"time out") ;

        if (x.revents & IOPAUSE_READ) {

            if (ftrigr_update(&FIFO) < 0)
                log_dieusys(LOG_EXIT_SYS, "ftrigr_update") ;

            for(pos = 0 ; pos < napid ; pos++) {

                sa.len = 0 ;
                r = ftrigr_checksa(&FIFO, apids[pos].ids, &sa) ;

                if (r < 0)
                    log_dieusys(LOG_EXIT_SYS, "ftrigr_check") ;

                else if (r) {

                    size_t l = 0 ;

                    for (; l < sa.len ; l++) {

                        unsigned int p = char2enum[(unsigned int)sa.s[l]] ;
                        unsigned char action = actions[what][p] ;
                        char s[2] = { sa.s[l], 0 } ;
                        log_trace("received signal: ", s, " from: ", pares[apids[pos].aresid].sa.s + pares[apids[pos].aresid].name) ;
                        switch(action) {

                            case SERVICE_ACTION_GOTIT:
                                FLAGS_SET(apids[pos].state, (!what ? FLAGS_UP : FLAGS_DOWN)) ;
                                goto next ;

                            case SERVICE_ACTION_FATAL:
                                FLAGS_SET(apids[pos].state, FLAGS_FATAL) ;
                                goto err ;

                            case SERVICE_ACTION_WAIT:
                                goto next ;

                            case SERVICE_ACTION_UNKNOWN:
                            default:
                                log_die(LOG_EXIT_ZERO,"invalid action -- please make a bug report") ;
                        }
                    }
                }
            }
        }
        next:
        apids[i].nedge-- ;
    }

    e = 1 ;
    err:
        stralloc_free(&sa) ;
        return e ;
}

static int async(pidservice_t *apids, unsigned int i, unsigned int what, ssexec_t *info, graph_t *graph, unsigned int deadline)
{
    log_flow() ;

    int e = 1 ;

    char *name = graph->data.s + genalloc_s(graph_hash_t,&graph->hash)[apids[i].vertex].vertex ;
    char *notifdir = pares[apids[i].aresid].sa.s + pares[apids[i].aresid].live.notifdir ;

    if (FLAGS_ISSET(apids[i].state, (!what ? FLAGS_DOWN : FLAGS_UP))) {

        if (!FLAGS_ISSET(apids[i].state, FLAGS_BLOCK)) {

            FLAGS_SET(apids[i].state, FLAGS_BLOCK) ;

            if (apids[i].nedge)
                if (!async_deps(apids, i, what, info, graph, deadline))
                    log_warnu_return(LOG_EXIT_ZERO, !what ? "start" : "stop", " dependencies of service: ", name) ;

            e = doit(&apids[i], what, deadline) ;

        } else {

            log_info("Skipping service: ", name, " -- already in ", what ? "stopping" : "starting", " process") ;

            log_trace("sends notification ", what ? "d" : "u", " to : ", notifdir) ;
            if (ftrigw_notify(notifdir, what ? 'd' : 'u') < 0)
                log_warnusys_return(LOG_EXIT_ZERO, "notifies event directory: ", notifdir) ;

        }

    } else {

        log_info("Skipping service: ", name, " -- already ", what ? "down" : "up") ;

        //log_trace("sends notification ", what ? "D" : "U", " to : ", notifdir) ;
        //if (ftrigw_notify(notifdir, what ? 'D' : 'U') < 0)
        //    log_warnusys_return(LOG_EXIT_ZERO, "notifies event directory: ", notifdir) ;

    }

    return e ;
}

static int waitit(pidservice_t *apids, unsigned int what, graph_t *graph, unsigned int deadline, ssexec_t *info)
{
    log_flow() ;

    unsigned int e = 1, pos = 0 ;
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
        !selfpipe_trap(SIGTERM) ||
        !sig_altignore(SIGPIPE))
            log_dieusys(LOG_EXIT_SYS, "selfpipe_trap") ;

    pidservice_init_fifo(apids, graph, info, deadline) ;

    for (pos = 0 ; pos < napid ; pos++)
        apidservice[pos] = apids[pos] ;

    for (pos = 0 ; pos < napid ; pos++) {

        pid = fork() ;

        if (pid < 0)
            log_dieusys(LOG_EXIT_SYS, "fork") ;

        if (!pid) {
            e = async(apidservice, pos, what, info, graph, deadline) ;
            goto freed ;
        }

        apidservice[pos].pid = pid ;

        npid++ ;
    }

    iopause_fd x = { .fd = spfd, .events = IOPAUSE_READ } ;

    while (npid) {

        r = iopause_g(&x, 1, &dead) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "iopause") ;
        if (!r)
            log_die(LOG_EXIT_SYS,"time out") ;

        if (x.revents & IOPAUSE_READ)
            if (!handle_signal(apidservice, what, graph, info))
                e = 0 ;
    }
    freed:
    ftrigr_end(&FIFO) ;
    selfpipe_finish() ;

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
     * also send signal to the depends/requiredby of the service
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

    /** build the graph of the entire system
     *
     *
     * ici tu passe dans le graph sans controle de savoir si le service
     * est au minima parsed voir initialized
     *
     *  */
    graph_build_service(&graph, ares, &areslen, info, gflag) ;

/*
    {
        stralloc sa = STRALLOC_ZERO ;
        char const *exclude[1] = { 0 } ;
        char solve[info->base.len + SS_SYSTEM_LEN + SS_RESOLVE_LEN + 1 + SS_SERVICE_LEN + 1] ;

        auto_strings(solve, info->base.s, SS_SYSTEM, SS_RESOLVE, "/", SS_SERVICE) ;

        if (!sastr_dir_get_recursive(&sa,solve,exclude,S_IFREG, 0))
            log_dieu(LOG_EXIT_SYS, "get resolve files") ;

        service_graph_g(sa.s, sa.len, &graph, ares, &areslen, info, STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_WANTUP|STATE_FLAGS_WANTDOWN) ;

        stralloc_free(&sa) ;
    }
*/
    if (!graph.mlen)
        log_die(LOG_EXIT_USER, "services selection is not supervised -- initiate its first") ;

    for (; *argv ; argv++) {

        int aresid = service_resolve_array_search(ares, areslen, *argv) ;
        if (aresid < 0)
            log_die(LOG_EXIT_USER, "service: ", *argv, " not available -- did you parsed it?") ;

        unsigned int l[graph.mlen], c = 0, pos = 0 ;

        /** find dependencies of the service from the graph, do it recursively */
        c = graph_matrix_get_edge_g_sorted_list(l, &graph, *argv, !!requiredby, 1) ;

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
    pidservice_array_free(apids, napid) ;
    service_resolve_array_free(ares, areslen) ;

    return (!r) ? 111 : 0 ;
}
