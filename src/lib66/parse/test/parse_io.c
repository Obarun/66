/*
 * parse_io.c -- characterization test for parse_io_resolve()
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

/* Locks the StdIn/StdOut/StdErr type resolution of parse_io_resolve() case by
 * case, so a rewrite can be proven behaviour-preserving. Expected values were
 * traced from the current machine, INCLUDING its :183 bug (case null_null_null,
 * out stays NULL); that one case flips when the bug is fixed. */

#include <assert.h>
#include <stdio.h>

#include <oblibs/strbuf.h>
#include <oblibs/string.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/ssexec.h>
#include <66/enum_parser.h>

static resolve_service_t res = RESOLVE_SERVICE_ZERO ;
static ssexec_t info = SSEXEC_ZERO ;

#define T E_PARSER_IO_TYPE_TTY
#define C E_PARSER_IO_TYPE_CONSOLE
#define SY E_PARSER_IO_TYPE_SYSLOG
#define FI E_PARSER_IO_TYPE_FILE
#define LG E_PARSER_IO_TYPE_66LOG
#define IN E_PARSER_IO_TYPE_INHERIT
#define NU E_PARSER_IO_TYPE_NULL
#define PA E_PARSER_IO_TYPE_PARENT
#define CL E_PARSER_IO_TYPE_CLOSE
#define NS E_PARSER_IO_TYPE_NOTSET

static void io_case(char const *name, int want, int islog, int i, int o, int e,
                    int xi, int xo, int xe, int xwant)
{
    printf("Running io_case %s...\n", name) ;

    res.logger.want = want ;
    res.islog = islog ;
    res.io.fdin.type = i ;  res.io.fdin.destination = 0 ;
    res.io.fdout.type = o ; res.io.fdout.destination = 0 ;
    res.io.fderr.type = e ; res.io.fderr.destination = 0 ;

    parse_io_resolve(&res, &info) ;

    assert(res.io.fdin.type == (uint32_t)xi) ;
    assert(res.io.fdout.type == (uint32_t)xo) ;
    assert(res.io.fderr.type == (uint32_t)xe) ;
    assert(res.logger.want == (uint32_t)xwant) ;
}

int main(void)
{
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;
    resolve_init(wres) ;
    res.name = resolve_add_string(wres, "testsvc") ;
    res.owner = 0 ; // root -> compute_log_dir uses SS_LOGGER_SYSDIR, no home needed

    if (!auto_strbuf(&info.live, "/run/66"))
        return 111 ;
    info.ownerstr[0] = '0' ; info.ownerstr[1] = 0 ;
    info.ownerlen = 1 ;

    /* logger.want = 0, islog = 0 : 66LOG/NOTSET collapse to PARENT, others kept */
    io_case("nolog_collapse",  0, 0, LG, NS, FI,  PA, PA, FI, 0) ;
    io_case("nolog_keep",      0, 0, T,  C,  NS,  T,  C,  PA, 0) ;

    /* logger.want = 0, islog = 1 : this is the logger's own resolve */
    io_case("islog",           0, 1, PA, NS, PA,  LG, LG, IN, 0) ;

    /* logger.want = 1 : the resolution machine */
    io_case("all_notset",      1, 0, NS, NS, NS,  LG, LG, IN, 1) ;
    io_case("in_notset_out_file", 1, 0, NS, FI, NS,  PA, FI, IN, 0) ;
    io_case("in_tty",          1, 0, T,  NS, NS,  T,  T,  IN, 0) ;
    io_case("in_66log",        1, 0, LG, NS, NS,  LG, LG, IN, 1) ;
    io_case("out_66log",       1, 0, NS, LG, NS,  LG, LG, IN, 1) ;
    io_case("in_null",         1, 0, NU, NS, NS,  NU, IN, IN, 0) ;
    io_case("in_close",        1, 0, CL, NS, NS,  CL, PA, IN, 0) ;
    io_case("in_parent",       1, 0, PA, NS, NS,  PA, PA, IN, 0) ;
    io_case("in_inherit_fallthrough", 1, 0, IN, NS, NS,  LG, LG, IN, 1) ;
    io_case("out_syslog",      1, 0, NS, SY, NS,  PA, SY, SY, 0) ;
    io_case("all_file",        1, 0, FI, FI, FI,  FI, FI, IN, 0) ;

    /* :183 bug -- current behaviour: out stays NULL, err collapses to INHERIT.
     * When the bug is fixed this becomes NU / IN / NU. */
    io_case("null_null_null",  1, 0, NU, NU, NU,  NU, NU, IN, 0) ;

    free(wres) ;

    printf("All tests passed successfully.\n") ;
    return 0 ;
}
