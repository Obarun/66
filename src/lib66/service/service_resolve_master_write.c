/*
 * service.c
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

#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/sastr.h>

#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>//openreadnclose

#include <66/constants.h>
#include <66/ssexec.h>
#include <66/resolve.h>
#include <66/service.h>

int service_resolve_master_write(ssexec_t *info, ss_resolve_graph_t *graph,char const *dir, unsigned int reverse)
{
    log_flow() ;

    int r, e = 0 ;
    size_t i = 0 ;
    char ownerstr[UID_FMT] ;
    size_t ownerlen = uid_fmt(ownerstr,info->owner) ;
    ownerstr[ownerlen] = 0 ;

    stralloc in = STRALLOC_ZERO ;
    stralloc inres = STRALLOC_ZERO ;
    stralloc gain = STRALLOC_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    size_t dirlen = strlen(dir) ;

    resolve_init(wres) ;

    char runat[info->livetree.len + 1 + info->treename.len + SS_SVDIRS_LEN + SS_MASTER_LEN + 1] ;
    auto_strings(runat, info->livetree.s, "/", info->treename.s, SS_SVDIRS, SS_MASTER) ;

    char dst[dirlen + SS_DB_LEN + SS_SRC_LEN + SS_MASTER_LEN + 1] ;
    auto_strings(dst, dir, SS_DB, SS_SRC, SS_MASTER) ;

    size_t livelen = info->live.len - 1 ;
    char state[livelen + SS_STATE_LEN + 1 + ownerlen + 1 + info->treename.len + 1] ;
    auto_strings(state, info->live.s, SS_STATE, "/", ownerstr, "/", info->treename.s) ;

    if (reverse)
    {
        size_t dstlen = strlen(dst) ;
        char file[dstlen + 1 + SS_CONTENTS_LEN + 1] ;
        auto_strings(file, dst, "/", SS_CONTENTS) ;

        size_t filesize=file_get_size(file) ;

        r = openreadfileclose(file,&gain,filesize) ;
        if(!r) goto err ;
        /** ensure that we have an empty line at the end of the string*/
        if (!auto_stra(&gain,"\n")) goto err ;
        if (!sastr_clean_element(&gain)) goto err ;
    }

    for (; i < genalloc_len(resolve_service_t,&graph->sorted); i++)
    {
        char *string = genalloc_s(resolve_service_t,&graph->sorted)[i].sa.s ;
        char *name = string + genalloc_s(resolve_service_t,&graph->sorted)[i].name ;
        if (reverse)
            if (sastr_cmp(&gain,name) == -1) continue ;

        if (!stralloc_cats(&in,name)) goto err ;
        if (!stralloc_cats(&in,"\n")) goto err ;

        if (!stralloc_cats(&inres,name)) goto err ;
        if (!stralloc_cats(&inres," ")) goto err ;
    }

    if (inres.len) inres.len--;
    if (!stralloc_0(&inres)) goto err ;

    r = file_write_unsafe(dst,SS_CONTENTS,in.s,in.len) ;
    if (!r)
    {
        log_warnusys("write: ",dst,"contents") ;
        goto err ;
    }

    res.name = resolve_add_string(wres,SS_MASTER+1) ;
    res.description = resolve_add_string(wres,"inner bundle - do not use it") ;
    res.treename = resolve_add_string(wres,info->treename.s) ;
    res.tree = resolve_add_string(wres,info->tree.s) ;
    res.live = resolve_add_string(wres,info->live.s) ;
    res.type = TYPE_BUNDLE ;
    res.depends = resolve_add_string(wres,inres.s) ;
    res.ndepends = genalloc_len(resolve_service_t,&graph->sorted) ;
    res.runat = resolve_add_string(wres,runat) ;
    res.state = resolve_add_string(wres,state) ;

    if (!resolve_write(wres,dir,SS_MASTER+1)) goto err ;

    e = 1 ;

    err:
        resolve_free(wres) ;
        stralloc_free(&in) ;
        stralloc_free(&inres) ;
        stralloc_free(&gain) ;
        return e ;
}

