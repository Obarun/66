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

#include <66/service.h>
#include <66/ssexec.h>
#include <66/graph.h>

extern int svc_scandir_ok (char const *dir) ;
extern int svc_scandir_send(char const *scandir,char const *signal) ;
extern int svc_send(char const *const *list, unsigned int nservice, char **sig, unsigned int siglen, ssexec_t *info) ;
extern int svc_send_wait(char const *const *list, unsigned int nservice, char **sig, unsigned int siglen, ssexec_t *info) ;
extern void svc_unsupervise(unsigned int *alist, unsigned int alen, graph_t *g, resolve_service_t *ares, unsigned int areslen) ;

#endif
