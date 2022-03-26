/*
 * info_graph_display_service.c
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
 * */

#include <stdint.h>

#include <oblibs/log.h>

#include <skalibs/stralloc.h>
#include <skalibs/lolstdio.h>
#include <skalibs/buffer.h>

#include <66/info.h>
#include <66/service.h>
#include <66/tree.h>
#include <66/state.h>
#include <66/enum.h>

#include <s6/supervise.h>

int info_graph_display_service(char const *name, char const *obj)
{
    log_flow() ;

    stralloc tree = STRALLOC_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    int r = service_intree(&tree, name, obj), err = 0 ;

    if (r != 2) {
        if (r == 1)
            log_warnu("find: ", name, " at tree: ", !obj ? tree.s : obj) ;
        if (r > 2)
            log_1_warn(name, " is set on different tree -- please use -t options") ;

        goto freed ;
    }

    if (!resolve_check(tree.s, name))
        goto freed ;

    if (!resolve_read(wres, tree.s, name))
        goto freed ;

    char str_pid[UINT_FMT] ;
    uint8_t pid_color = 0 ;
    char *ppid ;
    ss_state_t sta = STATE_ZERO ;
    s6_svstatus_t status = S6_SVSTATUS_ZERO ;

    if (res.type == TYPE_CLASSIC || res.type == TYPE_LONGRUN) {

        s6_svstatus_read(res.sa.s + res.runat ,&status) ;
        pid_color = !status.pid ? 1 : 2 ;
        str_pid[uint_fmt(str_pid, status.pid)] = 0 ;
        ppid = &str_pid[0] ;

    } else {

        char *ste = res.sa.s + res.state ;
        char *name = res.sa.s + res.name ;
        if (!state_check(ste,name)) {

            ppid = "unitialized" ;
            goto dis ;
        }

        if (!state_read(&sta,ste,name)) {

            log_warnu("read state of: ",name) ;
            goto freed ;
        }
        if (sta.init) {

            ppid = "unitialized" ;
            goto dis ;

        } else if (!sta.state) {

            ppid = "down" ;
            pid_color = 1 ;

        } else if (sta.state) {

            ppid = "up" ;
            pid_color = 2 ;
        }
    }

    dis:

    if (!bprintf(buffer_1,"(%s%s%s,%s%s%s,%s) %s", \

                pid_color > 1 ? log_color->valid : pid_color ? log_color->error : log_color->warning, \
                ppid, \
                log_color->off, \

                res.disen ? log_color->off : log_color->error, \
                res.disen ? "Enabled" : "Disabled", \
                log_color->off, \

                get_key_by_enum(ENUM_TYPE,res.type), \

                name))
                    goto freed ;

    err = 1 ;

    freed:
        resolve_free(wres) ;
        stralloc_free(&tree) ;

    return err ;

}
