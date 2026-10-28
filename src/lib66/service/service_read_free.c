/*
 * service_read_free.c
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

#include <oblibs/log.h>
#include <oblibs/strbuf.h>

#include <66/service.h>

void service_read_free(struct resolve_hash_s *h)
{
    log_flow() ;

    strbuf_free(&h->res.sa) ;
    strbuf_free(&h->limit.sa) ;
    strbuf_free(&h->environ.sa) ;
    strbuf_free(&h->io.sa) ;
    strbuf_free(&h->logger.sa) ;
    strbuf_free(&h->execute.sa) ;
    strbuf_free(&h->dependencies.sa) ;
    strbuf_free(&h->regex.sa) ;
    strbuf_free(&h->event.sa) ;
    strbuf_free(&h->copyfrom) ;
    strbuf_free(&h->before.sa) ;
    strbuf_free(&h->beforedeps.sa) ;

    *h = (struct resolve_hash_s)RESOLVE_HASH_ZERO ;
}
