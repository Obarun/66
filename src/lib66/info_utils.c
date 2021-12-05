/*
 * info_utils.c
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

#include <66/info.h>

#include <unistd.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <wchar.h>

#include <string.h>
#include <sys/types.h> //ssize_t

#include <oblibs/sastr.h>
#include <oblibs/log.h>

#include <skalibs/buffer.h>
#include <skalibs/lolstdio.h>

#include <s6/supervise.h>

#include <66/resolve.h>
#include <66/state.h>

unsigned int MAXDEPTH = 1 ;

info_graph_style graph_utf8 = {
    UTF_VR UTF_H,
    UTF_UR UTF_H,
    UTF_V " ",
    2
} ;

info_graph_style graph_default = {
    "|-",
    "`-",
    "|",
    2
} ;

int info_getcols_fd(int fd)
{
    int width = -1;

    if(!isatty(fd)) return 0;

#if defined(TIOCGSIZE)
    struct ttysize win;
    if(ioctl(fd, TIOCGSIZE, &win) == 0)
        width = win.ts_cols;
#elif defined(TIOCGWINSZ)
    struct winsize win;
    if(ioctl(fd, TIOCGWINSZ, &win) == 0)
        width = win.ws_col;
#endif

    // return abitrary value
    if(width <= 0) return 100 ;

    return width;
}

void info_field_align (char buf[][INFO_FIELD_MAXLEN],char fields[][INFO_FIELD_MAXLEN],wchar_t const field_suffix[],size_t buflen)
{
    log_flow() ;

    size_t a = 0, b = 0, maxlen = 0, wlen[buflen], len = INFO_FIELD_MAXLEN+nb_el(field_suffix) ;

    int maxcol = 0, wcol[buflen] ;

    wchar_t wbuf[buflen][len] ;

    for(a = 0; a < buflen; a++)
        for (b = 0; b < len; b++)
            wbuf[a][b] = 0 ;

    for(a = 0; a < buflen; a++)
    {
        wlen[a] = mbstowcs(wbuf[a], buf[a], strlen(buf[a]) + 1) ;
        wcol[a] = wcswidth(wbuf[a], wlen[a]) ;
        if(wcol[a] > maxcol) {
            maxcol = wcol[a] ;
        }
        if(wlen[a] > maxlen) {
            maxlen = wlen[a] ;
        }
    }

    for(a = 0; a < buflen; a++)
    {
        size_t padlen = maxcol - wcol[a] ;
        wmemset(wbuf[a] + wlen[a], L' ', padlen) ;
        wmemcpy(wbuf[a] + wlen[a] + padlen, field_suffix, nb_el(field_suffix)) ;
        wcstombs(fields[a], wbuf[a], sizeof(wbuf[a])) ;
    }
}

size_t info_length_from_wchar(char const *str)
{
    log_flow() ;

    ssize_t len ;
    wchar_t *wcstr ;
    if(!str || !str[0]) return 0 ;

    len = strlen(str) + 1 ;
    wcstr = calloc(len, sizeof(wchar_t)) ;
    len = mbstowcs(wcstr, str, len) ;
    len = wcswidth(wcstr, len) ;
    free(wcstr) ;

    return len == -1 ? 0 : (size_t)len  ;
}

size_t info_display_field_name(char const *field)
{
    log_flow() ;

    size_t len = 0 ;
    if(field)
    {
        len = info_length_from_wchar(field) + 1 ;
        if (!bprintf(buffer_1,"%s%s%s ", log_color->info, field, log_color->off)) log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
    }
    return len ;
}

void info_display_list(char const *field, stralloc *list)
{
    log_flow() ;

    size_t a = 0 , b, cols, padding = 0, slen = 0 ;

    unsigned short maxcols = info_getcols_fd(1) ;

    padding = info_length_from_wchar(field) + 1 ;

    cols = padding ;

    for (; a < list->len ; a += strlen(list->s + a) + 1)
    {
        char const *str = list->s + a ;
        slen = info_length_from_wchar(str) ;
        if((maxcols > padding) && (cols + slen + 2 >= maxcols))
        {
            cols = padding ;
            if (buffer_puts(buffer_1,"\n") == -1) goto err ;
            for(b = 1 ; b <= padding ; b++)
                if (buffer_puts(buffer_1," ") == -1) goto err ;
        }
        else if (cols != padding)
        {
            if (buffer_puts(buffer_1," ") == -1) goto err ;
            cols += 2 ;
        }
        if (!bprintf(buffer_1,"%s",str)) goto err ;
        cols += slen ;
    }
    if (buffer_puts(buffer_1,"\n") == -1) goto err ;

    return ;
    err:
        log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
}

void info_display_nline(char const *field,char const *str)
{
    log_flow() ;

    size_t pos = 0, padding = info_length_from_wchar(field) + 1, len ;

    stralloc sa = STRALLOC_ZERO ;

    if (!auto_stra(&sa, str))
        log_die_nomem("stralloc") ;

    if (!sastr_split_string_in_nline(&sa))
        log_dieu(LOG_EXIT_SYS,"split string in nline") ;

    len = sa.len ;
    char tmp[sa.len + 1] ;
    sastr_to_char(tmp, &sa) ;

    for (;pos < len ; pos += strlen(tmp + pos) + 1) {

        sa.len = 0 ;

        if (!auto_stra(&sa,tmp + pos))
            log_die_nomem("stralloc") ;

        if (field) {
            if (pos) {
                if (!bprintf(buffer_1,"%*s",padding,""))
                    log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
            }
        }
        info_display_list(field,&sa) ;
    }
    stralloc_free(&sa) ;
}

void info_graph_display(resolve_service_t *res, depth_t *depth, int last, int padding, info_graph_style *style)
{
    log_flow() ;

    int level = 1 ;
    char str_pid[UINT_FMT] ;
    uint8_t pid_color = 0 ;
    const char *tip = "" ;
    char *ppid ;
    ss_state_t sta = STATE_ZERO ;
    s6_svstatus_t status = S6_SVSTATUS_ZERO ;
    char *name = res->sa.s + res->name ;

    if (res->type == TYPE_CLASSIC || res->type == TYPE_LONGRUN)
    {
        s6_svstatus_read(res->sa.s + res->runat ,&status) ;
        pid_color = !status.pid ? 1 : 2 ;
        str_pid[uint_fmt(str_pid, status.pid)] = 0 ;
        ppid = &str_pid[0] ;
    }
    else
    {
        char *ste = res->sa.s + res->state ;
        char *name = res->sa.s + res->name ;
        if (!state_check(ste,name))
        {
            ppid = "unitialized" ;
            goto dis ;
        }

        if (!state_read(&sta,ste,name))
            log_dieusys(LOG_EXIT_SYS,"read state of: ",name) ;

        if (sta.init) {
            ppid = "unitialized" ;
        }
        else if (!sta.state)
        {
            ppid = "down" ;
            pid_color = 1 ;
        }
        else if (sta.state)
        {
            ppid = "up" ;
            pid_color = 2 ;
        }
    }
    dis:

    tip = last ? style->last : style->tip ;

    while(depth->prev)
        depth = depth->prev ;

    while(depth->next)
    {
        if (!bprintf(buffer_1,"%*s%-*s",style->indent * (depth->level - level) + (level == 1 ? padding : 0), "", style->indent, style->limb)) return ;
        level = depth->level + 1 ;
        depth = depth->next ;
    }

    if (!bprintf(buffer_1,"%*s%*s%s(%s%s%s,%s%s%s,%s) %s", \
                level == 1 ? padding : 0,"", \
                style->indent * (depth->level - level), "", \
                tip, \
                pid_color > 1 ? log_color->valid : pid_color ? log_color->error : log_color->warning, \
                ppid, \
                log_color->off, \
                res->disen ? log_color->off : log_color->error, \
                res->disen ? "Enabled" : "Disabled", \
                log_color->off, \
                get_key_by_enum(ENUM_TYPE,res->type), \
                name)) return ;

    if (buffer_putsflush(buffer_1,"\n") < 0) return ;
}

int info_walk(resolve_service_t *res,char const *src,int reverse, depth_t *depth, int padding, info_graph_style *style)
{
    log_flow() ;

    size_t pos = 0, idx = 0 ;
    stralloc sadeps = STRALLOC_ZERO ;
    resolve_service_t dres = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(SERVICE_STRUCT, &dres) ;
    if((!res->ndeps) || (depth->level > MAXDEPTH))
        goto freed ;

    if (!sastr_clean_string(&sadeps,res->sa.s + res->deps)) goto err ;
    if (reverse) sastr_reverse(&sadeps) ;

    for(; pos < sadeps.len ; pos += strlen(sadeps.s + pos) + 1,idx++ )
    {
        int last =  idx + 1 < res->ndeps  ? 0 : 1 ;
        char *name = sadeps.s + pos ;

        if (!resolve_check(src,name)) goto err ;
        if (!resolve_read(wres,src,name)) goto err ;

        info_graph_display(&dres, depth, last,padding,style) ;

        if (dres.ndeps)
        {
            depth_t d =
            {
                depth,
                NULL,
                depth->level + 1
            } ;
            depth->next = &d;

            if(last)
            {
                if(depth->prev)
                {
                    depth->prev->next = &d;
                    d.prev = depth->prev;
                    depth = &d;

                }
                else
                    d.prev = NULL;
            }
            if (!info_walk(&dres, src, reverse, &d, padding, style)) goto err;
            depth->next = NULL;
        }
    }
    freed:
    resolve_free(wres) ;
    stralloc_free(&sadeps) ;
    return 1 ;
    err:
        resolve_free(wres) ;
        stralloc_free(&sadeps) ;
        return 0 ;
}

int info_graph_init (resolve_service_t *res,char const *src,unsigned int reverse, int padding, info_graph_style *style)
{
    log_flow() ;

    depth_t d = {
        NULL,
        NULL,
        1
    } ;

    if(!info_walk(res,src,reverse,&d,padding,style)) return 0 ;

    return 1 ;
}
