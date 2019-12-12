/* 
 * info.h
 * 
 * Copyright (c) 2018-2019 Eric Vidal <eric@obarun.org>
 * 
 * All rights reserved.
 * 
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <wchar.h>

#include <oblibs/string.h>
#include <oblibs/log.h>

#include <66/enum.h>
#include <66/resolve.h>

 
#ifndef SS_INFO_H
#define SS_INFO_H

#define INFO_FIELD_MAXLEN 30

typedef struct depth_s depth_t ;
struct depth_s
{
	depth_t *prev ;
	depth_t *next ;
	int level ;
} ;

typedef void info_opts_func_t (char const *field,char const *treename) ;
typedef info_opts_func_t *info_opts_func_t_ref ;
typedef void info_opts_svfunc_t (char const *field,ss_resolve_t *res) ;
typedef info_opts_svfunc_t *info_opts_svfunc_t_ref ;

typedef struct info_opts_map_s info_opts_map_t ;
struct info_opts_map_s
{
	char const *str ;
	info_opts_func_t *func ;
	info_opts_svfunc_t *svfunc ;
	unsigned int id ;
} ;

#define UTF_V   "\342\224\202"  /* U+2502, Vertical line drawing char */
#define UTF_VR  "\342\224\234"  /* U+251C, Vertical and right */
#define UTF_H   "\342\224\200"  /* U+2500, Horizontal */
#define UTF_UR  "\342\224\224"  /* U+2514, Up and right */

typedef struct ss_resolve_graph_style_s ss_resolve_graph_style ;
struct ss_resolve_graph_style_s
{
	const char *tip;
	const char *last;
	const char *limb;
	int indent;
} ;

extern unsigned int MAXDEPTH ;
extern ss_resolve_graph_style *STYLE ; 
extern ss_resolve_graph_style graph_utf8 ;
extern ss_resolve_graph_style graph_default ;

extern void info_field_align (char buf[][INFO_FIELD_MAXLEN],char fields[][INFO_FIELD_MAXLEN],wchar_t const field_suffix[],size_t buflen) ;
extern int info_getcols_fd(int fd) ;
extern size_t info_length_from_wchar(char const *str) ;
extern void info_graph_display(ss_resolve_t *res, depth_t *depth, int last,int padding, ss_resolve_graph_style *style) ;
extern int info_graph_init (ss_resolve_t *res,char const *src,unsigned int reverse, int padding, ss_resolve_graph_style *style) ;
extern int info_walk(ss_resolve_t *res,char const *src,int reverse, depth_t *depth, int padding, ss_resolve_graph_style *style) ;
extern size_t info_display_field_name(char const *field) ;
extern void info_display_list(char const *field, stralloc *list) ;
extern void info_display_nline(char const *field,char const *str) ;
#endif
