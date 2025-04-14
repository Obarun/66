/*
 * tree_launch.c
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/string.h>
#include <oblibs/stack.h>
#include <oblibs/lexer.h>

#include <skalibs/djbunix.h>
#include <skalibs/tai.h>
#include <skalibs/iopause.h>
#include <skalibs/selfpipe.h>
#include <skalibs/tai.h>
#include <skalibs/sig.h>//sig_ignore
#include <skalibs/types.h>

#include <66/tree.h>
#include <66/state.h>
#include <66/resolve.h>
#include <66/ssexec.h>
#include <66/constants.h>

static uint32_t napid = 0 ;
static unsigned int npid = 0 ;
static uint8_t reloadmsg = 0 ;

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

static const unsigned char actions[3][7] = {
    // u                    U                       d                       D                       F                   b                   B
    { TREE_ACTION_WAIT,     TREE_ACTION_GOTIT,      TREE_ACTION_UNKNOWN,    TREE_ACTION_UNKNOWN,    TREE_ACTION_FATAL,  TREE_ACTION_WAIT,   TREE_ACTION_WAIT }, // !what -> up
    { TREE_ACTION_UNKNOWN,  TREE_ACTION_UNKNOWN,    TREE_ACTION_WAIT,       TREE_ACTION_GOTIT,      TREE_ACTION_FATAL,  TREE_ACTION_WAIT,   TREE_ACTION_WAIT }, // what -> down
    { TREE_ACTION_UNKNOWN,  TREE_ACTION_UNKNOWN,    TREE_ACTION_WAIT,       TREE_ACTION_GOTIT,      TREE_ACTION_FATAL,  TREE_ACTION_WAIT,   TREE_ACTION_WAIT } // what -> free

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

static inline void kill_all(pidtree_t *apidt)
{
    log_flow() ;

    unsigned int j = napid ;
    while (j--) kill(apidt[j].pid, SIGKILL) ;
}

static int check_action(pidtree_t *apidt, unsigned int pos, unsigned int receive, unsigned int what)
{
    unsigned int p = char2enum[receive] ;
    unsigned char action = actions[what][p] ;

    switch(action) {

        case TREE_ACTION_GOTIT:
            FLAGS_SET(apidt[pos].state, (!what ? TREE_FLAGS_UP : TREE_FLAGS_DOWN)) ;
            return 1 ;

        case TREE_ACTION_FATAL:
            FLAGS_SET(apidt[pos].state, TREE_FLAGS_FATAL) ;
            return -1 ;

        case TREE_ACTION_WAIT:
            return 0 ;

        case TREE_ACTION_UNKNOWN:
        default:
            log_die(LOG_EXIT_ZERO,"invalid action -- please make a bug report") ;
    }

}

static void notify(pidtree_t *apidt, unsigned int pos, char const *sig, unsigned int what)
{
    log_flow() ;

    uint32_t i = 0, idx = 0 ;
    char fmt[UINT_FMT] ;
    uint8_t flag = what ? TREE_FLAGS_DOWN : TREE_FLAGS_UP ;

    for (; i < apidt[pos].nnotif ; i++) {

        for (idx = 0 ; idx < napid ; idx++) {

            if (apidt[pos].notif[i]->index == apidt[idx].index && !FLAGS_ISSET(apidt[idx].state, flag))  {

                size_t nlen = uint_fmt(fmt, pos) ;
                fmt[nlen] = 0 ;
                size_t len = nlen + 1 + 2 ;
                char s[len + 1] ;
                auto_strings(s, fmt, ":", sig, "@") ;

                log_trace("sends notification ", sig, " to: ", apidt[idx].tres->sa.s + apidt[idx].tres->name, " from: ", apidt[pos].tres->sa.s + apidt[pos].tres->name) ;

                if (write(apidt[idx].pipe[1], s, strlen(s)) < 0)
                    log_dieusys(LOG_EXIT_SYS, "send notif to: ", apidt[idx].tres->sa.s + apidt[idx].tres->name) ;
            }
        }
    }
}

/**
 * @what: up or down
 * @success: 0 success, 1 fail
 * */
static void announce(unsigned int pos, pidtree_t *apidt, unsigned int what, unsigned int success, unsigned int exitcode)
{
    log_flow() ;

    char fmt[UINT_FMT] ;
    char const *treename = apidt[pos].tres->sa.s + apidt[pos].tres->name ;

    uint8_t flag = what ? TREE_FLAGS_DOWN : TREE_FLAGS_UP ;

    if (success) {

        fmt[uint_fmt(fmt, exitcode)] = 0 ;

        log_1_warnu(reloadmsg == 0 ? "start" : reloadmsg > 1 ? "unsupervise" : what == 0 ? "start" : "stop", " tree: ", treename, " -- exited with signal: ", fmt) ;

        notify(apidt, pos, "F", what) ;

        FLAGS_SET(apidt[pos].state, TREE_FLAGS_BLOCK|TREE_FLAGS_FATAL) ;

    } else {

        log_info("Successfully ", reloadmsg == 0 ? "started" : reloadmsg > 1 ? "unsupervised" : what == 0 ? "started" : "stopped", " tree: ", treename) ;

        notify(apidt, pos, what ? "D" : "U", what) ;

        FLAGS_CLEAR(apidt[pos].state, TREE_FLAGS_BLOCK) ;
        FLAGS_SET(apidt[pos].state, flag|TREE_FLAGS_UNBLOCK) ;

    }

}

static int handle_signal(pidtree_t *apidt, unsigned int what)
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

                    uint32_t pos = 0 ;
                    int wstat ;
                    pid_t r = wait_nohang(&wstat) ;

                    if (r < 0) {

                        if (errno == ECHILD)
                            break ;
                        else
                            log_dieusys(LOG_EXIT_SYS,"wait for children") ;

                    } else if (!r) break ;

                    for (; pos < napid ; pos++)
                        if (apidt[pos].pid == r)
                            break ;

                    if (pos < napid) {

                        if (!WIFSIGNALED(wstat) && !WEXITSTATUS(wstat)) {

                            announce(pos, apidt, what, 0, 0) ;
                            npid-- ;

                        } else {

                            ok = WIFSIGNALED(wstat) ? WTERMSIG(wstat) : WEXITSTATUS(wstat) ;
                            announce(pos, apidt, what, 1, ok) ;
                            npid-- ;
                            kill_all(apidt) ;
                            break ;
                        }
                    }
                }
                break ;
            case SIGTERM :
            case SIGKILL :
            case SIGINT :
                    log_1_warn("aborting transaction") ;
                    kill_all(apidt) ;
                    ok = 111 ;
                    break ;
            default : log_die(LOG_EXIT_SYS, "unexpected data in selfpipe") ;
        }
    }

    return ok ;
}

static int ssexec_callback(stack *stk, ssexec_t *info, unsigned int what)
{
    log_flow() ;

    int r, e = 1 ;
    size_t pos = 0, len = stk->len ;
    ss_state_t ste = STATE_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;
    _alloc_stk_(t, stk->len) ;


    /** only deal with enabled service at up time and
     * supervised service at down time */
    FOREACH_STK(stk, pos) {

        char *name = stk->s + pos ;

        r = resolve_read_g(wres, info->base.s, name) ;
        if (r == -1)
            log_dieu(LOG_EXIT_SYS, "read resolve file of: ", name) ;
        if (!r)
            log_dieu(LOG_EXIT_SYS, "read resolve file of: ", name, " -- please make a bug report") ;

        if (!state_read(&ste, &res))
            log_dieu(LOG_EXIT_SYS, "read state file of: ", name, " -- please make a bug report") ;

        if (!what ? res.enabled : ste.issupervised == STATE_FLAGS_TRUE && !res.earlier) {

            if (get_rstrlen_until(name, SS_LOG_SUFFIX) < 0 && !res.inns)
                if (!stack_add_g(&t, name))
                    log_dieu(LOG_EXIT_SYS, "add string") ;
        }
    }

    resolve_free(wres) ;

    if (!t.len)
        return 0 ;

    pos = 0, len = t.count ;

    int n = what == 2 ? 2 : 1 ;
    int nargc = n + len ;
    char const *prog = PROG ;
    char const *newargv[nargc + 1] ;
    unsigned int m = 0 ;

    newargv[m++] = "tree" ;
    if (what == 2)
        newargv[m++] = "-u" ;

    {
        FOREACH_STK(&t, pos)
            newargv[m++] = t.s + pos ;
    }

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

    return e ;
}

static int doit(pidtree_t apid, ssexec_t *info, unsigned int what, tain *deadline)
{
    log_flow() ;

    int r ;
    const char *treename = apid.tres->sa.s + apid.tres->name ;

    info->treename.len = 0 ;
    info->opt_tree = 1 ;

    if (!auto_stra(&info->treename, treename))
        log_die_nomem("stralloc") ;

    r = tree_sethome(info) ;
    if (r <= 0)
        log_warnu_return(LOG_EXIT_ONE, "find tree: ", info->treename.s) ;

    if (!tree_get_permissions(info->base.s, info->treename.s))
        log_warn_return(LOG_EXIT_ONE, "You're not allowed to use the tree: ", info->treename.s) ;

    if (!apid.tres->ncontents) {

        log_info("Empty tree: ", info->treename.s, " -- nothing to do") ;

    } else {

        _alloc_stk_(stk, strlen(apid.tres->sa.s + apid.tres->contents) + 1) ;

        if (!stack_string_clean(&stk, apid.tres->sa.s + apid.tres->contents))
            log_warn_return(LOG_EXIT_ONE, "clean string") ;

        return ssexec_callback(&stk, info, what) ;
    }

    return 0 ;
}

static int async_deps(pidtree_t *apidt, unsigned int i, unsigned int what, tain *deadline)
{
    log_flow() ;

    int r ;
    unsigned int pos = 0, id = 0, idx = 0 ;
    char buf[(UINT_FMT*2)*SS_MAX_SERVICE + 1] ;

    tain dead ;
    tain_now_set_stopwatch_g() ;
    tain_add_g(&dead, deadline) ;

    iopause_fd x = { .fd = apidt[i].pipe[0], .events = IOPAUSE_READ, 0 } ;

    unsigned int n = apidt[i].nedge ;
    unsigned int visit[n + 1] ;

    memset(visit, 0, (n + 1) * sizeof (unsigned int));

    log_trace("waiting dependencies for: ", apidt[i].tres->sa.s + apidt[i].tres->name) ;

    while (pos < n) {

        r = iopause_g(&x, 1, &dead) ;

        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "iopause") ;

        if (!r) {
            errno = ETIMEDOUT ;
            log_dieusys(LOG_EXIT_SYS,"time out", apidt[i].tres->sa.s + apidt[i].tres->name) ;
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
                 *      apids_array_id:signal_receive
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

                log_trace(apidt[i].tres->sa.s + apidt[i].tres->name, " acknowledges: ", pc, " from: ", apidt[id].tres->sa.s + apidt[id].tres->name) ;

                if (!visit[pos]) {

                    id = check_action(apidt, id, c, what) ;
                    if (id < 0)
                        log_die(LOG_EXIT_SYS, "tree dependency: ", apidt[id].tres->sa.s + apidt[id].tres->name, " of: ", apidt[id].tres->sa.s + apidt[id].tres->name," crashed") ;

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

static int async(pidtree_t *apidt, unsigned int i, unsigned int what, ssexec_t *info, tain *deadline)
{
    log_flow() ;

    int e = 0 ;
    ssexec_t sinfo = SSEXEC_ZERO ;
    char *name = apidt[i].tres->sa.s + apidt[i].tres->name ;

    ssexec_copy(&sinfo, info) ;

    log_trace("beginning of the process of tree: ", name) ;
    if (FLAGS_ISSET(apidt[i].state, (!what ? TREE_FLAGS_DOWN : TREE_FLAGS_UP)) ||
        /** force to pass through unsupersive process even
         * if the tree is marked down */
        FLAGS_ISSET(apidt[i].state, (what ? TREE_FLAGS_DOWN : TREE_FLAGS_UP)) && what == 2) {

        if (!FLAGS_ISSET(apidt[i].state, TREE_FLAGS_BLOCK)) {

            FLAGS_SET(apidt[i].state, TREE_FLAGS_BLOCK) ;

            if (apidt[i].nedge) {
                if (!async_deps(apidt, i, what, deadline)) {
                    ssexec_free(&sinfo) ;
                    log_warnu_return(LOG_EXIT_ZERO, !what ? "start" : "stop", " dependencies of tree: ", name) ;
                }
            }

            e = doit(apidt[i], &sinfo, what, deadline) ;

        } else {

            log_trace("skipping tree: ", name, " -- already in ", what ? "stopping" : "starting", " process") ;

            notify(apidt, i, what ? "d" : "u", what) ;
        }

    } else {

        /** do not notify here, the handle will make it for us */
        log_trace("skipping tree: ", name, " -- already ", what ? "down" : "up") ;

    }

    ssexec_free(&sinfo) ;

    return e ;
}

int tree_launch(pidtree_t *apidt, uint32_t ntree, unsigned int what, tain *deadline, ssexec_t *info)
{
    log_flow() ;

    uint32_t pos = 0, e = 0 ;
    int r ;
    pid_t pid ;
    pidtree_t apidtreetable[ntree] ;
    pidtree_t_ref apidtree = apidtreetable ;

    tain_now_set_stopwatch_g() ;
    tain_add_g(deadline, deadline) ;

    npid = 0 ;
    napid = ntree ;
    reloadmsg = what ;

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

            e = async(apidtree, pos, what, info, deadline) ;

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
            e = handle_signal(apidtree, what) ;

            if (e)
                break ;
        }
    }

    selfpipe_finish() ;

    for (pos = 0 ; pos < napid ; pos++) {
        close(apidtree[pos].pipe[1]) ;
        close(apidtree[pos].pipe[0]) ;
    }

    end:
        return e ;
}
