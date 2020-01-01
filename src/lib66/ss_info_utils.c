/* 
 * ss_info_utils.c
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
 * */
 
#include <66/info.h>

#include <unistd.h>
#include <sys/ioctl.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> //ssize_t

#include <oblibs/sastr.h>
#include <oblibs/log.h>

#include <skalibs/buffer.h>
#include <skalibs/lolstdio.h>

#include <s6/s6-supervise.h>

#include <66/resolve.h>

ss_resolve_graph_style graph_utf8 = {
	UTF_VR UTF_H,
	UTF_UR UTF_H,
	UTF_V " ",
	2
} ;

ss_resolve_graph_style graph_default = {
	"|-",
	"`-",
	"|",
	2
} ;

int info_getcols_fd(int fd)
{
	int width = -1;

	if(!isatty(fd))	return 0;

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
	unsigned int i ;
	
	size_t maxlen = 0 ;
	
	int maxcol = 0 ;
	
	static wchar_t wbuf[][INFO_FIELD_MAXLEN+nb_el(field_suffix)] = {{ 0 }} ;
	
	size_t wlen[buflen] ;
	
	int wcol[buflen] ;

	for(i = 0; i < buflen; i++)
	{
		wlen[i] = mbstowcs(wbuf[i], buf[i], strlen(buf[i]) + 1) ;
		wcol[i] = wcswidth(wbuf[i], wlen[i]) ;
		if(wcol[i] > maxcol) {
			maxcol = wcol[i] ;
		}
		if(wlen[i] > maxlen) {
			maxlen = wlen[i] ;
		}
	}

	for(i = 0; i < buflen; i++)
	{
		size_t padlen = maxcol - wcol[i] ;
		wmemset(wbuf[i] + wlen[i], L' ', padlen) ;
		wmemcpy(wbuf[i] + wlen[i] + padlen, field_suffix, nb_el(field_suffix)) ;
		wcstombs(fields[i], wbuf[i], sizeof(wbuf[i])) ;
	}
}

size_t info_length_from_wchar(char const *str)
{
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
	size_t pos = 0, padding = info_length_from_wchar(field) + 1 ;
	stralloc tmp = STRALLOC_ZERO ;
	stralloc cp = STRALLOC_ZERO ;
	if (!stralloc_cats(&tmp,str) ||
	!stralloc_0(&tmp)) log_die_nomem("stralloc") ;
	if (!sastr_split_string_in_nline(&tmp)) log_dieu(LOG_EXIT_SYS,"split string in nline") ;
	for (;pos < tmp.len ; pos += strlen(tmp.s + pos) + 1)
	{
		cp.len = 0 ;
		if (!stralloc_cats(&cp,tmp.s + pos) ||
		!stralloc_0(&cp)) log_die_nomem("stralloc") ;
		if (field) 
		{
			if (pos)
			{
				if (!bprintf(buffer_1,"%*s",padding,""))
					log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
			}
		}
		info_display_list(field,&cp) ;
	}
	stralloc_free(&tmp) ;
	stralloc_free(&cp) ;
}

void info_graph_display(ss_resolve_t *res, depth_t *depth, int last, int padding, ss_resolve_graph_style *style)
{
	s6_svstatus_t status = S6_SVSTATUS_ZERO ;
	char *name = res->sa.s + res->name ;
	if (res->type == CLASSIC || res->type == LONGRUN)
	{
		s6_svstatus_read(res->sa.s + res->runat ,&status) ;
	}
	else status.pid = 0 ;	
	
	const char *tip = "" ;
	int level = 1 ;
	
	tip = last ? style->last : style->tip ;
	
	while(depth->prev)
		depth = depth->prev ;
		
	while(depth->next)
	{
		if (!bprintf(buffer_1,"%*s%-*s",style->indent * (depth->level - level) + (level == 1 ? padding : 0), "", style->indent, style->limb)) return ;
		level = depth->level + 1 ;
		depth = depth->next ;
	} 
	
	if (!bprintf(buffer_1,"%*s%*s%s(%s%i%s,%s%s%s,%s) %s",level == 1 ? padding : 0,"", style->indent * (depth->level - level), "", tip, status.pid ? log_color->valid : log_color->off,status.pid ? status.pid : 0,log_color->off, res->disen ? log_color->off : log_color->error, res->disen ? "Enabled" : "Disabled",log_color->off,get_keybyid(res->type), name)) return ;

	if (buffer_putsflush(buffer_1,"\n") < 0) return ; 
}

int info_walk(ss_resolve_t *res,char const *src,int reverse, depth_t *depth, int padding, ss_resolve_graph_style *style)
{
	size_t pos = 0, idx = 0 ;
	stralloc sadeps = STRALLOC_ZERO ;
	ss_resolve_t dres = RESOLVE_ZERO ;
	
	if((!res->ndeps) || (depth->level > MAXDEPTH))
		goto freed ;
	
	if (!sastr_clean_string(&sadeps,res->sa.s + res->deps)) goto err ;
	if (reverse) sastr_reverse(&sadeps) ;

	for(; pos < sadeps.len ; pos += strlen(sadeps.s + pos) + 1,idx++ )
	{	
		int last =  idx + 1 < res->ndeps  ? 0 : 1 ;		
		char *name = sadeps.s + pos ;
		
		if (!ss_resolve_check(src,name)) goto err ;	
		if (!ss_resolve_read(&dres,src,name)) goto err ;
		
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
	ss_resolve_free(&dres) ;
	stralloc_free(&sadeps) ;
	return 1 ;
	err: 
		ss_resolve_free(&dres) ;
		stralloc_free(&sadeps) ;
		return 0 ;
}

int info_graph_init (ss_resolve_t *res,char const *src,unsigned int reverse, int padding, ss_resolve_graph_style *style)
{
	depth_t d = {
		NULL,
		NULL,
		1
	} ;	

	if(!info_walk(res,src,reverse,&d,padding,style)) return 0 ;
	
	return 1 ;
}
