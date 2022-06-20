/*
 * svc_init.c
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

#include <66/svc.h>

#include <string.h>
#include <stdlib.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/sastr.h>

#include <skalibs/genalloc.h>
#include <skalibs/types.h>
#include <skalibs/tai.h>
#include <skalibs/djbunix.h>
#include <skalibs/posixplz.h>//touch

#include <s6/supervise.h>
#include <s6/ftrigr.h>
#include <s6/ftrigw.h>

#include <66/utils.h>
#include <66/resolve.h>
#include <66/constants.h>
#include <66/ssexec.h>
#include <66/state.h>

#define FLAGS_INIT_EARLIER 1
#define FLAGS_INIT_DOWN (1 << 1)
/**
 * @init: array of resolve files of services
 * @len: length of the @init array
 * @flags: FLAGS_INIT_DOWN -> remove down file, FLAGS_INIT_EARLIER -> earlier services
 * */
int svc_init(ssexec_t *info, resolve_service_t *init, unsigned int len, uint8_t flags)
{
    log_flow() ;

    uint8_t earlier = FLAGS_ISSET(flags, FLAGS_INIT_EARLIER) ;
    uint8_t down = FLAGS_ISSET(flags, FLAGS_INIT_DOWN) ;
    gid_t gid = getgid() ;
    int r, e = 0 ;
    unsigned int pos = 0, nsv = 0 ;
    unsigned int real[len] ;
    unsigned int down[len] ;
    ss_state_t sta = STATE_ZERO ;

    for (; pos < len ; pos++)
        down[pos] = 0 ;

   for (pos = 0 ; pos < len ; pos++) {

        char *string = init[pos].sa.s ;
        char *name = string + init[pos].name ;
        size_t namelen = strlen(name) ;
        size_t treenamelen = strlen(init[pos].treename) ;

        if (!create_live_state(info, init[pos].treename))
            log_warnusys_return(LOG_EXIT_ZERO, "create the live directory of states") ;

        if (!earlier && s6_svc_ok(string + init[pos].runat)) {
            log_info("Skipping: ", name," -- already initialized") ;
            continue ;
        }

        char svsrc[info->base.len + SS_SYSTEM_LEN + 1 + treenamelen + SS_SVDIRS_LEN + SS_SVC_LEN + 1 + namelen + 1] ;
        auto_strings(svsrc, info->base.s, SS_SYSTEM_LEN, "/", treenamelen, SS_SVDIRS, SS_SVC, "/", name) ;

        size_t runatlen = string + init[pos].runat ;
        char scandir[runatlen + 6 + 1] ; // +6 event directory name
        auto_strings(scandir, string + init[pos].runat) ;

        r = scan_mode(scandir, S_IFDIR) ;
        if (r < 0)
            log_warnsys_return(LOG_EXIT_ZERO, "conflicting format for: ", scandir) ;

        if (!r) {

            log_trace("copy: ",svsrc, " to ", scandir) ;
            if (!hiercopy(svsrc, scandir))
                log_warnusys_return(LOG_EXIT_ZERO, "copy: ",svsrc," to: ",scandir) ;
        }

        auto_strings(scandir + runatlen, "/down") ;
        fd = open_trunc(scandir) ;
        if (fd < 0)
            log_warnusys_return(LOG_EXIT_ZERO, "create down file: ", scandir) ;
        fd_close(fd) ;

        if (!init[pos].down)
            down[nsv] = 1 ;

        if (!earlier) {

            auto_strings(scandir + runatlen, "/event") ;

            log_trace("create fifo: ",scandir) ;
            if (!ftrigw_fifodir_make(scandir, gid, 0))
                log_warnusys_return(LOG_EXIT_ZERO, "create fifo: ",scandir) ;
        }

        real[nsv++] = init[pos] ;
    }

   if (!earlier) {

        ftrigr_t fifo = FTRIGR_ZERO ;
        uint16_t ids[len] ;
        unsigned int nids = 0 ;

        tain deadline ;
        tain_now_set_stopwatch_g() ;
        tain_addsec(&deadline, &STAMP, 2) ;

        if (!ftrigr_startf_g(&fifo, &deadline))
            goto err ;

        for (pos = 0 ; pos < nsv ; pos++) {

            char *string = init[real[pos]].sa.s ;
            size_t runatlen = string + init[real[pos]].runat ;
            char scandir[runatlen + 6 + 1] ; // +6 event directory name
            auto_strings(scandir, string + init[real[pos]].runat) ;

            log_trace("subcribe to fifo: ", scandir) ;
            /** unsubscribe automatically, options is 0 */
            ids[nids] = ftrigr_subscribe_g(&fifo, scandir, "s", 0, &deadline) ;
            if (!ids[nids++]) {
                log_warnusys("subcribe to fifo: ", scandir) ;
                goto err ;
            }
        }

        if (nids) {
            log_trace("reload scandir: ", info->scandir.s) ;
            if (scandir_send_signal(info->scandir.s, "h") <= 0) {
                log_warnusys("reload scandir: ", info->scandir.s) ;
                goto err ;
            }

            log_trace("waiting for events on fifo") ;
            if (ftrigr_wait_and_g(&fifo, ids, nids, &deadline) < 0)
                goto err ;
        }
    }

    for (pos = 0 ; pos < nsv ; pos++) {

        char const *string = init[real[pos]].sa.s ;
        char const *name = string + init[real[pos]].name ;
        char const *state = string + init[real[pos]].state  ;

        log_trace("Write state file of: ", name) ;
        state_setflag(&sta, SS_FLAGS_RELOAD, SS_FLAGS_FALSE) ;
        state_setflag(&sta, SS_FLAGS_INIT, SS_FLAGS_FALSE) ;
        state_setflag(&sta, SS_FLAGS_STATE, SS_FLAGS_UNKNOWN) ;
        state_setflag(&sta, SS_FLAGS_PID, SS_FLAGS_UNKNOWN) ;

        if (!state_write(&sta,state,name)) {

            log_warnusys("write state file of: ",name) ;
            goto err ;
        }

        if (down[pos] && down) {

            size_t runatlen = string + init[down[pos]].runat ;
            char file[runatlen + 5 + 1] ;
            auto_strings(file, string + init[down[pos]].runat, "/down") ;

            log_trace("delete down file: ", file) ;
            if (unlink(file) < 0 && errno != ENOENT) {
                log_warnusys("delete down file: ", file)
            }
        }
        log_info("Initialized successfully: ",name) ;
    }

    e = 1 ;

    err:
        ftrigr_end(&fifo) ;
        return e ;
}
