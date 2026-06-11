/*
 * 66-nuke.c
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
 *
 * This file is a modified copy of s6-linux-init-nuke.c file
 * coming from skarnet software at https://skarnet.org/software/s6-linux-init.
 * All credits goes to Laurent Bercot <ska-remove-this-if-you-are-not-a-bot@skarnet.org>
 * */

#include <signal.h>
#include <unistd.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>

static opt_t const opts[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help", .arg = OPT_NONE, .help = "print this help" },
} ;

static opt_cmd_t const cmd = {
    .name = "66-nuke",
    .opts = opts,
    .nopts = OPT_COUNT(opts),
} ;

int main (int argc, char const *const *argv)
{
    {
        opt_scan_t st = OPT_SCAN_ZERO ;

        for (;;)
        {
            int o = opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) ;
            if (o == OPT_END) break ;
            switch (o) {
                case OPT_ID_HELP : return opt_emit_help(&cmd) ;
                default : return opt_emit_error(&cmd, o, &st) ;
            }
        }
    }

    if (getuid())
        log_die(LOG_EXIT_SYS,"You must be root to use this program") ;

    return kill(-1, SIGKILL) < 0 ;
}
