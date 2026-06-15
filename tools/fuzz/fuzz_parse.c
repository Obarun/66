/*
 * fuzz_parse.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

/* libFuzzer harness for parse_contents(), the in-memory entry point of the
 * frontend parser: it takes the whole INI text as a buffer and splits it into
 * sections, so it exercises the lexer (parse_get_section / parse_bracket) and
 * every parse_section_* handler without touching the disk. It returns 1/0 and
 * never exits, which makes it a clean fuzz target.
 *
 * Build: CC=clang meson setup builddir-fuzz -Dfuzz=true \
 *            -Db_sanitize=address,undefined && meson compile -C builddir-fuzz
 * Run:   builddir-fuzz/tools/fuzz/fuzz_parse tools/fuzz/corpus -max_len=8192
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <oblibs/log.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/parse.h>

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void) argc ;
    (void) argv ;
    /* parse_contents logs a warning on every malformed input; silence oblibs so
     * the fuzzer is not throttled by stderr I/O. Sanitizer reports come from the
     * runtime, not through VERBOSITY, so they are unaffected. */
    VERBOSITY = 0 ;
    return 0 ;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char *str = malloc(size + 1) ;
    if (!str)
        return 0 ;
    memcpy(str, data, size) ;
    str[size] = 0 ;

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;
    resolve_init(wres) ;
    res.name = resolve_add_string(wres, "fuzz") ;

    parse_contents(&res, str) ;

    resolve_free(wres) ;
    free(str) ;
    return 0 ;
}
