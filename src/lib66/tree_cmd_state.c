/*
 * tree_cmd_state.c
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

#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include <oblibs/obgetopt.h>
#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/files.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>
#include <skalibs/types.h>
#include <skalibs/djbunix.h>

#include <66/utils.h>
#include <66/constants.h>

//USAGE "tree_state [ -v verbosity ] [ -a add ] [ -d delete ] [ -s search ] tree"

int tree_state(int argc, char const *const *argv)
{
    log_flow() ;

    int r, fd,skip = -1 ;
    unsigned int add, del, sch, verbosity, err ;
    size_t statesize = 0, treelen, statelen, pos = 0 ;
    uid_t owner = MYUID ;
    char const *tree = 0 ;

    verbosity = 1 ;
    add = del = sch = err = 0 ;
    {
        subgetopt_t l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = getopt_args(argc,argv, "v:sad", &l) ;
            if (opt == -1) break ;
            if (opt == -2) log_warn_return(LOG_EXIT_ZERO,"options must be set first") ;
            switch (opt)
            {
                case 'v' :  if (!uint0_scan(l.arg, &verbosity)) return 0 ;  break ;
                case 'a' :  add = 1 ; if (del) return 0 ; break ;
                case 'd' :  del = 1 ; if (add) return 0 ; break ;
                case 's' :  sch = 1 ; break ;
                default :   return 0 ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 1) return 0 ;

    stralloc base = STRALLOC_ZERO ;
    stralloc contents = STRALLOC_ZERO ;

    tree = *argv ;
    treelen = strlen(tree) ;

    if (!set_ownersysdir(&base,owner))
    {
        log_warnusys("set owner directory") ;
        stralloc_free(&base) ;
        stralloc_free(&contents) ;
        return 0 ;
    }
    /** /system/state */
    char state[base.len + SS_SYSTEM_LEN + SS_STATE_LEN + 1] ;
    memcpy(state,base.s,base.len) ;
    memcpy(state + base.len,SS_SYSTEM,SS_SYSTEM_LEN) ;
    memcpy(state + base.len + SS_SYSTEM_LEN, SS_STATE ,SS_STATE_LEN) ;
    statelen = base.len + SS_SYSTEM_LEN + SS_STATE_LEN ;
    state[statelen] = 0 ;

    r = scan_mode(state,S_IFREG) ;
    if (r == -1) { errno = EEXIST ; goto out ; }
    if (!r)
    {
        log_warnusys("find: ",state) ;
        goto out ;
    }

    statesize = file_get_size(state) ;
    r = openreadfileclose(state,&contents,statesize) ;
    if(!r)
    {
        log_warnusys("open: ", state) ;
        goto out ;
    }

    if (contents.len)
    {
        if (!sastr_split_string_in_nline(&contents)) goto out ;
    }

    if (add)
    {
        if (sastr_cmp(&contents,tree) == -1)
        {
            fd = open_append(state) ;
            if (fd < 0)
            {
                log_warnusys("open: ",state) ;
                goto out ;
            }
            r = write(fd, tree,treelen);
            r = write(fd, "\n",1);
            if (r < 0)
            {
                log_warnusys("write: ",state," with ", tree," as content") ;
                fd_close(fd) ;
                goto out ;
            }
            fd_close(fd) ;
        }
        else
        {
            err = 2 ;
            goto out ;
        }
    }

    if (del)
    {
        skip = sastr_cmp(&contents,tree) ;
        if (skip >= 0)
        {
            fd = open_trunc(state) ;
            if (fd < 0)
            {
                log_warnusys("open_trunc ", state) ;
                goto out ;
            }

            /*** replace it by write_file_unsafe*/
            for (;pos < contents.len ; pos += strlen(contents.s + pos) + 1)
            {
                if (pos == (size_t)skip) continue ;
                char *name = contents.s + pos ;
                size_t namelen = strlen(contents.s + pos) ;
                r = write(fd, name,namelen);
                if (r < 0)
                {
                    log_warnusys("write: ",state," with ", name," as content") ;
                    fd_close(fd) ;
                    goto out ;
                }
                r = write(fd, "\n",1);
                if (r < 0)
                {
                    log_warnusys("write: ",state," with ", name," as content") ;
                    fd_close(fd) ;
                    goto out ;
                }
            }
            fd_close(fd) ;
        }
        else
        {
            err = 2 ;
            goto out ;
        }
    }
    if (sch)
    {
        if (sastr_cmp(&contents,tree) >= 0)
        {
            err = 1 ;
            goto out ;
        }
        else
        {
            err = 2 ;
            goto out ;
        }
    }
    err = 1 ;
    out:
    stralloc_free(&base) ;
    stralloc_free(&contents) ;

    return err ;
}

int tree_cmd_state(unsigned int verbosity,char const *cmd, char const *tree)
{
    log_flow() ;

    int r ;
    size_t pos = 0 ;
    stralloc opts = STRALLOC_ZERO ;

    if (!sastr_clean_string(&opts,cmd))
    {
        log_warnu("clean: ",cmd) ;
        stralloc_free(&opts) ;
        return 0 ;
    }
    int newopts = 5 + sastr_len(&opts) ;
    char const *newargv[newopts] ;
    unsigned int m = 0 ;
    char fmt[UINT_FMT] ;
    fmt[uint_fmt(fmt, verbosity)] = 0 ;

    newargv[m++] = "tree_state" ;
    newargv[m++] = "-v" ;
    newargv[m++] = fmt ;

    for (;pos < opts.len; pos += strlen(opts.s + pos) + 1)
        newargv[m++] = opts.s + pos ;

    newargv[m++] = tree ;
    newargv[m++] = 0 ;

    r = tree_state(newopts,newargv) ;

    stralloc_free(&opts) ;

    return r ;
}
