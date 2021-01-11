/*
 * ss_instance.c
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

#include <66/utils.h>

#include <string.h>

#include <oblibs/log.h>
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/directory.h>
#include <oblibs/environ.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/enum.h>

/** New functions */

int instance_check(char const *svname)
{
    log_flow() ;

    int r ;
    r = get_len_until(svname,'@') ;
    // avoid empty value after the instance template name
    if (strlen(svname+r) <= 1 && r > 0) return 0 ;

    return r ;
}

int instance_splitname(stralloc *sa,char const *name,int len,int what)
{
    log_flow() ;

    char const *copy ;
    size_t tlen = len + 1 ;

    char template[tlen + 1] ;
    memcpy(template,name,tlen) ;
    template[tlen] = 0 ;

    copy = name + tlen ;

    sa->len = 0 ;
    if (!what)
    {
        if (!stralloc_cats(sa,template) ||
        !stralloc_0(sa)) return 0 ;
    }
    else
    {
        if (!stralloc_catb(sa,copy,strlen(copy)) ||
        !stralloc_0(sa)) return 0 ;
    }
    return 1 ;
}

int instance_create(stralloc *sasv,char const *svname, char const *regex, int len)
{
    log_flow() ;

    char const *copy ;
    size_t tlen = len + 1 ;

    stralloc tmp = STRALLOC_ZERO ;

    if (!auto_stra(&tmp,sasv->s)) goto err ;

    copy = svname + tlen ;

    if (!sastr_replace_all(&tmp,regex,copy)){
        log_warnu("replace instance character for service: ",svname) ;
        goto err ;
    }
    if (!stralloc_copy(sasv,&tmp)) goto err ;
    stralloc_free(&tmp) ;

    return 1 ;
    err:
        stralloc_free(&tmp) ;
        return 0 ;
}
