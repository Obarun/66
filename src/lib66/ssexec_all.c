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

#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/posixplz.h>

#include <66/ssexec.h>
#include <66/constants.h>
#include <66/tree.h>
#include <66/utils.h>//scandir_ok
#include <66/db.h>

#include <s6-rc/s6rc-servicedir.h>

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

    static char const *const signal_table[] =
    {
        "up",
        "down",
        "unsupervise",
        0
    } ;
  unsigned int i = lookup(signal_table, signal) ;
  if (!signal_table[i]) log_usage(usage_all) ;
  return i ;
}

int all_doit(ssexec_t *info, unsigned int what, char const *const *envp)
{
    log_flow() ;

    int r ;

    stralloc salist = STRALLOC_ZERO ;

    char ownerstr[UID_FMT] ;
    size_t ownerlen = uid_fmt(ownerstr,info->owner) ;
    ownerstr[ownerlen] = 0 ;

    char src[info->live.len + SS_STATE_LEN + 1 + ownerlen + 1 + info->treename.len + 1 + SS_LIVETREE_INIT_LEN + 1] ;
    auto_strings(src,info->live.s,SS_STATE,"/",ownerstr,"/",info->treename.s,"/",SS_LIVETREE_INIT) ;

    r = scan_mode(src,S_IFREG) ;
    if (r == -1) log_die(LOG_EXIT_SYS,src," conflicted format") ;
    if (!r) {
        log_warn ("uninitialized tree: ", info->treename.s) ;
        goto freed ;
    }

    src[info->live.len + SS_STATE_LEN + 1 + ownerlen + 1 + info->treename.len] = 0 ;

    if (!sastr_dir_get(&salist,src,SS_LIVETREE_INIT,S_IFREG))
        log_dieusys(LOG_EXIT_SYS,"get contents of directory: ",src) ;

    if (salist.len)
    {
        size_t pos = 0, len = sastr_len(&salist) ;
        int n = what == 2 ? 3 : 2 ;
        int nargc = n + len ;
        char const *newargv[nargc] ;
        unsigned int m = 0 ;

        newargv[m++] = "fake_name" ;
        if (what == 2)
            newargv[m++] = "-u" ;

        FOREACH_SASTR(&salist,pos) {

            newargv[m++] = salist.s + pos ;
        }

        newargv[m++] = 0 ;

        if (!what) {

            if (ssexec_start(nargc,newargv,envp,info))
                goto err ;

        } else {

            if (ssexec_stop(nargc,newargv,envp,info))
                goto err ;
        }
    }
    else
        log_info("Empty tree: ",info->treename.s," -- nothing to do") ;

    freed:
        stralloc_free(&salist) ;
        return 1 ;
    err:
        stralloc_free(&salist) ;
        return 0 ;
}

static void all_redir_fd(void)
{
    log_flow() ;

    int fd ;
    while((fd = open("/dev/tty",O_RDWR|O_NOCTTY)) >= 0)
    {
        if (fd >= 3) break ;
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

void all_unsupervise(ssexec_t *info, char const *const *envp,int what)
{
    log_flow() ;

    size_t newlen = info->livetree.len + 1, pos = 0 ;

    char ownerstr[UID_FMT] ;
    size_t ownerlen = uid_fmt(ownerstr,info->owner) ;
    ownerstr[ownerlen] = 0 ;

    stralloc salist = STRALLOC_ZERO ;

    /** set what we need */
    char prefix[info->treename.len + 2] ;
    auto_strings(prefix,info->treename.s,"-") ;

    char livestate[info->live.len + SS_STATE_LEN + 1 + ownerlen + 1 + info->treename.len + 1] ;
    auto_strings(livestate,info->live.s,SS_STATE + 1,"/",ownerstr,"/",info->treename.s) ;

    /** bring down service */
    if (!all_doit(info,what,envp))
        log_warnusys("stop services") ;

    if (db_find_compiled_state(info->livetree.s,info->treename.s) >=0)
    {
        salist.len = 0 ;
        char livetree[newlen + info->treename.len + SS_SVDIRS_LEN + 1] ;
        auto_strings(livetree,info->livetree.s,"/",info->treename.s,SS_SVDIRS) ;

        if (!sastr_dir_get(&salist,livetree,"",S_IFDIR)) log_dieusys(LOG_EXIT_SYS,"get service list at: ",livetree) ;

        livetree[newlen + info->treename.len] = 0 ;

        pos = 0 ;
        FOREACH_SASTR(&salist,pos) {

            s6rc_servicedir_unsupervise(livetree,prefix,salist.s + pos,0) ;
        }

        char *realsym = realpath(livetree, 0) ;
        if (!realsym)
            log_dieusys(LOG_EXIT_SYS,"find realpath of: ",livetree) ;

        if (rm_rf(realsym) == -1)
            log_dieusys(LOG_EXIT_SYS,"remove: ", realsym) ;

        free(realsym) ;

        if (rm_rf(livetree) == -1)
            log_dieusys(LOG_EXIT_SYS,"remove: ", livetree) ;

        /** remove the symlink itself */
        unlink_void(livetree) ;
    }

    if (scandir_send_signal(info->scandir.s,"h") <= 0)
        log_dieusys(LOG_EXIT_SYS,"reload scandir: ",info->scandir.s) ;

    /** remove /run/66/state/uid/treename directory */
    log_trace("delete: ",livestate,"..." ) ;
    if (rm_rf(livestate) < 0)
        log_dieusys(LOG_EXIT_SYS,"delete ",livestate) ;

    log_info("Unsupervised successfully tree: ",info->treename.s) ;

    stralloc_free(&salist) ;
}

int ssexec_all(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{

    int r, what, shut = 0, fd ;

    size_t statesize, pos = 0 ;

    stralloc contents = STRALLOC_ZERO ;

    {
        subgetopt_t l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = getopt_args(argc,argv, ">f", &l) ;
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

    what = parse_signal(*argv) ;

    if ((scandir_ok(info->scandir.s)) <= 0)
        log_die(LOG_EXIT_SYS,"scandir: ", info->scandir.s," is not running") ;

    char ste[info->base.len + SS_SYSTEM_LEN + SS_STATE_LEN + 1] ;
    auto_strings(ste,info->base.s,SS_SYSTEM,SS_STATE) ;

    r = scan_mode(ste,S_IFREG) ;
    if (r < 0) log_die(LOG_EXIT_SYS,"conflict format for: ",ste) ;
    if (!r) log_dieusys(LOG_EXIT_SYS,"find: ",ste) ;

    /** only one tree?*/
    if (info->treename.len)
    {
        if (!auto_stra(&contents,info->treename.s))
            log_die_nomem("stralloc") ;
    }
    else
    {
        statesize = file_get_size(ste) ;

        r = openreadfileclose(ste,&contents,statesize) ;
        if(!r) log_dieusys(LOG_EXIT_SYS,"open: ", ste) ;

        /** ensure that we have an empty line at the end of the string*/
        if (!stralloc_cats(&contents,"\n") ||
        !stralloc_0(&contents)) log_die_nomem("stralloc") ;

        if (!sastr_clean_element(&contents)) {
            log_info("nothing to do") ;
            goto end ;
        }

    }

    if (shut)
    {
        pid_t dpid ;
        int wstat = 0 ;

        dpid = fork() ;

        if (dpid < 0) log_dieusys(LOG_EXIT_SYS,"fork") ;
        else if (dpid > 0)
        {
            if (waitpid_nointr(dpid,&wstat, 0) < 0)
                log_dieusys(LOG_EXIT_SYS,"wait for child") ;

            if (wstat)
                log_die(LOG_EXIT_SYS,"child fail") ;

            goto end ;
        }
        else all_redir_fd() ;
    }

    /** Down/unsupervise process? reverse in that case to respect tree start order*/
    if (what)
        if (!sastr_reverse(&contents)) log_dieu(LOG_EXIT_SYS,"reserve tree order") ;

    FOREACH_SASTR(&contents,pos) {

        info->treename.len = 0 ;

        if (!auto_stra(&info->treename,contents.s + pos))
            log_die_nomem("stralloc") ;

        info->tree.len = 0 ;

        if (!auto_stra(&info->tree,contents.s + pos))
            log_die_nomem("stralloc") ;

        r = tree_sethome(&info->tree,info->base.s,info->owner) ;
        if (r < 0 || !r) log_dieusys(LOG_EXIT_SYS,"find tree: ", info->treename.s) ;

        if (!tree_get_permissions(info->tree.s,info->owner))
            log_die(LOG_EXIT_USER,"You're not allowed to use the tree: ",info->tree.s) ;

        if (!what)
        {
            int nargc = 3 ;
            char const *newargv[nargc] ;
            unsigned int m = 0 ;

            newargv[m++] = "fake_name" ;
            newargv[m++] = "b" ;
            newargv[m++] = 0 ;

            if (ssexec_init(nargc,newargv,envp,info))
                log_dieu(LOG_EXIT_SYS,"initiate services of tree: ",info->treename.s) ;

            log_trace("reload scandir: ",info->scandir.s) ;
            if (scandir_send_signal(info->scandir.s,"h") <= 0)
                log_dieusys(LOG_EXIT_SYS,"reload scandir: ",info->scandir.s) ;
        }

        if (what < 2) {

            if (!all_doit(info,what,envp))
                log_dieu(LOG_EXIT_SYS,(what) ? "start" : "stop" , " services of tree: ",info->treename.s) ;

        } else {

            all_unsupervise(info,envp,what) ;
        }
    }
    end:
        if (shut)
        {
            while((fd = open("/dev/tty",O_RDWR|O_NOCTTY)) >= 0)
            {
                if (fd >= 3) break ;
            }
            dup2 (fd,0) ;
            dup2 (fd,1) ;
            dup2 (fd,2) ;
            fd_close(fd) ;
        }

        stralloc_free(&contents) ;

    return 0 ;
}
