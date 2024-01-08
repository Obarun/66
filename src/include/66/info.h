/*
 * info.h
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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
#include <oblibs/graph.h>

#include <66/enum.h>
#include <66/service.h>


#ifndef SS_INFO_H
#define SS_INFO_H

#define INFO_FIELD_MAXLEN 30
#define INFO_NKEY 150

typedef int info_graph_func(char const *name) ;
typedef info_graph_func *info_graph_func_t_ref ;

typedef struct depth_s depth_t ;
struct depth_s
{
    depth_t *prev ;
    depth_t *next ;
    int level ;
} ;

typedef void info_opts_func_t (char const *field,char const *treename) ;
typedef info_opts_func_t *info_opts_func_t_ref ;
typedef void info_opts_svfunc_t (char const *field,resolve_service_t *res) ;
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

typedef struct info_graph_style_s info_graph_style ;
struct info_graph_style_s
{
    const char *tip;
    const char *last;
    const char *limb;
    int indent;
} ;

extern unsigned int MAXDEPTH ;
extern info_graph_style *STYLE ;
extern info_graph_style graph_utf8 ;
extern info_graph_style graph_default ;

extern int info_getcols_fd(int fd) ;
extern void info_field_align (char buf[][INFO_FIELD_MAXLEN],char fields[][INFO_FIELD_MAXLEN],wchar_t const field_suffix[],size_t buflen) ;
extern size_t info_length_from_wchar(char const *str) ;
extern size_t info_display_field_name(char const *field) ;
extern void info_display_list(char const *field, stralloc *list) ;
extern void info_display_nline(char const *field,char const *str) ;

extern depth_t info_graph_init(void) ;
extern int info_walk(graph_t *g, char const *name, char const *obj, info_graph_func *func, uint8_t requiredby, uint8_t reverse, depth_t *depth, int padding, info_graph_style *style) ;
extern int info_graph_display(char const *name, info_graph_func *func, depth_t *depth, int last, int padding, info_graph_style *style) ;
extern int info_graph_display_service(char const *name) ;
extern int info_graph_display_tree(char const *name) ;

#endif
