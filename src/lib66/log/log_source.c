/*
 * log_source.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

#include <oblibs/string.h>
#include <oblibs/strbuf.h>
#include <oblibs/sbl.h>

#include <66/log.h>

static int source_parse(log_source_t *src)
{
    char const *s = src->data.s ;
    size_t len = src->data.len, n = 0 ;

    if (!len)
        return 1 ;

    // memchr is binary-safe
    for (char const *p = s, *nl ; (nl = memchr(p, '\n', (size_t)(s + len - p))) ; p = nl + 1)
        n++ ;
    if (s[len - 1] != '\n')
        n++ ;

    src->line = malloc(n * sizeof(log_line_t)) ;
    if (!src->line)
        return (errno = ENOMEM, 0) ;

    /* inherit carries the most recent parsed timestamp: a line with no stamp of
     * its own inherits it (epoch 0 before any stamp) so it stays chronologically
     * next to its context instead of falling back to epoch 0 in the merge. */
    struct timespec inherit = { 0, 0 } ;
    size_t start = 0, li = 0 ;

    while (start < len) {

        char const *nl = memchr(s + start, '\n', len - start) ;
        size_t llen = nl ? (size_t)(nl - s - start) : len - start ;

        log_line_t *pline = &src->line[li++] ;
        struct timespec ts ;
        size_t msgoff ;
        int type = log_line_key(s + start, llen, &ts, &msgoff) ;

        pline->off = start ;
        pline->len = llen ;
        pline->msgoff = msgoff ;
        pline->type = (uint8_t)type ;

        if (type == LOG_STAMP_NONE) {
            pline->stamp = inherit ;
        } else {
            pline->stamp = ts ;
            inherit = ts ;
            src->stamped = 1 ;
        }

        start += nl ? llen + 1 : llen ;
    }

    src->nline = li ;
    return 1 ;
}

static int ensure_nl(strbuf *sb)
{
    if (sb->len && sb->s[sb->len - 1] != '\n')
        return strbuf_catb(sb, "\n", 1) ;
    return 1 ;
}

static int read_member(log_source_t *src, char const *logdir, char const *name)
{
    char path[strlen(logdir) + 1 + strlen(name) + 1] ;
    auto_strings(path, logdir, "/", name) ;

    if (!ensure_nl(&src->data))
        return 0 ;

    if (!strbuf_read_file(&src->data, path) && errno != ENOENT)
        return 0 ;

    return 1 ;
}

int log_source_logdir(log_source_t *src, char const *name, char const *logdir)
{
    if (strlen(name) > SS_MAX_SERVICE_NAME)
        return (errno = EINVAL, 0) ;

    auto_strings(src->name, name) ;

    _cleanup_strbuf_ strbuf names = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf arch = STRBUF_ZERO ;
    char const *exclude[1] = { 0 } ;
    uint8_t has_current = 0 ;
    size_t pos = 0 ;

    if (!sbl_dir_get(&names, logdir, exclude, S_IFREG)) {
        if (errno == ENOENT)
            return 1 ;
        return 0 ;
    }

    FOREACH_SBL(&names, pos) {
        char const *e = names.s + pos ;
        if (e[0] == '@') {
            if (!sbl_add(&arch, e))
                return 0 ;
        } else if (!strcmp(e, "current")) {
            has_current = 1 ;
        }
    }

    if (arch.len && !sbl_sort(&arch))
        return 0 ;

    {
        size_t apos = 0 ;
        FOREACH_SBL(&arch, apos)
            if (!read_member(src, logdir, arch.s + apos))
                return 0 ;
    }

    if (has_current && !read_member(src, logdir, "current"))
        return 0 ;

    return source_parse(src) ;
}

int log_source_file(log_source_t *src, char const *name, char const *file)
{
    if (strlen(name) > SS_MAX_SERVICE_NAME)
        return (errno = EINVAL, 0) ;

    auto_strings(src->name, name) ;

    if (!strbuf_read_file(&src->data, file) && errno != ENOENT)
        return 0 ;

    return source_parse(src) ;
}

void log_source_free(log_source_t *src)
{
    strbuf_free(&src->data) ;
    free(src->line) ;
    src->line = 0 ;
    src->nline = 0 ;
    src->cur = 0 ;
}
