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
#include <unistd.h> // pipe, close
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/graph.h>
#include <oblibs/io.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>
#include <oblibs/sbl.h>
#include <oblibs/types.h>
#include <oblibs/sse.h>
#include <oblibs/fd.h>

#include <66/resolve.h>
#include <66/tree.h>
#include <66/service.h>
#include <66/state.h>
#include <66/constants.h>
#include <66/config.h> // SS_MAX_SERVICE
#include <66/ssexec.h>

// Internal event types for coordination
enum tree_event_type_e
{
    TREE_EVENT_CHILD_SUCCESS,
    TREE_EVENT_CHILD_FAILED,
    TREE_EVENT_TIMEOUT,
    TREE_EVENT_SHUTDOWN_REQUEST
} ;
typedef enum tree_event_type_e tree_event_type_t ;

// Internal coordination message
struct tree_event_msg_s {
    tree_event_type_t type ; // type of the event
    uint32_t id ; // id of the service
    uint64_t timestamp ; // For debugging/logging
} ;
typedef struct tree_event_msg_s tree_event_msg_t ;

static tree_manager_t *pmanager ;
static uint32_t npid = 0 ;
static uint32_t *v2tree ;

// prototype
static int launch_tree(uint32_t id) ;
static void child_cb(sse_watcher_t *w, void *cbdata, int event) ;
static void timeout_cb(sse_watcher_t *w, void *cbdata, int event) ;

// helpers
static uint32_t get_atree_id(vertex_t *v)
{
    log_flow() ;
    uint32_t id = v->index ;
    return v2tree[id] ;
}

static bool deps_satisfied(uint32_t id)
{
    log_flow() ;

    uint32_t pos = 0 ;
    tree_ctx_t *tree = &pmanager->atree[id];
    int flag = !pmanager->operation ? TREE_FLAGS_UP : TREE_FLAGS_DOWN ;

    for (; pos < tree->ndepends ; pos++) {

        uint32_t did = get_atree_id(tree->depends[pos]) ;
        tree_ctx_t *dep = &pmanager->atree[did] ;

        if (!FLAGS_ISSET(dep->state, flag))
            return false ;
    }

    return true ;
}

static void wait_deps(uint32_t id)
{
    log_flow() ;

    uint32_t pos = 0 ;
    tree_ctx_t *tree = &pmanager->atree[id];

    // Check all dependents of this service
    for (; pos < tree->nrequiredby ; pos++) {

        uint32_t did = get_atree_id(tree->requiredby[pos]) ;
        tree_ctx_t *dep = &pmanager->atree[did] ;

        if (dep->state == TREE_FLAGS_WAITING_DEPS && deps_satisfied(did))
            launch_tree(did) ;
    }
}

static inline int tree_send_event(tree_event_type_t type, uint32_t id)
{
    log_flow() ;

    tree_event_msg_t msg = {
        .type = type,
        .id = id,
        .timestamp = time(NULL)
    } ;

    ssize_t written = io_write(pmanager->notifd[1], (char *)&msg, sizeof(msg)) ;
    return (written == sizeof(msg)) ? 1 : 0 ;
}

// state = true > success
static void announce(uint32_t id, bool success)
{
    log_flow() ;

    tree_ctx_t *tree = &pmanager->atree[id] ;
    char const *treename = tree->tres->sa.s + tree->tres->name ;

    if (success) {

        log_info("Successfully executed ", pmanager->cmdmsg, " on tree: ", treename) ;

        tree_send_event(TREE_EVENT_CHILD_SUCCESS, id) ;

    } else {

        flog_1_warnu("%s tree: %s -- exited with signal: %d", pmanager->cmdmsg, treename, tree->exitcode) ;

        tree_send_event(TREE_EVENT_CHILD_FAILED, id) ;
    }

}

static int ssexec_callback(tree_ctx_t *tree, uint32_t id, strbuf *stk, ssexec_t *info)
{
    log_flow() ;

    int r ;
    size_t pos = 0, len = stk->len ;
    ss_state_t ste = STATE_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;
    _alloc_sbl_(t, stk->len) ;


    /** only deal with enabled service at up time and
     * supervised service at down time */
    {
        FOREACH_SBL(stk, pos) {

            char *name = stk->s + pos ;

            r = resolve_read(wres, info->base.s, name) ;
            if (r == -1)
                log_dieu(LOG_EXIT_SYS, "read resolve file of: ", name) ;
            if (!r)
                log_dieu(LOG_EXIT_SYS, "read resolve file of: ", name, " -- please make a bug report") ;

            if (!state_read(&ste, &res))
                log_dieu(LOG_EXIT_SYS, "read state file of: ", name, " -- please make a bug report") ;

            if (!pmanager->operation ? res.enabled : ste.issupervised == STATE_FLAGS_TRUE && !res.earlier) {

                if (get_rstrlen_until(name, SS_LOG_SUFFIX) < 0 && !res.inns)
                    if (!sbl_add(&t, name))
                        log_dieu(LOG_EXIT_SYS, "add string") ;
            }
        }
    }

    resolve_free(wres) ;

    if (!t.len)
        return 0 ;

    pos = 0, len = sbl_count(&t) ;

    int n = pmanager->operation == 2 ? 3 : 2 ;
    int nargc = n + len ;
    char const *newargv[nargc] ;
    unsigned int m = 0 ;

    newargv[m++] = !pmanager->operation ? "start" : "stop" ;
    if (pmanager->operation == 2)
        newargv[m++] = "-u" ;

    FOREACH_SBL(&t, pos)
        newargv[m++] = t.s + pos ;

    newargv[m] = 0 ;

    log_trace("sending ", newargv[0], " command to service of tree: ", tree->tres->sa.s + tree->tres->name) ;

    /* fork (not exec): the child inherits info -- and thus who -- in memory, and
     * runs the start/stop command directly. Preserving the caller's ssexec_t is
     * the whole point (a spawned "66 start" would restart from SSEXEC_ZERO). */
    tree->pid = fork() ;
    if (tree->pid < 0) {
        FLAGS_SET(tree->state, TREE_FLAGS_FAILED) ;
        log_warnusys_return(LOG_EXIT_ZERO, newargv[0], " services of tree: ", tree->tres->sa.s + tree->tres->name) ;
    }

    if (!tree->pid) {

        /* tear down the inherited tree-manager loop before svc_launch builds its
         * own: close the epoll/notifier fds and, critically, reset the process-wide
         * lx_signalfd static state (else svc_launch's sse_start_signal reuses this
         * signalfd) while unblocking the inherited signal mask. */
        sse_free_signal(&pmanager->signalfd) ;
        close(pmanager->loop.fd) ;
        close(pmanager->notifd[0]) ;
        close(pmanager->notifd[1]) ;

        opt_cmd_t const *cmd = !pmanager->operation ? &cmd_start : &cmd_stop ;
        _exit(opt_dispatch(m, newargv, cmd, info)) ;
    }

    if (!sse_start_child(&pmanager->loop, &tree->child, child_cb, (void*)(uintptr_t)id, tree->pid, 2, true)) {
        if (tree->pid)
            kill(tree->pid, SIGKILL);
        tree->state = TREE_FLAGS_FAILED ;
        log_warnusys_return(LOG_EXIT_ZERO, "start child watcher for tree: ", tree->tres->sa.s + tree->tres->name) ;
    }

    if (pmanager->timeout) {
        if (!sse_start_timer(&pmanager->loop, &tree->timeout, timeout_cb, (void*)(uintptr_t)id, pmanager->timeout, 0, 1))
            log_warnusys_return(LOG_EXIT_ZERO, "start timer watcher for tree: ",  tree->tres->sa.s + tree->tres->name) ;
    }

    return 1 ;
}

static int launch_tree(uint32_t id)
{
    log_flow() ;

    tree_ctx_t *tree = &pmanager->atree[id] ;
    ssexec_t sinfo = SSEXEC_ZERO ;
    ssexec_copy(&sinfo, pmanager->info) ;

    int r ;
    const char *treename = tree->tres->sa.s + tree->tres->name ;

    sinfo.treename.len = 0 ;
    sinfo.opt_tree = 1 ;

    if (!auto_strbuf(&sinfo.treename, treename))
        log_die_nomem("strbuf") ;

    r = tree_sethome(&sinfo) ;
    if (r <= 0)
        log_warnu_return(LOG_EXIT_ONE, "find tree: ", sinfo.treename.s) ;

    if (!tree_get_permissions(sinfo.base.s, sinfo.treename.s))
        log_warn_return(LOG_EXIT_ONE, "You're not allowed to use the tree: ", sinfo.treename.s) ;

    if (!tree->tres->ncontents) {

        log_info("Empty tree: ", sinfo.treename.s, " -- nothing to do") ;
        ssexec_free(&sinfo) ;
        npid-- ;
        announce(id, true) ;
        return 1 ;
    }

    _alloc_sbl_(stk, strlen(tree->tres->sa.s + tree->tres->contents) + 1) ;

    if (!sbl_clean_string(&stk, tree->tres->sa.s + tree->tres->contents))
        log_warn_return(LOG_EXIT_ONE, "clean string") ;

    r = ssexec_callback(tree, id, &stk, &sinfo) ;
    ssexec_free(&sinfo) ;

    if (!r) {
        npid-- ;
        announce(id, true) ;
        return 1 ;
    }

    return r ;
}

// callback
static void signalfd_cb(sse_watcher_t *w, void *cbdata, int event)
{
    log_flow() ;

    (void)cbdata ;

    // Check for watcher errors first
    if (w->api_errno != 0) {
        log_warn("signalfd watcher error: ", strerror(w->api_errno)) ;
        sse_free_signal(w) ;
        pmanager->loop.running = false ;
        return ;
    }

    if (!(event & SSE_READ)) {
        log_warn("unexpected event on signalfd callback") ;
        sse_free_signal(w) ;
        pmanager->loop.running = false ;
        return ;
    }

    sse_signal_t *s = (sse_signal_t *)w->sdata;
    if (!s) {
        log_warn("signalfd sdata is NULL") ;
        sse_free_signal(w) ;
        pmanager->loop.running = false ;
        return ;
    }

    switch (s->si.ssi_signo) {

        case SIGTERM :
        case SIGKILL :
        case SIGINT :
            log_1_warn("received SIGTERM or SIGKILL or SIGINT, aborting transaction") ;
            tree_send_event(TREE_EVENT_SHUTDOWN_REQUEST, 0) ;
            break ;
        default :
            log_die(LOG_EXIT_SYS, "unexpected signal") ;
    }
}

static void deadline_cb(sse_watcher_t *w, void *cbdata, int event)
{
    log_flow() ;

    (void)cbdata ;
    (void)event ;

    // Check for watcher errors
    if (w->api_errno != 0) {
        log_warn("deadline watcher error: ", strerror(w->api_errno)) ;
        // Still trigger shutdown, then free
        tree_send_event(TREE_EVENT_SHUTDOWN_REQUEST, 0) ;
        sse_free_timer(w) ;
        return ;
    }

    log_warn("global deadline reached, shutting down") ;
    tree_send_event(TREE_EVENT_SHUTDOWN_REQUEST, 0) ;
    // one-shot timer
    sse_free_timer(w) ;
}

static void notifier_cb(sse_watcher_t *w, void *cbdata, int event)
{
    log_flow() ;

    (void)cbdata ;
    tree_event_msg_t msg ;
    ssize_t n ;

    // Check for watcher errors
    if (w->api_errno != 0) {
        log_warn("notifier watcher error: ", strerror(w->api_errno)) ;
        pmanager->loop.running = false ;
        // simply return and let cleanup handle it
        return ;
    }

    if (!(event & SSE_READ)) {
        log_warn("unexpected event on notifier callback") ;
        pmanager->loop.running = false ;
        return ;
    }

    while ((n = io_read(pmanager->notifd[0], (char *)&msg, sizeof(msg))) == sizeof(msg)) {

        if (msg.id >= pmanager->ntree) {
            log_warn("invalid tree id in notification") ;
            continue ;
        }

        tree_ctx_t *tree = &pmanager->atree[msg.id] ;

        switch (msg.type) {

            case TREE_EVENT_CHILD_SUCCESS:
                tree->state = 0 ;
                FLAGS_SET(tree->state, !pmanager->operation ? TREE_FLAGS_UP : TREE_FLAGS_DOWN) ;
                wait_deps(msg.id) ;
                break ;

            case TREE_EVENT_CHILD_FAILED:
                tree->state = 0 ;
                FLAGS_SET(tree->state, TREE_FLAGS_FAILED) ;
                break ;

            case TREE_EVENT_TIMEOUT:
                tree->state = 0 ;
                FLAGS_SET(tree->state, TREE_FLAGS_TIMEOUT) ;
                break ;

            case TREE_EVENT_SHUTDOWN_REQUEST:
                pmanager->loop.running = false ;
                break ;

            default:
                log_warn("unexpected event type") ;
                break ;
        }

        // Check if we're done
        if (!npid)
            pmanager->loop.running = false ;
    }

    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        log_warnusys("read from notifier pipe");
        pmanager->loop.running = false;
    }
}

static void child_cb(sse_watcher_t *w, void *cbdata, int event)
{
    log_flow() ;

    uint32_t id = (uint32_t)(uintptr_t)cbdata;
    tree_ctx_t *tree = &pmanager->atree[id];

    if (w->api_errno != 0) {
        log_warn("child watcher error: ", strerror(w->api_errno)) ;
        npid--;
        tree->state = TREE_FLAGS_FAILED ;
        announce(id, false) ;
        sse_free_child(w) ;
        return ;
    }

    if (!(event & SSE_READ)) {
        log_warn("unexpected event on child callback") ;
        return ;
    }

    if (event & (SSE_HUP | SSE_READ)) {

        sse_child_t *data = (sse_child_t *)w->sdata;
        if (!data) {
            log_warn("child watcher sdata is NULL") ;
            sse_free_child(w) ;
            return ;
        }

        int wstat = data->status ;
        npid-- ;

        tree->exitcode = WEXITSTATUS(wstat) ;

        bool success = !WIFSIGNALED(wstat) && !WEXITSTATUS(wstat) ;
        announce(id, success) ;

        if (tree->timeout.fd > 0)
            sse_free_timer(&tree->timeout) ;

        sse_free_child(&tree->child) ;
    }
}

static void timeout_cb(sse_watcher_t *w, void *cbdata, int event)
{
    log_flow() ;

    uint32_t id = (uint32_t)(uintptr_t)cbdata ;
    tree_ctx_t *tree = &pmanager->atree[id] ;
    (void)event ;
    if (w->api_errno != 0) {
        log_warn("timeout watcher error: ", strerror(w->api_errno)) ;
        sse_free_timer(w) ;
        return ;
    }

    if (!tree && tree->pid > 0) {
        // Kill the service
        log_warn("tree timeout, killing: ", tree->tres->sa.s + tree->tres->name) ;
        kill(tree->pid, SIGTERM) ;

        tree_send_event(TREE_EVENT_TIMEOUT, id) ;
    }

    // one-shot timer
    sse_free_timer(w) ;
}

// main API
static int tree_manager_init(tree_ctx_t *atree, uint32_t ntree, uint8_t operation, ssexec_t *info)
{
    log_flow() ;

    if (!atree || ntree == 0) {
        errno = EINVAL ;
        log_dieusys(LOG_EXIT_SYS, "bad parameter") ;
    }

    pmanager->atree = atree ;
    pmanager->ntree = ntree ;
    pmanager->shutdown_requested = false ;
    pmanager->info = info ;
    pmanager->timeout = (uint64_t)info->timeout ;
    pmanager->operation = operation ;
    pmanager->cmdmsg = operation > 1 ? "unsupervise" : !operation ? "start" : "stop" ;

    // Initialize SSE event loop
    if (!sse_new(&pmanager->loop, ntree))
        log_dieusys(LOG_EXIT_SYS, "initiate events loop") ;

    pmanager->loop.running = true ;

    // general signal
    if (!sse_start_signal(&pmanager->loop, &pmanager->signalfd, signalfd_cb, NULL, 0))
        log_dieusys(LOG_EXIT_SYS, "start signal watcher") ;

    if (!sse_attach_signal(&pmanager->signalfd, SIGINT))
        log_dieusys(LOG_EXIT_SYS, "block signal SIGINT") ;

    if (!sse_attach_signal(&pmanager->signalfd, SIGKILL))
        log_dieusys(LOG_EXIT_SYS, "block signal SIGKILL") ;

    if (!sse_attach_signal(&pmanager->signalfd, SIGTERM))
        log_dieusys(LOG_EXIT_SYS, "block signal SIGTERM") ;

    if (!sse_ignore_signal(&pmanager->signalfd, SIGPIPE))
        log_dieusys(LOG_EXIT_SYS, "ignore signal SIGPIPE") ;

    // Create notifier pipe
    if (pipe(pmanager->notifd) < 0)
        log_dieusys(LOG_EXIT_SYS, "create notifier pipe") ;

    // Set pipes non-blocking
    if (!io_set_nonblock(pmanager->notifd[0]) || !io_set_nonblock(pmanager->notifd[1]))
        log_dieusys(LOG_EXIT_SYS, "set none blocking notifier pipe") ;

    // Setup notifier pipe watcher
    if (!sse_start_io(&pmanager->loop, &pmanager->notifier, notifier_cb, NULL, pmanager->notifd[0], SSE_READ, 0))
        log_dieusys(LOG_EXIT_SYS, "start notifier watcher") ;

    // Setup global deadline timer if timeout specified
    if (pmanager->timeout) {
        if (!sse_start_timer(&pmanager->loop, &pmanager->deadline, deadline_cb, NULL, pmanager->timeout, 0, 0))
            log_dieusys(LOG_EXIT_SYS, "start deadline watcher") ;
    }

    return 1;
}

static int tree_manager_start(void)
{
    log_flow() ;

    uint32_t pos = 0 ;

    for (; pos < pmanager->ntree ; pos++) {

        tree_ctx_t *tree = &pmanager->atree[pos] ;

        FLAGS_SET(tree->target_state, TREE_FLAGS_UP) ;

        if (FLAGS_ISSET(tree->state, TREE_FLAGS_UP)) {
            log_warn("skipping already up tree: ", tree->tres->sa.s + tree->tres->name) ;
            continue ;
        }

        // Skip if service is already running
        if (FLAGS_ISSET(tree->state, TREE_FLAGS_STARTING | TREE_FLAGS_PROCESSING)) {
            log_warn("skipping already processing tree: ", tree->tres->sa.s + tree->tres->name) ;
            continue ;
        }

        npid++ ;
        // Check if we can start this service now
        if (deps_satisfied(pos)) {

            tree->state = TREE_FLAGS_STARTING ;
            log_trace("initiate start process for tree: ", tree->tres->sa.s + tree->tres->name) ;

            if (!launch_tree(pos)) {

                log_warn("failed to start tree: ", tree->tres->sa.s + tree->tres->name);
                tree->state = TREE_FLAGS_FAILED ;
                if (tree->pid)
                    kill(tree->pid, SIGKILL) ;
            }

        } else {
            // Mark as waiting for dependencies
            tree->state = TREE_FLAGS_WAITING_DEPS ;
            log_trace("tree waiting for dependencies: ", tree->tres->sa.s + tree->tres->name) ;
        }
    }

    return 1 ;
}

static int tree_manager_stop(void)
{
    log_flow() ;

    uint32_t pos = 0 ;

    for (; pos < pmanager->ntree ; pos++) {

        tree_ctx_t *tree = &pmanager->atree[pos] ;

        FLAGS_SET(tree->target_state, TREE_FLAGS_DOWN) ;

        if (FLAGS_ISSET(tree->state, TREE_FLAGS_DOWN)) {
            log_warn("skipping already down tree: ", tree->tres->sa.s + tree->tres->name) ;
            continue ;
        }

        if (FLAGS_ISSET(tree->state, TREE_FLAGS_STOPPING | TREE_FLAGS_PROCESSING)) {
            log_warn("skipping already processing tree: ", tree->tres->sa.s + tree->tres->name) ;
            continue ;
        }

        npid++ ;

        /* Check if we can start this service now */
        if (deps_satisfied(pos)) {

            tree->state = TREE_FLAGS_STOPPING ;
            log_trace("initiate stop process for tree: ", tree->tres->sa.s + tree->tres->name) ;

            if (!launch_tree(pos)) {

                log_warn("failed to stop tree: ", tree->tres->sa.s + tree->tres->name) ;
                tree->state = TREE_FLAGS_FAILED ;
                if (tree->pid)
                    kill(tree->pid, SIGKILL) ;
                tree->pid = 0 ;
            }

        } else {
            // Mark as waiting for dependencies
            tree->state = TREE_FLAGS_WAITING_DEPS ;
            log_trace("tree waiting for dependencies: ", tree->tres->sa.s + tree->tres->name) ;
        }
    }

    return 1 ;
}

static int tree_manager_run(void)
{
    log_flow() ;

    pmanager->loop.running = true ;

    while (pmanager->loop.running && npid) {

        if (!sse_run(&pmanager->loop, SSE_TIMEOUT_INFINITE))
            return 0 ;

        // Check if all services have reached their target states
        if (!npid)
            pmanager->loop.running = false ;
    }

    return 1 ;
}

static void tree_manager_free(void)
{
    log_flow() ;

    pmanager->loop.running = false ;

    sse_free(&pmanager->loop) ;

    if (pmanager->notifd[0])
        close_fd(pmanager->notifd[0]) ;
    if (pmanager->notifd[1])
        close_fd(pmanager->notifd[1]) ;
}

int tree_launch(tree_ctx_t *atree, uint32_t ntree, uint8_t operation, ssexec_t *info)
{
    log_flow() ;

    tree_manager_t manager ;
    pmanager = &manager ;

    uint32_t vertex_to_atree[SS_MAX_SERVICE] ;

    npid = 0 ;

    if (!tree_manager_init(atree, ntree, operation, info))
        log_dieusys(LOG_EXIT_SYS, "initiate manager") ;

    // table mapping for depends array
    for (uint32_t pos = 0 ; pos < ntree ; pos++){

        uint32_t idx = atree[pos].index ;
        if (idx >= SS_MAX_SERVICE)
            log_dieusys(LOG_EXIT_SYS, "build correspondence table for dependencies") ;

        vertex_to_atree[idx] = pos ;
    }

    v2tree = vertex_to_atree ;

    int result ;
    if (!operation) {
        result = tree_manager_start() ;
    } else {
        result = tree_manager_stop() ;
    }

    if (result) {
        result = tree_manager_run() ;
    }

    tree_manager_free() ;
    return !result ? 1 : 0 ;
}
