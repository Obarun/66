/*
 * svc.h
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

#ifndef SS_SVC_H
#define SS_SVC_H

#include <unistd.h>

#include <66/service.h>
#include <66/ssexec.h>
#include <66/graph.h>

#include <skalibs/tai.h>

#include <66/service.h>
#include <66/graph.h>

#define DATASIZE 65

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
    resolve_service_t *res ;
    uint32_t index ; // index number of the vertex
    uint8_t state ; // current state of the vertex
    uint32_t nedge ; // number
    vertex_t *notif[SS_MAX_SERVICE] ; // array of vertex_t to notif when a edge is done
    uint32_t nnotif ; // number
} ;

#define PIDSERVICE_ZERO { \
    .pipe[0] = -1, \
    .pipe[1] = -1, \
    .res = NULL, \
    .index = 0, \
    .state = 0, \
    .nedge = 0, \
    .notif = {NULL}, \
    .nnotif = 0 \
}

extern void svc_init_array(pidservice_t *apids, service_graph_t *g, uint8_t requiredby, uint32_t flag) ;
extern int svc_launch(pidservice_t *apids, uint32_t nservice, uint8_t what, ssexec_t *info, char const *rise, uint8_t rise_opt, uint8_t msg, char const *signal, uint8_t propagate) ;
extern int svc_compute_ns(resolve_service_t *res, uint8_t what, ssexec_t *info, char const *updown, uint8_t opt_updown, uint8_t reloadmsg,char const *data, uint8_t propagate) ;
extern int svc_scandir_ok (char const *dir) ;
extern int svc_scandir_send(char const *scandir,char const *signal) ;
extern int svc_send_wait(char const *const *list, uint32_t nservice, char **sig, unsigned int siglen, ssexec_t *info) ;
extern void svc_unsupervise(service_graph_t *g) ;
extern void svc_send_fdholder(char const *socket, char const *signal) ;

#endif
