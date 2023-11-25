/*
 * svc.h
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#ifndef SS_SVC_H
#define SS_SVC_H

#include <unistd.h>

#include <66/service.h>
#include <66/ssexec.h>
#include <66/graph.h>

#include <skalibs/tai.h>

#include <66/service.h>

#define DATASIZE 63

#define SVC_FLAGS_STARTING 1 // 1 starting not really up
#define SVC_FLAGS_STOPPING (1 << 1) // 2 stopping not really down
#define SVC_FLAGS_UP (1 << 2) // 4 really up
#define SVC_FLAGS_DOWN (1 << 3) // 8 really down
#define SVC_FLAGS_BLOCK (1 << 4) // 16 all deps are not up/down
#define SVC_FLAGS_UNBLOCK (1 << 5) // 32 all deps are up/down
#define SVC_FLAGS_FATAL (1 << 6) // 64 process crashed

typedef struct pidservice_s pidservice_t, *pidservice_t_ref ;
struct pidservice_s
{
    int pipe[2] ;
    pid_t pid ;
    int aresid ; // id at array ares
    unsigned int vertex ; // id at graph_hash_t struct
    uint8_t state ;
    int nedge ;
    unsigned int edge[SS_MAX_SERVICE + 1] ; // array of id at graph_hash_t struct
    int nnotif ;
    /** id at graph_hash_t struct of depends/requiredby service
     * to notify when a service is started/stopped */
    unsigned int notif[SS_MAX_SERVICE + 1] ;
} ;

#define PIDSERVICE_ZERO { \
    .pipe[0] = -1, \
    .pipe[1] = -1, \
    .aresid = -1, \
    .vertex = -1, \
    .state = 0, \
    .nedge =  0, \
    .edge = { 0 }, \
    .nnotif = 0, \
    .notif = { 0 } \
}

extern void svc_init_array(unsigned int *list, unsigned int listlen, pidservice_t *apids, graph_t *g, resolve_service_t *ares, unsigned int areslen, ssexec_t *info, uint8_t requiredby, uint32_t flag) ;
extern int svc_launch(pidservice_t *apids, unsigned int napid, uint8_t what, graph_t *graph, resolve_service_t *ares, unsigned int areslen, ssexec_t *info, char const *rise, uint8_t rise_opt, uint8_t msg, char const *signal, uint8_t propagate) ;
extern int svc_compute_ns(resolve_service_t *ares, unsigned int areslen, unsigned int aresid, uint8_t what, ssexec_t *info, char const *updown, uint8_t opt_updown, uint8_t reloadmsg,char const *data, uint8_t propagate, pidservice_t *apids, unsigned int napids) ;
extern int svc_scandir_ok (char const *dir) ;
extern int svc_scandir_send(char const *scandir,char const *signal) ;
extern int svc_send_wait(char const *const *list, unsigned int nservice, char **sig, unsigned int siglen, ssexec_t *info) ;
extern void svc_unsupervise(unsigned int *alist, unsigned int alen, graph_t *g, resolve_service_t *ares, unsigned int areslen, ssexec_t *info) ;
extern void svc_send_fdholder(char const *socket, char const *signal) ;

#endif
