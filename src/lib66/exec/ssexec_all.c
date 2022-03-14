/*
 * ssexec_all.c
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
#include <stddef.h>//size_t
#include <stdlib.h>//realpath

#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/log.h>
#include <oblibs/sastr.h>
#include <oblibs/obgetopt.h>
#include <oblibs/files.h>
#include <oblibs/graph.h>

#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/posixplz.h>
#include <skalibs/tai.h>
#include <skalibs/selfpipe.h>
#include <skalibs/iopause.h>

#include <66/ssexec.h>
#include <66/constants.h>
#include <66/tree.h>
#include <66/utils.h>//scandir_ok
#include <66/db.h>
#include <66/graph.h>

#include <s6-rc/s6rc-servicedir.h>


#define FLAGS_STARTING 1 // starting not really up
#define FLAGS_STOPPING 2 // stopping not really down
#define FLAGS_UP 3 // really up
#define FLAGS_DOWN 4 // really down
#define FLAGS_BLOCK 5 // all deps are not up/down
#define FLAGS_UNBLOCK 6 // all deps up/down
#define FLAGS_FATAL 7 // process crashed
#define FLAGS_EXITED 8 // process exited


/** return 1 if set, else 0*/
#define FLAGS_ISSET(has, want) \
        ((~(has) & (want)) == 0)

typedef struct pidvertex_s pidvertex_t, *pidvertex_t_ref ;
struct pidvertex_s
{
    pid_t pid ;
    unsigned int vertex ; // id at graph_hash_t struct
    uint8_t state ;
    int nedge ;
    unsigned int *edge ; // array of id at graph_hash_t struct
} ;
#define PIDINDEX_ZERO { 0, 0, 0, 0, 0 }

//static pidvertex_t *apidvertex ;
static pidvertex_t *apidvertex ;
static unsigned int napid = 0 ;
static unsigned int npid = 0 ;
static int flag ;
static int flag_run ;

static void async(unsigned int i, unsigned int what, ssexec_t *info, graph_t *graph) ;
static int unsupervise(ssexec_t *info, int what) ;

static inline unsigned int lookup (char const *const *table, char const *signal)
{
    log_flow() ;

    unsigned int i = 0 ;
    for (; table[i] ; i++) if (!strcmp(signal, table[i])) break ;
    return i ;
}

static inline unsigned int parse_signal (char const *signal)
{
    log_flow() ;

    static char const *const signal_table[] = {
        "up",
        "down",
        "unsupervise",
        0
    } ;
    unsigned int i = lookup(signal_table, signal) ;
    if (!signal_table[i]) log_usage(usage_all) ;
    return i ;
}

static void all_redir_fd(void)
{
    log_flow() ;

    int fd ;
    while((fd = open("/dev/tty",O_RDWR|O_NOCTTY)) >= 0) {

        if (fd >= 3)
            break ;
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


static int doit(ssexec_t *info, char const *treename, unsigned int what)
{
    log_flow() ;

    int r, e = 1 ;

    {
        info->treename.len = 0 ;

        if (!auto_stra(&info->treename, treename))
            log_die_nomem("stralloc") ;

        info->tree.len = 0 ;

        if (!auto_stra(&info->tree, treename))
            log_die_nomem("stralloc") ;

        r = tree_sethome(info) ;
        if (r <= 0)
            log_warnu_return(LOG_EXIT_ONE, "find tree: ", info->treename.s) ;

        if (!tree_get_permissions(info->tree.s, info->owner))
            log_warn_return(LOG_EXIT_ONE, "You're not allowed to use the tree: ", info->tree.s) ;

    }

    if (!tree_isinitialized(info->base.s, info->treename.s)) {

        if (!what) {

            int nargc = 3 ;
            char const *newargv[nargc] ;
            unsigned int m = 0 ;

            newargv[m++] = "fake_name" ;
            newargv[m++] = "b" ;
            newargv[m++] = 0 ;

            if (ssexec_init(nargc, newargv, (char const *const *)environ, info))
                log_warnu_return(LOG_EXIT_ONE, "initiate services of tree: ", info->treename.s) ;

            log_trace("reload scandir: ", info->scandir.s) ;
            if (scandir_send_signal(info->scandir.s, "h") <= 0)
                log_warnu_return(LOG_EXIT_ONE, "reload scandir: ", info->scandir.s) ;

        } else {

            log_warn ("uninitialized tree: ", info->treename.s) ;
            goto end ;
        }
    }
    /** tree can be empty
     * In this case ssexec_init do not crash but the
     * /run/66/state/<owner>/<treename> doesn't exit.
     * So check it again */
    if (!tree_isinitialized(info->base.s, info->treename.s))
        goto end ;

    if (what == 2) {

       if (unsupervise(info, what))
            goto err ;

    } else {

        char const *exclude[1] = { 0 } ;
        char ownerstr[UID_FMT] ;
        size_t ownerlen = uid_fmt(ownerstr, info->owner) ;
        ownerstr[ownerlen] = 0 ;

        stralloc salist = STRALLOC_ZERO ;

        char src[info->live.len + SS_STATE_LEN + 1 + ownerlen + 1 + info->treename.len + 1] ;

        auto_strings(src, info->live.s, SS_STATE, "/", ownerstr, "/", info->treename.s) ;

        if (!sastr_dir_get(&salist, src, exclude, S_IFREG))
            log_warnusys_return(LOG_EXIT_ONE, "get contents of directory: ", src) ;

        if (salist.len) {

            size_t pos = 0, len = sastr_len(&salist) ;
            int n = what == 2 ? 3 : 2 ;
            int nargc = n + len ;
            char const *newargv[nargc] ;
            unsigned int m = 0 ;

            newargv[m++] = "fake_name" ;
            if (what == 2)
                newargv[m++] = "-u" ;

            FOREACH_SASTR(&salist, pos)
                newargv[m++] = salist.s + pos ;

            newargv[m++] = 0 ;

            if (!what) {

                if (ssexec_start(nargc, newargv, (char const *const *)environ, info))
                    goto err ;

            } else {

                if (ssexec_stop(nargc, newargv, (char const *const *)environ, info))
                    goto err ;
            }

        } else log_info("Empty tree: ", info->treename.s, " -- nothing to do") ;

        stralloc_free(&salist) ;
    }

    end:
        e = 0 ;
    err:
        return e ;
}

static int unsupervise(ssexec_t *info, int what)
{
    log_flow() ;

    size_t newlen = info->livetree.len + 1, pos = 0 ;
    char const *exclude[1] = { 0 } ;

    char ownerstr[UID_FMT] ;
    size_t ownerlen = uid_fmt(ownerstr, info->owner) ;
    ownerstr[ownerlen] = 0 ;

    stralloc salist = STRALLOC_ZERO ;

    /** set what we need */
    char prefix[info->treename.len + 2] ;
    auto_strings(prefix, info->treename.s, "-") ;

    char livestate[info->live.len + SS_STATE_LEN + 1 + ownerlen + 1 + info->treename.len + 1] ;
    auto_strings(livestate, info->live.s, SS_STATE + 1, "/", ownerstr, "/", info->treename.s) ;

    /** bring down service */
    if (doit(info, info->treename.s, what))
        log_warnusys("stop services") ;

    if (db_find_compiled_state(info->livetree.s, info->treename.s) >=0) {

        salist.len = 0 ;
        char livetree[newlen + info->treename.len + SS_SVDIRS_LEN + 1] ;
        auto_strings(livetree, info->livetree.s, "/", info->treename.s, SS_SVDIRS) ;

        if (!sastr_dir_get(&salist,livetree,exclude,S_IFDIR))
            log_warnusys_return(LOG_EXIT_ONE, "get service list at: ", livetree) ;

        livetree[newlen + info->treename.len] = 0 ;

        pos = 0 ;
        FOREACH_SASTR(&salist,pos) {

            s6rc_servicedir_unsupervise(livetree, prefix, salist.s + pos, 0) ;
        }

        char *realsym = realpath(livetree, 0) ;
        if (!realsym)
            log_warnusys_return(LOG_EXIT_ONE, "find realpath of: ", livetree) ;

        if (rm_rf(realsym) == -1)
            log_warnusys_return(LOG_EXIT_ONE, "remove: ", realsym) ;

        free(realsym) ;

        if (rm_rf(livetree) == -1)
            log_warnusys_return(LOG_EXIT_ONE, "remove: ", livetree) ;

        /** remove the symlink itself */
        unlink_void(livetree) ;
    }

    if (scandir_send_signal(info->scandir.s,"h") <= 0)
        log_warnusys_return(LOG_EXIT_ONE, "reload scandir: ", info->scandir.s) ;

    /** remove /run/66/state/uid/treename directory */
    log_trace("delete: ", livestate, "..." ) ;
    if (rm_rf(livestate) < 0)
        log_warnusys_return(LOG_EXIT_ONE, "delete ", livestate) ;

    log_info("Unsupervised successfully tree: ", info->treename.s) ;

    stralloc_free(&salist) ;

    return 0 ;
}

static pidvertex_t pidvertex_init(unsigned int len)
{
    log_flow() ;

    pidvertex_t pidv = PIDINDEX_ZERO ;

    pidv.edge = (unsigned int *)malloc(len*sizeof(unsigned int)) ;

    graph_array_init_single(pidv.edge, len) ;

    return pidv ;
}

static void pidvertex_free(pidvertex_t *pidv)
{
    free(pidv->edge) ;
}

static void pidvertex_init_array(pidvertex_t *apidvertex, graph_t *g, unsigned int *list, unsigned int count, ssexec_t *info, uint8_t requiredby)
{
    log_flow() ;

    int r = -1 ;
    size_t pos = 0 ;

    for (; pos < count ; pos++) {

        char *name = g->data.s + genalloc_s(graph_hash_t,&g->hash)[list[pos]].vertex ;

        pidvertex_t pidv = pidvertex_init(g->mlen) ;

        pidv.nedge = graph_matrix_get_edge_g_sorted_list(pidv.edge, g, name, requiredby) ;

        if (pidv.nedge < 0)
            log_dieu(LOG_EXIT_SYS,"get sorted ", requiredby ? "required by" : "dependency", " list of tree: ", name) ;

        pidv.vertex = list[pos] ;

        if (pidv.vertex < 0)
            log_dieu(LOG_EXIT_SYS, "get vertex id -- please make a bug report") ;

        r = tree_isinitialized(info->base.s, name) ;

        if (r < 0)
            log_dieu(LOG_EXIT_SYS, "read resolve file of tree: ", name) ;

        if (r)
            pidv.state |= FLAGS_UP ;
        else
            pidv.state |= FLAGS_DOWN ;

        apidvertex[pos] = pidv ;
    }

}

static void pidvertex_array_free(pidvertex_t *apidv,  unsigned int len)
{
    log_flow() ;

    size_t pos = 0 ;

    for(; pos < len ; pos++)
        pidvertex_free(&apidv[pos]) ;

}

static int pidvertex_get_id(pidvertex_t *apidv, unsigned int id)
{
    log_flow() ;

    unsigned int pos = 0 ;

    for (; pos < napid ; pos++) {
        if (apidv[pos].vertex == id)
            return (unsigned int) pos ;
    }
    return -1 ;
}

static void async_deps(unsigned int i, unsigned int what, ssexec_t *info, graph_t *graph)
{
    log_flow() ;

    unsigned int pos = 0 ;

    while (apidvertex[i].nedge) {
        /** TODO: the pidvertex_get_id() function make a loop
         * through the apidvertex array to find the corresponding
         * index of the edge at the apidvertex array.
         * This is clearly a waste of time and need to be optimized. */
        unsigned int id = pidvertex_get_id(apidvertex, apidvertex[i].edge[pos]) ;

        if (id < 0)
            log_dieu(LOG_EXIT_SYS, "get apidvertex id -- please make a bug report") ;

        async(id, what, info, graph) ;
        apidvertex[i].nedge-- ;
        pos++ ;
    }


}

static void async(unsigned int i, unsigned int what, ssexec_t *info, graph_t *graph)
{
    /**
     * @i: apidvertex array index
     * */

    log_flow() ;

    int r ;
    pid_t pid ;

    char *treename = graph->data.s + genalloc_s(graph_hash_t,&graph->hash)[apidvertex[i].vertex].vertex ;

    if (FLAGS_ISSET(apidvertex[i].state, flag) && !FLAGS_ISSET(apidvertex[i].state, flag_run)) {

        apidvertex[i].state |= flag_run ;
        apidvertex[i].state |= FLAGS_BLOCK ;

        pid = fork() ;

        if (pid < 0)
            log_dieusys(LOG_EXIT_SYS, "fork") ;

        if (!pid) {

            if (apidvertex[i].nedge)
                async_deps(i, what, info, graph) ;

            r = doit(info, treename, what) ;
            _exit(r) ;

        }

        apidvertex[npid++].pid = pid ;

     } else {

         log_trace("skipping: ", treename, " -- already ", what ? "down" : "up") ;
     }

    return ;
}

static inline void kill_all (void)
{
    log_flow() ;

    unsigned int j = npid ;
    while (j--) kill(apidvertex[j].pid, SIGKILL) ;
}

static int handle_signal(pidvertex_t *apidvertex, unsigned int what)
{
    log_flow() ;

    int ok = 1 ;

    for (;;) {

        int s = selfpipe_read() ;
        switch (s) {

            case -1 : log_warnusys_return(LOG_EXIT_ZERO,"selfpipe_read") ;
            case 0 : return ok ;
            case SIGCHLD :

                for (;;) {

                    unsigned int j = 0 ;
                    int wstat ;
                    pid_t r = wait_nohang(&wstat) ;

                    if (r < 0) {

                        if (errno = ECHILD)
                            break ;
                        else
                            log_dieusys(LOG_EXIT_SYS,"wait for children") ;

                    } else if (!r) break ;

                    for (; j < npid ; j++)
                        if (apidvertex[j].pid == r)
                            break ;

                    if (j < npid) {

                        unsigned int i = j ;
                        apidvertex[j] = apidvertex[--npid] ;
                        if (!WIFSIGNALED(wstat) && !WEXITSTATUS(wstat)) {

                            apidvertex[i].state = 0 ;
                            if (what)
                                apidvertex[i].state = FLAGS_UP ;
                            else
                                apidvertex[i].state = FLAGS_DOWN ;

                            apidvertex[i].state |= FLAGS_UNBLOCK ;

                        } else {

                            ok = 0 ;
                            apidvertex[i].state = 0 ;
                            if (what)
                                apidvertex[i].state = FLAGS_DOWN ;
                            else
                                apidvertex[i].state = FLAGS_UP ;
                        }
                    }
                }
                break ;
            case SIGTERM :
            case SIGINT :
                    log_info("received SIGINT, aborting service transition") ;
                    kill_all() ;
                    break ;
            default : log_die(LOG_EXIT_SYS, "unexpected data in selfpipe") ;
        }
    }
    return ok ;
}

static int waitit(int spfd, pidvertex_t *apidv, graph_t *graph, unsigned int what, tain *deadline, ssexec_t *info)
{
    iopause_fd x = { .fd = spfd, .events = IOPAUSE_READ } ;
    unsigned int e = 1, pos = 0 ;
    int r ;
    pidvertex_t apidvertextable[napid] ;
    apidvertex = apidvertextable ;

    for (; pos < napid ; pos++)
        apidvertex[pos] = apidv[pos] ;

    for (pos = 0 ; pos < napid ; pos++)
        async(pos, what, info, graph) ;

    while (npid) {

        r = iopause_g(&x, 1, deadline) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "iopause") ;
        if (!r)
            log_die(LOG_EXIT_SYS,"time out") ;
        if (!handle_signal(apidvertex, what))
            e = 0 ;
    }
    return e ;
}

int ssexec_all(int argc, char const *const *argv,char const *const *envp, ssexec_t *info)
{

    int r, shut = 0, fd ;
    tain deadline ;
    unsigned int what ;

    uint8_t requiredby = 0 ;
    graph_t graph = GRAPH_ZERO ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = getopt_args(argc,argv, ">" OPTS_ALL, &l) ;
            if (opt == -1) break ;
            if (opt == -2) log_die(LOG_EXIT_USER,"options must be set first") ;
            switch (opt)
            {
                case 'f' :  shut = 1 ; break ;
                default :   log_usage(usage_all) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc != 1) log_usage(usage_all) ;

    if (info->timeout)
        tain_from_millisecs(&deadline, info->timeout) ;
    else
        deadline = tain_infinite_relative ;

    what = parse_signal(*argv) ;

    if (what) {

        requiredby = 1 ;
        flag =  FLAGS_UP ;
        flag_run = FLAGS_STOPPING ;

    } else {

        flag = FLAGS_DOWN ;
        flag_run = FLAGS_STARTING ;
    }

    if ((scandir_ok(info->scandir.s)) <= 0)
        log_die(LOG_EXIT_SYS,"scandir: ", info->scandir.s," is not running") ;

    if (!graph_build_g(&graph, info->base.s, info->treename.s, DATA_TREE))
        log_dieu(LOG_EXIT_SYS,"build the graph") ;

    /** initialize and allocate apidvertex array */

    pidvertex_t apidv[graph.mlen] ;

    /** only on tree */
    if (info->treename.len) {

        unsigned int *alist ;

        alist = (unsigned int *)malloc(graph.mlen*sizeof(unsigned int)) ;

        graph_array_init_single(alist, graph.mlen) ;

        napid = graph_matrix_get_edge_g_sorted_list(alist, &graph, info->treename.s, requiredby) ;

        if (napid < 0)
            log_dieu(LOG_EXIT_SYS, "get ", requiredby ? "required" : "dependencies", " sorted list of: ", info->treename.s) ;

        alist[napid++] = (unsigned int)graph_hash_vertex_get_id(&graph, info->treename.s) ;

        pidvertex_init_array(apidv, &graph, alist, napid, info, requiredby) ;

        free(alist) ;

    } else {

        napid = graph.sort_count ;

        pidvertex_init_array(apidv, &graph, graph.sort, graph.sort_count, info, requiredby) ;
    }

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

            r = 1 ;

            goto end ;
        }
    }

    tain_now_set_stopwatch_g() ;
    tain_add_g(&deadline, &deadline) ;

    int spfd = selfpipe_init() ;

    if (spfd < 0)
        log_dieusys(LOG_EXIT_SYS, "selfpipe_init") ;

    if (!selfpipe_trap(SIGCHLD) ||
        !selfpipe_trap(SIGINT) ||
        !selfpipe_trap(SIGTERM))
            log_dieusys(LOG_EXIT_SYS, "selfpipe_trap") ;

    r = waitit(spfd, apidv, &graph, what, &deadline, info) ;

    end:
        selfpipe_finish() ;

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
        pidvertex_array_free(apidv, napid) ;


    return (!r) ? 111 : 0 ;
}
