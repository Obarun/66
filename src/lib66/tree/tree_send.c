/*
 * tree_send.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>

#include <oblibs/log.h>
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

int tree_send(uint8_t operation, char const *treename, uint8_t dofork, ssexec_t *info)
{
    log_flow() ;

    int r, shut = 0 ;
    uint8_t what = operation, requiredby = 0 ;
    tree_graph_t graph = GRAPH_TREE_ZERO ;
    uint32_t flag = 0, ntree = 0 ;

    shut = dofork ;

    info->treename.len = 0 ;

    if (treename && treename[0]) {
        if (!auto_strbuf(&info->treename, treename))
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
