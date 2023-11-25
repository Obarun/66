/*
 * ssexec_tree_signal.c
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

#include <sys/stat.h>//S_IFREG,umask
#include <sys/types.h>//pid_t
#include <fcntl.h>//O_RDWR
#include <unistd.h>//dup2,setsid,chdir,fork
#include <sys/ioctl.h>
#include <stdint.h>//uint8_t
#include <stdlib.h>//realpath
#include <string.h>//strdup

#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/log.h>
#include <oblibs/sastr.h>
#include <oblibs/obgetopt.h>
#include <oblibs/files.h>
#include <oblibs/graph.h>
#include <oblibs/directory.h>

#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/posixplz.h>
#include <skalibs/tai.h>
#include <skalibs/selfpipe.h>
#include <skalibs/sig.h>
#include <skalibs/iopause.h>

#include <66/ssexec.h>
#include <66/constants.h>
#include <66/tree.h>
#include <66/svc.h>//scandir_ok
#include <66/utils.h>
#include <66/graph.h>
#include <66/state.h>

#include <s6/ftrigr.h>
#include <s6/ftrigw.h>

#define FLAGS_STARTING 1 // 1 starting not really up
#define FLAGS_STOPPING (1 << 1) // 2 stopping not really down
#define FLAGS_UP (1 << 2) // 4 really up
#define FLAGS_DOWN (1 << 3) // 8 really down
#define FLAGS_BLOCK (1 << 4) // 16 all deps are not up/down
#define FLAGS_UNBLOCK (1 << 5) // 32 all deps up/down
#define FLAGS_FATAL (1 << 6) // 64 process crashed

static unsigned int napid = 0 ;
static unsigned int npid = 0 ;

static resolve_tree_t_ref pares = 0 ;
static unsigned int *pareslen = 0 ;
static uint8_t reloadmsg = 0 ;

typedef struct pidtree_s pidtree_t, *pidtree_t_ref ;
struct pidtree_s
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
     * to notify when a tree is started/stopped */
    unsigned int notif[SS_MAX_SERVICE + 1] ;
} ;
#define PIDTREE_ZERO { { -1, -1 }, -1, -1, 0, 0, 0, { 0 } }

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

typedef enum tree_action_e tree_action_t, *tree_action_t_ref ;
enum tree_action_e
{
    TREE_ACTION_GOTIT = 0,
    TREE_ACTION_WAIT,
    TREE_ACTION_FATAL,
    TREE_ACTION_UNKNOWN
} ;

static const unsigned char actions[2][7] = {
    // u U d D F b B
    { TREE_ACTION_WAIT, TREE_ACTION_GOTIT, TREE_ACTION_UNKNOWN, TREE_ACTION_UNKNOWN, TREE_ACTION_FATAL, TREE_ACTION_WAIT, TREE_ACTION_WAIT }, // !what -> up
    { TREE_ACTION_UNKNOWN, TREE_ACTION_UNKNOWN, TREE_ACTION_WAIT, TREE_ACTION_GOTIT, TREE_ACTION_FATAL, TREE_ACTION_WAIT, TREE_ACTION_WAIT } // what -> down

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

static inline unsigned int lookup (char const *const *table, char const *signal)
{
    unsigned int i = 0 ;
    for (; table[i] ; i++) if (!strcmp(signal, table[i])) break ;
    return i ;
}

static inline unsigned int parse_signal (char const *signal, ssexec_t *info)
{
    log_flow() ;

    static char const *const signal_table[] = {
        "start",
        "stop",
        "unsupervise",
        0
    } ;
    unsigned int i = lookup(signal_table, signal) ;
    if (!signal_table[i]) log_usage(info->usage, "\n", info->help) ;
    return i ;
}
/**
 * huh!! a revoir et faire test
 *
 *
*/
static void all_redir_fd(void)
{
    log_flow() ;
    char *target = !isatty(1) ? "/dev/null" : "/dev/tty" ;
    int fd, count = 3 ;
    while((fd = open(target,O_RDWR|O_NOCTTY)) >= 0 && count <= fd) {
        close(count) ;
        count++ ;
        close(fd) ;
        //if (fd >= 3)
        //    break ;
    }

    dup2 (fd,0) ;
    dup2 (fd,1) ;
    dup2 (fd,2) ;
    fd_close(fd) ;

    if (setsid() < 0)
        log_dieusys(LOG_EXIT_SYS,"setsid") ;

    if ((chdir("/")) < 0)
        log_dieusys(LOG_EXIT_SYS,"chdir") ;

    ioctl(0,TIOCSCTTY,1) ;

    umask(022) ;
}

void tree_resolve_array_free(resolve_tree_t *ares, unsigned int areslen)
{
    log_flow() ;

    unsigned int pos = 0 ;
    for (; pos < areslen ; pos++)
        stralloc_free(&ares[pos].sa) ;
}

static inline void kill_all(pidtree_t *apidt)
{
    log_flow() ;

    unsigned int j = napid ;
    while (j--) kill(apidt[j].pid, SIGKILL) ;
}

static pidtree_t pidtree_init(unsigned int len)
{
    log_flow() ;

    pidtree_t pids = PIDTREE_ZERO ;

    if (len > SS_MAX_SERVICE)
        log_die(LOG_EXIT_SYS, "too many trees") ;

    graph_array_init_single(pids.edge, len) ;

    return pids ;
}


static int pidtree_get_id(pidtree_t *apidt, unsigned int id)
{
    log_flow() ;

    unsigned int pos = 0 ;

    for (; pos < napid ; pos++) {
        if (apidt[pos].vertex == id)
            return (unsigned int) pos ;
    }
    return -1 ;
}


static void notify(pidtree_t *apidt, unsigned int pos, char const *sig, unsigned int what)
{
    log_flow() ;

    unsigned int i = 0, idx = 0 ;
    char fmt[UINT_FMT] ;
    uint8_t flag = what ? FLAGS_DOWN : FLAGS_UP ;

    for (; i < apidt[pos].nnotif ; i++) {

        for (idx = 0 ; idx < napid ; idx++) {

            if (apidt[pos].notif[i] == apidt[idx].vertex && !FLAGS_ISSET(apidt[idx].state, flag))  {

                size_t nlen = uint_fmt(fmt, apidt[pos].aresid) ;
                fmt[nlen] = 0 ;
                size_t len = nlen + 1 + 2 ;
                char s[len + 1] ;
                auto_strings(s, fmt, ":", sig, "@") ;

                log_trace("sends notification ", sig, " to: ", pares[apidt[idx].aresid].sa.s + pares[apidt[idx].aresid].name, " from: ", pares[apidt[pos].aresid].sa.s + pares[apidt[pos].aresid].name) ;

                if (write(apidt[idx].pipe[1], s, strlen(s)) < 0)
                    log_dieusys(LOG_EXIT_SYS, "send notif to: ", pares[apidt[idx].aresid].sa.s + pares[apidt[idx].aresid].name) ;
            }
        }
    }
}

/**
 * @what: up or down
 * @success: 0 fail, 1 win
 * */
static void announce(unsigned int pos, pidtree_t *apidt, char const *base, unsigned int what, unsigned int success, unsigned int exitcode)
{
    log_flow() ;

    char fmt[UINT_FMT] ;
    char const *treename = pares[apidt[pos].aresid].sa.s + pares[apidt[pos].aresid].name ;

    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;

    uint8_t flag = what ? FLAGS_DOWN : FLAGS_UP ;

    if (success) {

        notify(apidt, pos, "F", what) ;

        fmt[uint_fmt(fmt, exitcode)] = 0 ;

        log_1_warnu(reloadmsg == 0 ? "start" : reloadmsg > 1 ? "unsupervise" : what == 0 ? "start" : "stop", " tree: ", treename, " -- exited with signal: ", fmt) ;

        FLAGS_SET(apidt[pos].state, FLAGS_BLOCK|FLAGS_FATAL) ;

    } else {

        notify(apidt, pos, what ? "D" : "U", what) ;

        FLAGS_CLEAR(apidt[pos].state, FLAGS_BLOCK) ;
        FLAGS_SET(apidt[pos].state, flag|FLAGS_UNBLOCK) ;

        log_info("Successfully ", reloadmsg == 0 ? "started" : reloadmsg > 1 ? "unsupervised" : what == 0 ? "started" : "stopped", " tree: ", treename) ;
    }

    resolve_free(wres) ;
}

static void pidtree_init_array(unsigned int *list, unsigned int listlen, pidtree_t *apidt, graph_t *g, resolve_tree_t *ares, unsigned int areslen, ssexec_t *info, uint8_t requiredby, uint8_t what)
{
    log_flow() ;

    unsigned int pos = 0 ;

    for (; pos < listlen ; pos++) {

        pidtree_t pids = pidtree_init(g->mlen) ;

        char *name = g->data.s + genalloc_s(graph_hash_t,&g->hash)[list[pos]].vertex ;

        pids.aresid = tree_resolve_array_search(ares, areslen, name) ;

        if (pids.aresid < 0)
            log_dieu(LOG_EXIT_SYS,"find ares id of: ", name, " -- please make a bug report") ;

        pids.nedge = graph_matrix_get_edge_g_sorted_list(pids.edge, g, name, requiredby, 1) ;

        if (pids.nedge < 0)
            log_dieu(LOG_EXIT_SYS,"get sorted ", requiredby ? "required by" : "dependency", " list of tree: ", name) ;

        pids.nnotif = graph_matrix_get_edge_g_sorted_list(pids.notif, g, name, !requiredby, 1) ;

        if (pids.nnotif < 0)
            log_dieu(LOG_EXIT_SYS,"get sorted ", !requiredby ? "required by" : "dependency", " list of tree: ", name) ;

        pids.vertex = graph_hash_vertex_get_id(g, name) ;

        if (pids.vertex < 0)
            log_dieu(LOG_EXIT_SYS, "get vertex id -- please make a bug report") ;

        if (what)
            FLAGS_SET(pids.state, FLAGS_UP) ;
        else
            FLAGS_SET(pids.state, FLAGS_DOWN) ;

        apidt[pos] = pids ;
    }
}

static int handle_signal(pidtree_t *apidt, unsigned int what, graph_t *graph, ssexec_t *info)
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
                        if (apidt[pos].pid == r)
                            break ;

                    if (pos < napid) {

                        if (!WIFSIGNALED(wstat) && !WEXITSTATUS(wstat)) {

                            announce(pos, apidt, info->base.s, what, 0, 0) ;

                        } else {

                            ok = WIFSIGNALED(wstat) ? WTERMSIG(wstat) : WEXITSTATUS(wstat) ;
                            announce(pos, apidt, info->base.s, what, 1, ok) ;

                            kill_all(apidt) ;
                            break ;
                        }

                        npid-- ;
                    }
                }
                break ;
            case SIGTERM :
            case SIGKILL :
            case SIGINT :
                    log_1_warn("aborting transaction") ;
                    kill_all(apidt) ;
                    ok = 110 ;
                    break ;
            default : log_die(LOG_EXIT_SYS, "unexpected data in selfpipe") ;
        }
    }

    return ok ;
}

/** this following function come from:
 * https://git.skarnet.org/cgi-bin/cgit.cgi/s6-rc/tree/src/s6-rc/s6-rc.c#n111
 * under license ISC where parameters was modified
static uint32_t compute_timeout (uint32_t timeout, tain *deadline)
{
  uint32_t t = timeout ;
  int globalt ;
  tain globaltto ;
  tain_sub(&globaltto, deadline, &STAMP) ;
  globalt = tain_to_millisecs(&globaltto) ;
  if (!globalt) globalt = 1 ;
  if (globalt > 0 && (!t || (unsigned int)globalt < t))
    t = (uint32_t)globalt ;
  return t ;
}
 */
static int ssexec_callback(stralloc *sa, ssexec_t *info, unsigned int what)
{
    log_flow() ;

    int r, e = 1 ;
    size_t pos = 0, len = sa->len ;
    ss_state_t ste = STATE_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;
    char t[len + 1] ;

    sastr_to_char(t, sa) ;

    sa->len = 0 ;

    /** only deal with enabled service at up time and
     * supervised service at down time */
    for (; pos < len ; pos += strlen(t + pos) + 1) {

        char *name = t + pos ;

        r = resolve_read_g(wres, info->base.s, name) ;
        if (r == -1)
            log_dieu(LOG_EXIT_SYS, "read resolve file of: ", name) ;
        if (!r)
            log_dieu(LOG_EXIT_SYS, "read resolve file of: ", name, " -- please make a bug report") ;

        if (!state_read(&ste, &res))
            log_dieu(LOG_EXIT_SYS, "read state file of: ", name, " -- please make a bug report") ;

        if (!what ? res.enabled : ste.issupervised == STATE_FLAGS_TRUE && !res.earlier) {

            if (get_rstrlen_until(name, SS_LOG_SUFFIX) < 0 && !res.inns)
                if (!sastr_add_string(sa, name))
                    log_dieu(LOG_EXIT_SYS, "add string") ;
        }
    }

    resolve_free(wres) ;

    if (!sa->len) {
        e = 0 ;
        goto end ;
    }

    {
        pos = 0, len = sastr_nelement(sa) ;

        int n = what == 2 ? 2 : 1 ;
        int nargc = n + len ;
        char const *prog = PROG ;
        char const *newargv[nargc] ;
        unsigned int m = 0 ;

        newargv[m++] = "signal" ;
        if (what == 2)
            newargv[m++] = "-u" ;

        FOREACH_SASTR(sa, pos)
            newargv[m++] = sa->s + pos ;

        newargv[m] = 0 ;

        if (!what) {

            PROG = "start" ;
            e = ssexec_start(nargc, newargv, info) ;
            PROG = prog ;

        } else {

            PROG = "stop" ;
            e = ssexec_stop(nargc, newargv, info) ;
            PROG = prog ;
        }
    }
    end:
    return e ;
}

static int doit(char const *treename, ssexec_t *sinfo, unsigned int what, tain *deadline)
{
    log_flow() ;

    int r, e = 1 ;
    ssexec_t info = SSEXEC_ZERO ;

    ssexec_copy(&info, sinfo) ;

    {
        info.treename.len = 0 ;
        info.opt_tree = 1 ;
        if (!auto_stra(&info.treename, treename))
            log_die_nomem("stralloc") ;

        r = tree_sethome(&info) ;
        if (r <= 0)
            log_warnu_return(LOG_EXIT_ZERO, "find tree: ", info.treename.s) ;

        if (!tree_get_permissions(info.base.s, info.treename.s))
            log_warn_return(LOG_EXIT_ZERO, "You're not allowed to use the tree: ", info.treename.s) ;
    }

    {
        stralloc sa = STRALLOC_ZERO ;

        if (!resolve_get_field_tosa_g(&sa, info.base.s, info.treename.s, DATA_TREE, E_RESOLVE_TREE_CONTENTS))
            log_warnu_return(LOG_EXIT_ZERO, "get services list from tree: ", info.treename.s) ;

        if (!sa.len) {

            log_info("Empty tree: ", info.treename.s, " -- nothing to do") ;

        } else {

            int r = ssexec_callback(&sa, &info, what) ;
            if (r)
                goto err ;

            if (what == 2)
                log_info("Unsupervised successfully tree: ", info.treename.s) ;
        }

        stralloc_free(&sa) ;
    }

    e = 0 ;
    err:
        ssexec_free(&info) ;
        return e ;
}

static int check_action(pidtree_t *apidt, unsigned int pos, unsigned int receive, unsigned int what)
{
    unsigned int p = char2enum[receive] ;
    unsigned char action = actions[what][p] ;

    switch(action) {

        case TREE_ACTION_GOTIT:
            FLAGS_SET(apidt[pos].state, (!what ? FLAGS_UP : FLAGS_DOWN)) ;
            return 1 ;

        case TREE_ACTION_FATAL:
            FLAGS_SET(apidt[pos].state, FLAGS_FATAL) ;
            return -1 ;

        case TREE_ACTION_WAIT:
            return 0 ;

        case TREE_ACTION_UNKNOWN:
        default:
            log_die(LOG_EXIT_ZERO,"invalid action -- please make a bug report") ;
    }

}

static int async_deps(pidtree_t *apidt, unsigned int i, unsigned int what, ssexec_t *info, graph_t *graph, tain *deadline)
{
    log_flow() ;

    int r ;
    unsigned int pos = 0, id = 0, ilog = 0, idx = 0 ;
    char buf[(UINT_FMT*2)*SS_MAX_SERVICE + 1] ;

    tain dead ;
    tain_now_set_stopwatch_g() ;
    tain_add_g(&dead, deadline) ;

    iopause_fd x = { .fd = apidt[i].pipe[0], .events = IOPAUSE_READ, 0 } ;

    unsigned int n = apidt[i].nedge ;
    unsigned int visit[n + 1] ;

    memset(visit, 0, (n + 1) * sizeof (unsigned int));

    while (pos < n) {

        r = iopause_g(&x, 1, &dead) ;

        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "iopause") ;

        if (!r) {
            errno = ETIMEDOUT ;
            log_dieusys(LOG_EXIT_SYS,"time out", pares[apidt[i].aresid].sa.s + pares[apidt[i].aresid].name) ;
        }

        if (x.revents & IOPAUSE_READ) {

            memset(buf, 0, ((UINT_FMT*2)*SS_MAX_SERVICE + 1) * sizeof(char)) ;
            r = read(apidt[i].pipe[0], buf, sizeof(buf)) ;
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
                 *      index_of_the_ares_array_of_the_tree_dependency:signal_receive
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

                unsigned int c = line[sep + 1] ;
                char pc[2] = { c, 0 } ;
                line[sep] = 0 ;

                if (!uint0_scan(line, &id))
                    log_dieusys(LOG_EXIT_SYS, "retrieve service number -- please make a bug report") ;

                ilog = id ;

                log_trace(pares[apidt[i].aresid].sa.s + pares[apidt[i].aresid].name, " acknowledges: ", pc, " from: ", pares[ilog].sa.s + pares[ilog].name) ;

                if (!visit[pos]) {

                    id = pidtree_get_id(apidt, id) ;
                    if (id < 0)
                        log_dieu(LOG_EXIT_SYS, "get apidtree id -- please make a bug report") ;

                    id = check_action(apidt, id, c, what) ;
                    if (id < 0)
                        log_die(LOG_EXIT_SYS, "tree dependency: ", pares[ilog].sa.s + pares[ilog].name, " of: ", pares[apidt[i].aresid].sa.s + pares[apidt[i].aresid].name," crashed") ;

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

static int async(pidtree_t *apidt, unsigned int i, unsigned int what, ssexec_t *info, graph_t *graph, tain *deadline)
{
    log_flow() ;

    int e = 0 ;

    char *name = graph->data.s + genalloc_s(graph_hash_t,&graph->hash)[apidt[i].vertex].vertex ;

    log_trace("beginning of the process of tree: ", name) ;

    if (FLAGS_ISSET(apidt[i].state, (!what ? FLAGS_DOWN : FLAGS_UP)) ||
        /** force to pass through unsupersive process even
         * if the tree is marked down */
        FLAGS_ISSET(apidt[i].state, (what ? FLAGS_DOWN : FLAGS_UP)) && what == 2) {

        if (!FLAGS_ISSET(apidt[i].state, FLAGS_BLOCK)) {

            FLAGS_SET(apidt[i].state, FLAGS_BLOCK) ;

            if (apidt[i].nedge)
                if (!async_deps(apidt, i, what, info, graph, deadline))
                    log_warnu_return(LOG_EXIT_ZERO, !what ? "start" : "stop", " dependencies of tree: ", name) ;

            e = doit(name, info, what, deadline) ;

        } else {

            log_trace("skipping tree: ", name, " -- already in ", what ? "stopping" : "starting", " process") ;

            notify(apidt, i, what ? "d" : "u", what) ;

        }

    } else {

        /** do not notify here, the handle will make it for us */
        log_trace("skipping tree: ", name, " -- already ", what ? "down" : "up") ;

    }

    return e ;
}

static int waitit(pidtree_t *apidt, unsigned int what, graph_t *graph, tain *deadline, ssexec_t *info)
{
    log_flow() ;

    unsigned int e = 0, pos = 0 ;
    int r ;
    pid_t pid ;
    pidtree_t apidtreetable[napid] ;
    pidtree_t_ref apidtree = apidtreetable ;

    tain_now_set_stopwatch_g() ;
    tain_add_g(deadline, deadline) ;

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

        apidtree[pos] = apidt[pos] ;

        if (pipe(apidtree[pos].pipe) < 0)
            log_dieusys(LOG_EXIT_SYS, "pipe");

    }

    for (pos = 0 ; pos < napid ; pos++) {

        pid = fork() ;

        if (pid < 0)
            log_dieusys(LOG_EXIT_SYS, "fork") ;

        if (!pid) {

            selfpipe_finish() ;

            close(apidtree[pos].pipe[1]) ;

            e = async(apidtree, pos, what, info, graph, deadline) ;

            goto end ;
        }

        apidtree[pos].pid = pid ;

        close(apidtree[pos].pipe[0]) ;

        npid++ ;
    }

    while (npid) {

        r = iopause_g(&x, 1, deadline) ;

        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "iopause") ;

        if (!r) {
            errno = ETIMEDOUT ;
            log_diesys(LOG_EXIT_SYS,"time out") ;
        }

        if (x.revents & IOPAUSE_READ) {
            e = handle_signal(apidtree, what, graph, info) ;

            if (e)
                break ;
        }
    }


    selfpipe_finish() ;
    end:
        for (pos = 0 ; pos < napid ; pos++) {
            close(apidtree[pos].pipe[1]) ;
            close(apidtree[pos].pipe[0]) ;
        }

        return e ;
}

int ssexec_tree_signal(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    int r, shut = 0, fd ;
    tain deadline ;
    uint8_t what = 0, requiredby = 0, found = 0 ;
    stralloc sa = STRALLOC_ZERO ;
    size_t pos = 0 ;

    unsigned int areslen = 0, list[SS_MAX_SERVICE + 1], visit[SS_MAX_SERVICE + 1] ;
    resolve_tree_t ares[SS_MAX_SERVICE + 1] ;
    resolve_wrapper_t_ref wres = 0 ;

    graph_t graph = GRAPH_ZERO ;

    memset(list, 0, (SS_MAX_SERVICE + 1) * sizeof(unsigned int)) ;
    memset(visit, 0, (SS_MAX_SERVICE + 1) * sizeof(unsigned int)) ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = subgetopt_r(argc, argv, OPTS_TREE_SIGNAL, &l) ;
            if (opt == -1) break ;

            switch (opt) {
                case 'f' :  shut = 1 ; break ;
                default :   log_usage(info->usage, "\n", info->help) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 1)
        log_usage(info->usage, "\n", info->help) ;

    info->treename.len = 0 ;

    if (argv[1]) {
        if (!auto_stra(&info->treename, argv[1]))
            log_die_nomem("stralloc") ;
    }

    if (info->timeout)
        tain_from_millisecs(&deadline, info->timeout) ;
    else
        deadline = tain_infinite_relative ;

    what = parse_signal(*argv, info) ;

    reloadmsg = what ;

    if (what)
        requiredby = 1 ;

    if ((svc_scandir_ok(info->scandir.s)) <= 0)
        log_die(LOG_EXIT_SYS,"scandir: ", info->scandir.s," is not running") ;

    graph_build_tree(&graph, info->base.s, !info->treename.len ? E_RESOLVE_TREE_MASTER_ENABLED : E_RESOLVE_TREE_MASTER_CONTENTS) ;

    if (!graph.mlen)
        log_die(LOG_EXIT_USER, "trees selection is not created -- creates at least one tree") ;

    if (!graph_matrix_sort_tosa(&sa, &graph))
        log_dieu(LOG_EXIT_SYS, "get list of trees for graph -- please make a bug report") ;

    FOREACH_SASTR(&sa, pos) {

        char *treename = sa.s + pos ;

        /** only one tree */
        if (info->treename.len) {

            if (!strcmp(info->treename.s, treename))
                found = 1 ;
            else continue ;
        }

        if (tree_resolve_array_search(ares, areslen, treename) < 0) {

            resolve_tree_t tres = RESOLVE_TREE_ZERO ;
            /** need to make a copy of the resolve due of the freed
             * of the wres struct at the end of the process */
            resolve_tree_t cp = RESOLVE_TREE_ZERO ;
            wres = resolve_set_struct(DATA_TREE, &tres) ;

            if (resolve_read_g(wres, info->base.s, treename) <= 0)
                log_dieu(LOG_EXIT_SYS, "read resolve file of: ", treename, " -- please make a bug report") ;

            tree_resolve_copy(&cp, &tres) ;

            ares[areslen++] = cp ;

            resolve_free(wres) ;
        }

        unsigned int l[graph.mlen], c = 0, pos = 0, idx = 0 ;

        idx = graph_hash_vertex_get_id(&graph, treename) ;

        if (!visit[idx]) {
            /** avoid double entry */
            list[napid++] = idx ;
            visit[idx] = 1 ;

        }

        /** find dependencies of the tree from the graph, do it recursively */
        c = graph_matrix_get_edge_g_sorted_list(l, &graph, treename, requiredby, 1) ;

        /** append to the list to deal with */
        for (; pos < c ; pos++) {
            if (!visit[l[pos]]) {
                list[napid++] = l[pos] ;
                visit[l[pos]] = 1 ;
            }
        }

        if (found)
            break ;
    }

    pidtree_t apidt[graph.mlen] ;

    pares = ares ;
    pareslen = &areslen ;

    if (!napid) {
        r = 0 ;
        goto end ;
    }

    pidtree_init_array(list, napid, apidt, &graph, ares, areslen, info, requiredby, what) ;

    if (shut) {

        pid_t pid ;
        int wstat = 0 ;

        pid = fork() ;

        if (pid < 0)
            log_dieusys(LOG_EXIT_SYS,"fork") ;

        if (!pid) {

            all_redir_fd() ;

        } else {

            if (waitpid_nointr(pid,&wstat, 0) < 0)
                log_dieusys(LOG_EXIT_SYS,"wait for child") ;

            if (wstat)
                log_die(LOG_EXIT_SYS,"child fail") ;

            r = 0 ;

            goto end ;
        }
    }

    r = waitit(apidt, what, &graph, &deadline, info) ;

    end:

        if (shut) {

            while((fd = open("/dev/tty",O_RDWR|O_NOCTTY)) >= 0)
                if (fd >= 3)
                    break ;

            dup2 (fd,0) ;
            dup2 (fd,1) ;
            dup2 (fd,2) ;
            fd_close(fd) ;
        }

        graph_free_all(&graph) ;
        stralloc_free(&sa) ;
        tree_resolve_array_free(ares, areslen) ;

        return r ;
}
