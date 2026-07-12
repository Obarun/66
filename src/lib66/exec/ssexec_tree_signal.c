/*
 * ssexec_tree_signal.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <stdint.h>
#include <string.h>

#include <oblibs/log.h>

#include <66/ssexec.h>
#include <66/tree.h>

static inline unsigned int lookup (char const *const *table, char const *signal)
{
    unsigned int i = 0 ;
    for (; table[i] ; i++) if (!strcmp(signal, table[i])) break ;
    return i ;
}

static inline unsigned int parse_signal (char const *signal)
{
    log_flow() ;

    static char const *const signal_table[] = {
        "start",
        "stop",
        "free",
        0
    } ;
    unsigned int i = lookup(signal_table, signal) ;
    if (!signal_table[i]) log_die(LOG_EXIT_USER, "unknown tree signal: ", signal) ;
    return i ;
}

/** The sub-command name selects the signal. The thin entries
 * (do_tree_start/stop/free) post it here; the body reads it. */
static char const *tree_signal_name = 0 ;
static uint8_t tree_signal_fork = 0 ;

int on_tree_signal(int id, char const *arg, void *data)
{
    (void)arg ;
    (void)data ;

    switch (id) {
        case 'f' : tree_signal_fork = 1 ; break ;
    }

    return 0 ;
}

/** Select the signal from the sub-command name, then run the handler. The leaf
 * options (-f) have already been scanned by opt_dispatch at the sub-node. */

int do_tree_start(int argc, char const *const *argv, void *data)
{
    tree_signal_name = "start" ;
    return ssexec_tree_signal(argc, argv, data) ;
}

int do_tree_stop(int argc, char const *const *argv, void *data)
{
    tree_signal_name = "stop" ;
    return ssexec_tree_signal(argc, argv, data) ;
}

int do_tree_free(int argc, char const *const *argv, void *data)
{
    tree_signal_name = "free" ;
    return ssexec_tree_signal(argc, argv, data) ;
}

int ssexec_tree_signal(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    /* drain option state into locals, then reset the statics for re-entrancy. */
    uint8_t dofork = tree_signal_fork ;
    char const *signame = tree_signal_name ;
    tree_signal_fork = 0 ;
    tree_signal_name = 0 ;

    uint8_t operation = parse_signal(signame) ;
    char const *treename = argc >= 1 ? argv[0] : 0 ;

    return tree_send(operation, treename, dofork, info) ;
}
