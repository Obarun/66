/*
 * ssexec_tree_signal.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

// #include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>

#include <skalibs/sgetopt.h>
#include <skalibs/tai.h>

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

static inline unsigned int parse_signal (char const *signal, ssexec_t *info)
{
    log_flow() ;

    static char const *const signal_table[] = {
        "start",
        "stop",
        "free",
        0
    } ;
    unsigned int i = lookup(signal_table, signal) ;
    if (!signal_table[i]) log_usage(info->usage, "\n", info->help) ;
    return i ;
}

static void all_redir_fd(void)
{
    log_flow();

    int fd = open("/var/log/shutdown.log", O_WRONLY | O_CREAT | O_APPEND, 0644) ;
    if (fd < 0)
        log_dieusys(LOG_EXIT_SYS, "open /var/log/shutdown.log") ;

    if (dup2(fd, 1) < 0)
        log_dieusys(LOG_EXIT_SYS, "dup2 stdout") ;

    if (dup2(fd, 2) < 0)
        log_dieusys(LOG_EXIT_SYS, "dup2 stderr") ;

    int null_fd = open("/dev/null", O_RDONLY) ;
    if (null_fd >= 0) {
        dup2(null_fd, 0) ; // stdin
        close(null_fd) ;
    }

    if (fd > 2)
        close(fd);

    if (setsid() < 0)
        log_dieusys(LOG_EXIT_SYS, "setsid");

    if (chdir("/") < 0)
        log_dieusys(LOG_EXIT_SYS, "chdir");

    umask(022);
}

int ssexec_tree_signal(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    int r, shut = 0 ;
    tain deadline ;
    uint8_t what = 0, requiredby = 0 ;
    tree_graph_t graph = GRAPH_TREE_ZERO ;
    uint32_t flag = 0, ntree = 0 ;

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

    if (what) {
        requiredby = 1 ;
        FLAGS_SET(flag, GRAPH_WANT_REQUIREDBY) ;
    } else {
        FLAGS_SET(flag, GRAPH_WANT_DEPENDS) ;
    }

    if ((svc_scandir_ok(info->scandir.s)) <= 0)
        log_die(LOG_EXIT_SYS,"scandir: ", info->scandir.s," is not running") ;

    if (!graph_new(&graph, (uint32_t)SS_MAX_SERVICE))
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

    pidtree_t apidt[graph.g.nsort] ;

    tree_init_array(apidt, &graph, requiredby, flag) ;

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

    r = tree_launch(apidt, graph.g.nsort, what, &deadline, info) ;

    tree_graph_destroy(&graph) ;

    return r ;
}
