/*
 * svc_init.c
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

#include <66/svc.h>

#include <string.h>
#include <stdlib.h>
//#include <stdio.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/sastr.h>

#include <skalibs/genalloc.h>
#include <skalibs/types.h>
#include <skalibs/tai.h>
#include <skalibs/djbunix.h>

#include <s6/s6-supervise.h>
#include <s6/ftrigr.h>
#include <s6/ftrigw.h>

#include <66/utils.h>
#include <66/resolve.h>
#include <66/constants.h>
#include <66/ssexec.h>
#include <66/state.h>

int svc_init(ssexec_t *info,char const *src, genalloc *ga)
{
    log_flow() ;

    size_t namelen, srclen, svscanlen, tmplen, pos, i ;
    ssize_t logname ;
    gid_t gid = getgid() ;
    uint16_t id ;

    ftrigr_t fifo = FTRIGR_ZERO ;
    stralloc sadown = STRALLOC_ZERO ;
    genalloc ids = GENALLOC_ZERO ; // uint16_t
    ss_state_t sta = STATE_ZERO ;

    tain_t deadline ;
    tain_now_set_stopwatch_g() ;
    tain_addsec(&deadline,&STAMP,2) ;

    if (!ftrigr_startf(&fifo, &deadline, &STAMP))
        goto err ;

    if (!ss_resolve_create_live(info)) { log_warnusys("create live state") ; goto err ; }

    for (i = 0 ; i < genalloc_len(ss_resolve_t,ga); i++)
    {
        logname = 0 ;
        char *string = genalloc_s(ss_resolve_t,ga)[i].sa.s ;
        char *name = string + genalloc_s(ss_resolve_t,ga)[i].name ;
        char *state = string + genalloc_s(ss_resolve_t,ga)[i].state ;
        if (s6_svc_ok(string + genalloc_s(ss_resolve_t,ga)[i].runat))
        {
            log_info("Initialization aborted -- ",name," already initialized") ;
            log_trace("Write state file of: ",name) ;
            if (!ss_state_write(&sta,state,name))
            {
                log_warnusys("write state file of: ",name) ;
                goto err ;
            }
            continue ;
        }
        logname = get_rstrlen_until(name,SS_LOG_SUFFIX) ;
        if (logname > 0) name = string + genalloc_s(ss_resolve_t,ga)[i].logassoc ;

        namelen = strlen(name) ;
        srclen = strlen(src) ;
        char svsrc[srclen + 1 + namelen + 1] ;
        memcpy(svsrc,src,srclen) ;
        svsrc[srclen] = '/' ;
        memcpy(svsrc + srclen + 1,name,namelen) ;
        svsrc[srclen + 1 + namelen] = 0 ;

        if (logname > 0) svscanlen = strlen(string + genalloc_s(ss_resolve_t,ga)[i].runat) - SS_LOG_SUFFIX_LEN ;
        else svscanlen = strlen(string + genalloc_s(ss_resolve_t,ga)[i].runat) ;
        char svscan[svscanlen + 6 + 1] ;
        memcpy(svscan,string + genalloc_s(ss_resolve_t,ga)[i].runat,svscanlen) ;
        svscan[svscanlen] = 0 ;

        log_trace("init service: ", string + genalloc_s(ss_resolve_t,ga)[i].name) ;
        /** if logger was created do not pass here to avoid to erase
         * the fifo of the logger*/
        if (!scan_mode(svscan,S_IFDIR))
        {
            log_trace("copy: ",svsrc, " to ", svscan) ;
            if (!hiercopy(svsrc,svscan))
            {
                log_warnusys("copy: ",svsrc," to: ",svscan) ;
                goto err ;
            }
        }

        /** if logger you need to copy again the real path */
        svscanlen = strlen(string + genalloc_s(ss_resolve_t,ga)[i].runat) ;
        memcpy(svscan,string + genalloc_s(ss_resolve_t,ga)[i].runat,svscanlen) ;
        svscan[svscanlen] = 0 ;
        /** if logger and the reload was asked the folder xxx/log doesn't exist
         * check it and create again if doesn't exist */
        if (!scan_mode(svscan,S_IFDIR))
        {
            tmplen = strlen(svsrc) ;
            char tmp[tmplen + 4 + 1] ;
            memcpy(tmp,svsrc,tmplen) ;
            memcpy(tmp + tmplen,"/log",4) ;
            tmp[tmplen + 4] = 0 ;
            log_trace("copy: ",tmp, " to ", svscan) ;
            if (!hiercopy(tmp,svscan))
            {
                log_warnusys("copy: ",tmp," to: ",svscan) ;
                goto err ;
            }
        }
        memcpy(svscan + svscanlen, "/down", 5) ;
        svscan[svscanlen + 5] = 0 ;

        if (!genalloc_s(ss_resolve_t,ga)[i].down)
        {
            if (!sastr_add_string(&sadown,svscan))
            {
                log_warnusys("add: ",svscan," to genalloc") ;
                goto err ;
            }
        }

        log_trace("create file: ",svscan) ;
        if (!touch(svscan))
        {
            log_warnusys("create file: ",svscan) ;
            goto err ;
        }
        memcpy(svscan + svscanlen, "/event", 6) ;
        svscan[svscanlen + 6] = 0 ;
        log_trace("create fifo: ",svscan) ;
        if (!ftrigw_fifodir_make(svscan, gid, 0))
        {
            log_warnusys("create fifo: ",svscan) ;
            goto err ;
        }
        log_trace("subcribe to fifo: ",svscan) ;
        /** unsubscribe automatically, options is 0 */
        id = ftrigr_subscribe_g(&fifo, svscan, "s", 0, &deadline) ;
        if (!id)
        {
            log_warnusys("subcribe to fifo: ",svscan) ;
            goto err ;
        }
        if (!genalloc_append(uint16_t, &ids, &id)) goto err ;
    }
    if (genalloc_len(uint16_t,&ids))
    {
        log_trace("reload scandir: ",info->scandir.s) ;
        if (scandir_send_signal(info->scandir.s,"an") <= 0)
        {
            log_warnusys("reload scandir: ",info->scandir.s) ;
            goto err ;
        }

        log_trace("waiting for events on fifo") ;
        if (ftrigr_wait_and_g(&fifo, genalloc_s(uint16_t, &ids), genalloc_len(uint16_t, &ids), &deadline) < 0)
                goto err ;

        for (pos = 0 ; pos < sadown.len; pos += strlen(sadown.s + pos) + 1)
        {
            log_trace("delete down file at: ",sadown.s + pos) ;
            if (unlink(sadown.s + pos) < 0 && errno != ENOENT) goto err ;
        }

        for (pos = 0 ; pos < genalloc_len(ss_resolve_t,ga) ; pos++)
        {
            char const *string = genalloc_s(ss_resolve_t,ga)[pos].sa.s ;
            char const *name = string + genalloc_s(ss_resolve_t,ga)[pos].name  ;
            char const *state = string + genalloc_s(ss_resolve_t,ga)[pos].state  ;

            log_trace("Write state file of: ",name) ;
            if (!ss_state_write(&sta,state,name))
            {
                log_warnusys("write state file of: ",name) ;
                goto err ;
            }
            log_info("Initialized successfully: ",name) ;
        }
    }
    ftrigr_end(&fifo) ;
    stralloc_free(&sadown) ;
    genalloc_free(uint16_t, &ids) ;
    return 1 ;

    err:
        ftrigr_end(&fifo) ;
        genalloc_free(uint16_t, &ids) ;
        stralloc_free(&sadown) ;
        ftrigr_end(&fifo) ;
        return 0 ;

}
