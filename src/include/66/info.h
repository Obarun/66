/*
 * info.h
 *
 * Copyright (c) 2019 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#ifndef SS_INFO_H
#define SS_INFO_H

#include <wchar.h>

#include <oblibs/strbuf.h>

#include <66/service.h>
#include <66/status.h>
#include <66/tree.h>
#include <66/graph.h>

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

extern unsigned int INFO_MAXDEPTH ;
extern info_graph_style *STYLE ;
extern info_graph_style graph_utf8 ;
extern info_graph_style graph_default ;

/* longest rendering is "<uint64 days> <h>h", the uint64 taking 20 digits */
#define INFO_DURATION_LEN 31

/**
 * @brief Render a duration as a human-readable string, two units at most
 *        ("3s", "2min 5s", "9h 40min", "3 days 4h"); the lesser unit is
 *        omitted when zero.
 * @param[out] s     Receives the NUL-terminated rendering; must hold at least
 *                   INFO_DURATION_LEN + 1 bytes.
 * @param[in] secs   The duration, in seconds.
 * @return the length of the rendering, NUL excluded.
 */
extern size_t info_fmt_duration(char *s, uint64_t secs) ;

extern int info_getcols_fd(int fd) ;
extern void info_field_align (char buf[][INFO_FIELD_MAXLEN],char fields[][INFO_FIELD_MAXLEN],wchar_t const field_suffix[],size_t buflen) ;
extern size_t info_length_from_wchar(char const *str) ;
extern size_t info_display_field_name(char const *field) ;
extern void info_display_list(char const *field, strbuf *list) ;
extern void info_display_nline(char const *field,char const *str) ;

/* Generic resolve-file display engine. A resolve struct stores each cdb field
 * inline: a string is a uint32 offset into its blob, an integer is the value
 * itself, a limit is a uint64. One row per cdb key describes how to render it. */

enum info_field_type_e { INFO_FIELD_STR, INFO_FIELD_U32, INFO_FIELD_U64, INFO_FIELD_FLAG } ;

typedef struct info_field_s info_field_t ;
struct info_field_s {
    char const *key ;       // cdb key, also what -f matches
    uint8_t type ;          // info_field_type_e
    size_t offset ;         // offsetof the member in the core resolve OR the addon struct
    uint8_t addon ;         // 0 = core resolve; else the addon id (DATA_SERVICE_LIMIT, ...)
} ;

/* a loaded addon a field may live in; indexed by the addon id used in the
 * field table. base==0 means the addon is absent (the field renders as default). */
typedef struct info_addon_s info_addon_t ;
struct info_addon_s {
    void const *base ;      // the loaded addon struct, or 0 if absent
    char const *blob ;     // the addon's string blob
} ;

/* Shared field-listing engine: -f selection, name alignment and the noname
 * branch live here once. A caller passes the field keys and a writer that
 * prints the value (followed by a newline) of the field at a given index;
 * the keys are the only thing the engine knows about the field layout.
 * The engine owns the alignment, so it hands the writer the padded label it
 * printed: a value spanning several lines needs that width to indent its
 * continuations under the first one. */

typedef void info_value_writer(void *ctx, size_t index, char const *label) ;
typedef info_value_writer *info_value_writer_t_ref ;

/**
 * @brief Select, align and display a set of named fields.
 * @param[in] keys     Field keys, one per field, in display order; what -f matches.
 * @param[in] labels   Printed field names, one per key, same order. A debug dump
 *                     passes @keys here; a human-facing command passes its own
 *                     wording. Never 0.
 * @param[in] nfields  Number of keys.
 * @param[in] select   Comma-separated list of keys to show, or 0 for all.
 * @param[in] noname   If non-zero, print only the values, not the field names.
 * @param[in] write    Writer printing the value of field @index, newline
 *                     included. Receives the padded label already printed for
 *                     that field, or 0 when @noname is set.
 * @param[in] ctx      Opaque context handed back to @write.
 */
extern void info_fields_display(char const *const *keys, char const *const *labels, size_t nfields, char const *select, uint8_t noname, info_value_writer *write, void *ctx) ;

/**
 * @brief Display the fields of a resolve struct from a declarative table.
 * @param[in] base     Pointer to the core resolve struct.
 * @param[in] blob    The core struct's string blob (res->sa.s).
 * @param[in] fields   Field table, one row per cdb key, in display order.
 * @param[in] nfields  Number of rows in @fields.
 * @param[in] select   Comma-separated list of keys to show, or 0 for all.
 * @param[in] noname   If non-zero, print only the values, not the field names.
 * @param[in] addons   Loaded addons indexed by the addon id used in @fields; an
 *                     entry with base==0 renders its fields as the default. May
 *                     be 0 when @fields references core fields only.
 * @param[in] naddons  Number of entries in @addons; an addon id >= this is
 *                     treated as absent.
 */
extern void info_resolve_display(void const *base, char const *blob, info_field_t const *fields, size_t nfields, char const *select, uint8_t noname, info_addon_t const *addons, size_t naddons) ;

/**
 * @brief Display the runtime status record of a service.
 * @param[in] st       The status record to display.
 * @param[in] select   Comma-separated list of keys to show, or 0 for all.
 * @param[in] noname   If non-zero, print only the values, not the field names.
 */
extern void info_status_display(service_status_t const *st, char const *select, uint8_t noname) ;

extern depth_t info_graph_init(void) ;
extern int service_info_walk(service_graph_t *g, char const *name, char const *treename, uint8_t requiredby, uint8_t reverse, depth_t *depth, int padding, info_graph_style *style, ssexec_t *info) ;
extern int tree_info_walk(tree_graph_t *g, char const *name, uint8_t requiredby, uint8_t reverse, depth_t *depth, int padding, info_graph_style *style, ssexec_t *info) ;
extern int info_graph_display(char const *name, info_graph_func *func, depth_t *depth, int last, int padding, info_graph_style *style) ;
extern int info_graph_display_service(char const *name) ;
extern int info_graph_display_tree(char const *name) ;

#endif
