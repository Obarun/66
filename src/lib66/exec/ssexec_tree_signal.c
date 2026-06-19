/*
 * ssexec_tree_signal.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>
#include <oblibs/types.h>
#include <oblibs/io.h>
#include <oblibs/fd.h>

#include <66/config.h>
#include <66/ssexec.h>
#include <66/tree.h>
#include <66/svc.h>
#include <66/graph.h>

static inline unsigned int lookup (char const *const *table, char const *signal)
{
    unsigned int i = 0 ;
    for (; table[i] ; i++) if (!strcmp(signal, table[i])) break ;
    return i ;
}

static inline unsigned int parse_signal (char const *signal)
{
    log_flow() ;

    static char const *const signal_table[] = {
        "start",
        "stop",
        "free",
        0
    } ;
    unsigned int i = lookup(signal_table, signal) ;
    if (!signal_table[i]) log_die(LOG_EXIT_USER, "unknown tree signal: ", signal) ;
    return i ;
}

static void all_redir_fd(void)
{
    log_flow();

    int fd = io_open_mode("/var/log/shutdown.log", O_WRONLY | O_CREAT | O_APPEND, 0644) ;
    if (fd < 0)
        log_dieusys(LOG_EXIT_SYS, "open /var/log/shutdown.log") ;

    if (copy_fd(1, fd) < 0)
        log_dieusys(LOG_EXIT_SYS, "dup2 stdout") ;

    if (copy_fd(2, fd) < 0)
        log_dieusys(LOG_EXIT_SYS, "dup2 stderr") ;

    int null_fd = io_open("/dev/null", O_RDONLY) ;
    if (null_fd >= 0)
        move_fd(0, null_fd) ; // stdin -> /dev/null (closes null_fd)

    if (fd > 2)
        close_fd(fd) ;

    if (setsid() < 0)
        log_dieusys(LOG_EXIT_SYS, "setsid");

    if (chdir("/") < 0)
        log_dieusys(LOG_EXIT_SYS, "chdir");

    umask(022);
}


/** The sub-command name selects the signal. The thin entries
 * (do_tree_start/stop/free) post it here; the body reads it. */
static char const *tree_signal_name = 0 ;
static uint8_t tree_signal_fork = 0 ;

int on_tree_signal(int id, char const *arg, void *data)
{
    (void)arg ;
    (void)data ;

    switch (id) {
        case 'f' : tree_signal_fork = 1 ; break ;
    }

    return 0 ;
}

/** Select the signal from the sub-command name, then run the handler. The leaf
 * options (-f) have already been scanned by opt_dispatch at the sub-node. */

int do_tree_start(int argc, char const *const *argv, void *data)
{
    tree_signal_name = "start" ;
    return ssexec_tree_signal(argc, argv, data) ;
}

int do_tree_stop(int argc, char const *const *argv, void *data)
{
    tree_signal_name = "stop" ;
    return ssexec_tree_signal(argc, argv, data) ;
}

int do_tree_free(int argc, char const *const *argv, void *data)
{
    tree_signal_name = "free" ;
    return ssexec_tree_signal(argc, argv, data) ;
}

int ssexec_tree_signal(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    /* drain option state into locals, then reset the statics for re-entrancy. */
    uint8_t dofork = tree_signal_fork ;
    char const *signame = tree_signal_name ;
    tree_signal_fork = 0 ;
    tree_signal_name = 0 ;

    int r, shut = 0 ;
    uint8_t what = 0, requiredby = 0 ;
    tree_graph_t graph = GRAPH_TREE_ZERO ;
    uint32_t flag = 0, ntree = 0 ;

    shut = dofork ;

    what = parse_signal(signame) ;

    info->treename.len = 0 ;

    if (argc >= 1) {
        if (!auto_strbuf(&info->treename, argv[0]))
            log_die_nomem("strbuf") ;
    }

    if (what) {
        requiredby = 1 ;
        FLAGS_SET(flag, GRAPH_WANT_REQUIREDBY) ;
    } else {
        FLAGS_SET(flag, GRAPH_WANT_DEPENDS) ;
    }

    if ((svc_scandir_ok(info->scandir.s)) <= 0)
        log_die(LOG_EXIT_SYS,"scandir: ", info->scandir.s," is not running") ;

    if (!tree_graph_new(&graph, (uint32_t)SS_MAX_SERVICE))
        log_dieusys(LOG_EXIT_SYS, "initiate the graph") ;

    /** only one tree */

    if (info->treename.len) {

        ntree = tree_graph_build_name(&graph, info->treename.s, info, flag) ;
        if (!ntree) {
            if (errno == EINVAL)
                log_dieusys(LOG_EXIT_USER, "build the graph") ;

            log_die(LOG_EXIT_USER, "find tree: ", info->treename.s) ;
        }

    } else {

        FLAGS_SET(flag, GRAPH_WANT_ENABLED) ;
        ntree = tree_graph_build_master(&graph, info, flag) ;

        if (!ntree) {
            if (errno == EINVAL)
                log_dieusys(LOG_EXIT_USER, "build the graph") ;

            log_die(LOG_EXIT_USER, "trees selection is not created -- creates at least one tree") ;
        }

    }

    tree_ctx_t atree[graph.g.nsort] ;

    tree_init_ctx(atree, &graph, requiredby, flag) ;

    if (shut) {

        pid_t pid = fork() ;

        if (pid < 0)
            log_dieusys(LOG_EXIT_SYS,"fork") ;

        if (!pid) {
            all_redir_fd() ;
        } else {
            tree_graph_destroy(&graph) ;
            return 0 ;
        }
    }

    r = tree_launch(atree, graph.g.nsort, what, info) ;

    tree_graph_destroy(&graph) ;

    return r ;
}
