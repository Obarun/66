/*
 * version.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 *
 * These following functions are largely inspired by the libversion project
 * (https://github.com/repology/libversion) used by the repology
 * site https://repology.org/. The libversion is under MIT license
 * and was written by Dmitry Marakasov <amdmi3@amdmi3.ru>.
 *
 * The code was rewritten entirely an simplify to match the needs of 66.
 */

#include <string.h>
#include <errno.h>

#include <66/utils.h>
#include <66/constants.h>

#define VMIN(a, b) ((a) < (b) ? (a) : (b))

enum metaorder_e
{
	METAORDER_PRE_RELEASE,
	METAORDER_ZERO,
	METAORDER_NONZERO,
	METAORDER_LETTER_SUFFIX,
} ;
typedef enum metaorder_e metaorder_t ;

typedef struct version_metaorder_s version_metaorder_t ;
struct version_metaorder_s
{
	int metaorder ;
	const char *start ;
	const char *end ;
} ;

static inline int is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ;
}

static inline int is_num(char c)
{
    return c >= '0' && c <= '9' ;
}

static inline int is_sep(char c)
{
    return !is_num(c) && !is_alpha(c) && c != '\0' ;
}

static inline const char *skip_sep(const char *s)
{
    const char *c = s ;
    while (is_sep(*c))
        ++c ;
    return c ;
}

static inline const char *skip_alpha(const char *s)
{
    const char *c = s ;
    while (is_alpha(*c))
        ++c ;
    return c ;
}

static inline const char *skip_zero(const char *s)
{
	const char *c = s;
	while (*c == '0')
		++c ;
	return c ;
}

static inline const char *skip_num(const char *s)
{
    const char *c = s ;
    while(is_num(*c))
        ++c ;
    return c ;
}

static inline char to_lower(char c)
{
    if (c >= 'A' && c <= 'Z')
		return c - 'A' + 'a' ;
	else
		return c ;
}

int cmp_component(const version_metaorder_t *a, const version_metaorder_t *b)
{

    if (a->metaorder < b->metaorder) return -1 ;
    if (a->metaorder > b->metaorder) return 1 ;

    int ae = (a->start == a->end) ;
    int be = (b->start == b->end) ;

    if (ae && be) return 0 ;
    if (ae) return -1 ;
    if (be) return 1 ;

    int aa = is_alpha(*a->start) ;
    int ba = is_alpha(*b->start) ;

    if (aa && ba) {

        if (to_lower(*a->start) < to_lower(*b->start)) return -1 ;
        if (to_lower(*a->start) > to_lower(*b->start)) return 1 ;
        return 0 ;
    }

    if (aa) return -1 ;
    if (ba) return 1 ;

    if (a->end - a->start < b->end - b->start) return -1 ;
    if (a->end - a->start > b->end - b->start) return 1 ;

    int r = memcmp(a->start, b->start, a->end - a->start) ;
    if (r < 0) return -1 ;
    if (r > 0) return 1 ;
    return 0 ;
}

static void token_to_component(const char **s, version_metaorder_t *c)
{
    if (is_alpha(**s)) {
        c->start = *s ;
        c->end = *s = skip_alpha(*s) ;
        c->metaorder = METAORDER_PRE_RELEASE ;
    } else {
        c->start = *s = skip_zero(*s) ;
        c->end = *s = skip_num(*s) ;
        if (c->start == c->end)
            c->metaorder = METAORDER_ZERO ;
        else c->metaorder = METAORDER_NONZERO ;
    }
}

size_t next_component(const char **s, version_metaorder_t *c)
{
    *s = skip_sep(*s) ;

    if (**s == '\0') {
        c->metaorder = METAORDER_ZERO ;
        c->start = c->end = "" ;
        return 1 ;
    }

    token_to_component(s, c) ;

    if (is_alpha(**s)) {
        ++c ;
        c->start = *s ;
        c->end = skip_alpha(*s) ;

        if (!is_num(*c->end)) {
            c->metaorder = METAORDER_LETTER_SUFFIX ;
            *s = c->end ;
            return 2 ;
        }
    }
    return 1 ;
}

int version_compare(const char *a, const char *b)
{
    version_metaorder_t ac[2], bc[2] ;
    size_t alen = 0, blen = 0, shift, i ;

    int ax, bx, r ;

    if (strlen(a) > SS_SERVICE_VERSION_MAXLEN || strlen(b) > SS_SERVICE_VERSION_MAXLEN)
        return (errno = EINVAL, -2) ;

    do {

        if (alen == 0)
            alen = next_component(&a, ac) ;
        if (blen == 0)
            blen = next_component(&b, bc) ;

        shift = VMIN(alen, blen) ;

        for (i = 0 ; i < shift ; i++) {
            r = cmp_component(&ac[i], &bc[i]) ;
            if (r != 0)
                return r ;
        }

        if (alen != blen) {
            for (i = 0 ; i < shift ; i++) {
                ac[i] = ac[i + shift] ;
                bc[i] = bc[i + shift] ;
            }
        }

        alen -= shift ;
        blen -= shift ;

        ax = (*a == '\0' && alen == 0) ;
        bx = (*b == '\0' && blen == 0) ;

    } while (!ax || !bx) ;

    return 0 ;
}
