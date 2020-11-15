/*
 * ssexec_all.c
 *
 * Copyright (c) 2018-2020 Eric Vidal <eric@obarun.org>
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

#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/log.h>
#include <oblibs/sastr.h>
#include <oblibs/obgetopt.h>
#include <oblibs/files.h>

#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/ssexec.h>
#include <66/constants.h>
#include <66/tree.h>
#include <66/utils.h>//scandir_ok

int all_doit(ssexec_t *info, unsigned int what, char const *const *envp)
{
    int r ;

    stralloc salist = STRALLOC_ZERO ;

    char ownerstr[UID_FMT] ;
    size_t ownerlen = uid_fmt(ownerstr,info->owner) ;
    ownerstr[ownerlen] = 0 ;

    char src[info->live.len + SS_STATE_LEN + 1 + ownerlen + 1 + info->treename.len + 1 + SS_LIVETREE_INIT_LEN + 1] ;
    auto_strings(src,info->live.s,SS_STATE,"/",ownerstr,"/",info->treename.s,SS_LIVETREE_INIT) ;

    r = scan_mode(src,S_IFREG) ;
    if (r == -1) log_die(LOG_EXIT_SYS,src," conflicted format") ;
    if (!r) {
        log_warn ("uninitialized tree: ", info->treename.s) ;
        goto freed ;
    }

    src[info->live.len + SS_STATE_LEN + 1 + ownerlen + 1 + info->treename.len] = 0 ;

    if (!sastr_dir_get(&salist,src,"init",S_IFREG))
        log_dieusys(LOG_EXIT_SYS,"get contents of directory: ",src) ;

    if (salist.len)
    {
        size_t pos = 0, len = sastr_len(&salist) ;
        int nargc = 2 + len + 1 ;
        char const *newargv[nargc] ;
        unsigned int m = 0 ;

        newargv[m++] = "fake_name" ;

        FOREACH_SASTR(&salist,pos) {

            newargv[m++] = salist.s + pos ;
        }

        newargv[m++] = 0 ;

        if (what) {

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

int ssexec_all(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{

    int r, what = 1, shut = 0, fd ;
    uint8_t unsupervise = 0 ;

    size_t statesize, pos = 0 ;

    stralloc contents = STRALLOC_ZERO ;

    {
        subgetopt_t l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = getopt_args(argc,argv, ">fU", &l) ;
            if (opt == -1) break ;
            if (opt == -2) log_die(LOG_EXIT_USER,"options must be set first") ;
            switch (opt)
            {
                case 'f' :  shut = 1 ; break ;
                case 'U' :  unsupervise = 1 ;
                            break ;
                default :   log_usage(usage_all) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc != 1) log_usage(usage_all) ;

    if (*argv[0] == 'u')
        what = 1 ;
    else if (*argv[0] == 'd')
        what = 0 ;
    else
        log_usage(usage_all) ;

    if ((scandir_ok(info->scandir.s)) <= 0) log_die(LOG_EXIT_SYS,"scandir: ", info->scandir.s," is not running") ;

    char ste[info->base.len + SS_SYSTEM_LEN + SS_STATE_LEN + 1] ;
    auto_strings(info->base.s,SS_SYSTEM,SS_STATE) ;

    r = scan_mode(ste,S_IFREG) ;
    if (r < 0) log_die(LOG_EXIT_SYS,"conflict format for: ",ste) ;
    if (!r) log_dieusys(LOG_EXIT_SYS,"find: ",ste) ;

    /** only one tree?*/
    if (info->treename.len)
    {
        if (!stralloc_cats(&contents,info->treename.s))log_dieu(LOG_EXIT_SYS,"add: ", info->treename.s," as tree to start") ;
        if (!stralloc_0(&contents)) log_die_nomem("stralloc") ;
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

    /** Down process? reverse in that case to respect tree start order*/
    if (!what)
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

        if (what)
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
            if (scandir_send_signal(info->scandir.s,"an") <= 0)
                log_dieusys(LOG_EXIT_SYS,"reload scandir: ",info->scandir.s) ;
        }
        if (!all_doit(info,what,envp))
            log_dieu(LOG_EXIT_SYS,(what) ? "start" : "stop" , " services of tree: ",info->treename.s) ;
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
