/*
 * tree.h
 *
 * Copyright (c) 2018 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#ifndef SS_TREE_H
#define SS_TREE_H

#include <sys/types.h>
#include <stdint.h>

#include <oblibs/hash.h>
#include <oblibs/sse.h>
#include <oblibs/cdb.h>
#include <oblibs/strbuf.h>

#include <66/ssexec.h>
#include <66/resolve.h>
#include <66/graph.h>
#include <66/enum_tree.h>

#define TREE_GROUPS_BOOT "boot"
#define TREE_GROUPS_BOOT_LEN (sizeof TREE_GROUPS_BOOT - 1)
#define TREE_GROUPS_ADM "admin"
#define TREE_GROUPS_ADM_LEN (sizeof TREE_GROUPS_ADM - 1)
#define TREE_GROUPS_USER "user"
#define TREE_GROUPS_USER_LEN (sizeof TREE_GROUPS_USER - 1)

typedef struct resolve_tree_s resolve_tree_t, *resolve_tree_t_ref ;
struct resolve_tree_s
{
    strbuf sa ;
    uint32_t rversion ; //string, version of 66 of the resolve file at write time

    uint32_t name ;
    uint32_t enabled ;
    uint32_t depends ;
    uint32_t requiredby ;
    uint32_t allow ;
    uint32_t groups ;
    uint32_t contents ;

    uint32_t ndepends ;
    uint32_t nrequiredby ;
    uint32_t nallow ;
    uint32_t ngroups ; //not really useful for now, we accept only one group
    uint32_t ncontents ;

    uint32_t init ;//not initialized->0, initialized->1
    uint32_t supervised ;//not superviseded->0, supervised->1
} ;

#define RESOLVE_TREE_ZERO { STRBUF_ZERO,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }

extern const resolve_tree_t tree_resolve_zero ;

typedef struct resolve_tree_master_s resolve_tree_master_t, *resolve_tree_master_t_ref ;
struct resolve_tree_master_s
{
    strbuf sa ;
    uint32_t rversion ; //string, version of 66 of the resolve file at write time

    uint32_t name ;
    uint32_t allow ;
    uint32_t current ;
    uint32_t contents ;

    uint32_t nallow ;
    uint32_t ncontents ;

} ;

#define RESOLVE_TREE_MASTER_ZERO { STRBUF_ZERO,0,0,0,0,0,0,0 }

extern const resolve_tree_master_t tree_resolve_master_zero ;

typedef struct tree_seed_s tree_seed_t, tree_seed_t_ref ;
struct tree_seed_s
{
    strbuf sa ;

    int name ;
    int depends ;
    int requiredby ;
    int allow ;
    int deny ;
    int groups ;
    int contents ;

    uint8_t current ;
    uint8_t disen ;

    uint8_t nopts ;
} ;

#define TREE_SEED_ZERO { STRBUF_ZERO, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0 }

struct resolve_hash_tree_s {
	char name[SS_MAX_SERVICE_NAME + 1] ; // name as key
	uint8_t visit ;
	resolve_tree_t tres ;
	hash_node_t node ;

} ;

#define RESOLVE_HASH_TREE_ZERO { 0, 0, RESOLVE_TREE_ZERO, HASH_NODE_ZERO }

#define TREE_FLAGS_DOWN 1
#define TREE_FLAGS_UP (1 << 1)
#define TREE_FLAGS_PROCESSING (1 << 2)
#define TREE_FLAGS_STARTING (1 << 3)
#define TREE_FLAGS_STOPPING (1 << 4)
#define TREE_FLAGS_FAILED (1 << 5)
#define TREE_FLAGS_WAITING_DEPS (1 << 6)
#define TREE_FLAGS_TIMEOUT (1 << 7)

struct tree_ctx_s
{
    pid_t pid ; // Process ID when running
    resolve_tree_t *tres ; // Service resolution data

    // Watchers
    sse_watcher_t child ;   // Child process watcher
    sse_watcher_t timeout ; // Timeout watcher

    // State management
    uint8_t state ; // Current state
    uint8_t target_state ; // Desired state

    // Dependencies
    uint32_t index ; // vertex index of the service
    vertex_t *depends[SS_MAX_SERVICE] ; // Services depends
    uint32_t ndepends ;
    vertex_t *requiredby[SS_MAX_SERVICE] ; // requiredby dependencies of the service
    uint32_t nrequiredby ;

    // Runtime data
    int exitcode ; // Last exit code
} ;
typedef struct tree_ctx_s tree_ctx_t ;

#define TREE_CTX_ZERO { \
    .pid = -1, \
    .tres = NULL, \
    .child = {0}, \
    .timeout = {0}, \
    .state = 0, \
    .target_state = 0, \
    .index = 0, \
    .depends = { NULL }, \
    .ndepends = 0, \
    .requiredby = { NULL }, \
    .nrequiredby = 0, \
    .exitcode = 0 \
}

struct tree_manager_s
{
    sse_epoll_t loop ; // Main event loop
    tree_ctx_t *atree ; // Service array
    uint32_t ntree ; // Number of services

    // Global watchers
    sse_watcher_t signalfd ; // Global signal handler
    sse_watcher_t notifier ; // Internal notifier pipe
    sse_watcher_t deadline ; // Global deadline timer if any

    // notifier mechanism
    int notifd[2] ; // Internal event notifier pipe

    // State
    bool shutdown_requested ; // Shutdown in progress
    int exitcode ; // first tree that missed its state

    // Configuration
    ssexec_t *info ;
    uint64_t timeout ; // Global operation timeout
    uint8_t operation ; // START/STOP operation
    char *cmdmsg ; // echo start/stop or unsupervise
} ;
typedef struct tree_manager_s tree_manager_t ;


/** @Return 1 on success
 * @Return 0 if not valid
 * @Return -1 on system error */
extern int tree_isvalid(char const *base, char const *treename) ;

/** Append @tree with the name of the current tree
 * @Return 1 on success
 * @Return 0 on fail */
extern int tree_find_current(char *tree, char const *base) ;

/** @Return 1 on success
 * @Return 0 if not valid
 * @Return -1 on system error */
extern int tree_iscurrent(char const *base, char const *treename) ;

/** @Return 1 on success
 * @Return 0 on fail
 * @Return -1 on system error */
extern int tree_isinitialized(char const *base, char const *treename) ;

/** @Return 1 on success
 * @Return 0 on fail
 * @Return -1 on system error */
extern int tree_issupervised(char const *base, char const *treename) ;

/** @Return 1 on success
 * @Return 0 if not valid
 * @Return -1 on system error */
extern int tree_isenabled(char const *base, char const *treename) ;

/** @Return 1 on success
 * @Return 0 if not valid
 * @Return -1 on system error */
extern int tree_ongroups(char const *base, char const *treename, char const *group) ;

extern int tree_get_permissions(char const *base, char const *treename) ;

extern int tree_sethome(ssexec_t *info) ;

extern int tree_switch_current(char const *base, char const *tree) ;

/** Resolve API */
/** tree */
extern int tree_resolve_read_cdb(ocdb *c, resolve_tree_t *tres) ;
extern int tree_resolve_write_cdb(ocdbmaker *c, resolve_tree_t *tres) ;
extern void tree_resolve_sanitize(resolve_tree_t *tres) ;
extern void tree_resolve_modify_field(resolve_tree_t *tres, uint32_t field, char const *data) ;
extern int tree_resolve_get_field_tosa(strbuf *sa, resolve_tree_t *tres, resolve_tree_enum_table_t table) ;
extern void tree_service_add(char const *treename, char const *service, ssexec_t *info) ;
extern void tree_service_remove(char const *base, char const *treename, char const *service) ;
/** Master */
extern int tree_resolve_master_read_cdb(ocdb *c, resolve_tree_master_t *mres) ;
extern int tree_resolve_master_write_cdb(ocdbmaker *c, resolve_tree_master_t *mres) ;
extern int tree_resolve_master_create(char const *base, uid_t owner) ;
extern void tree_resolve_master_sanitize(resolve_tree_master_t *mres) ;
extern void tree_resolve_master_modify_field(resolve_tree_master_t *mres, uint32_t field, char const *data) ;
extern int tree_resolve_master_get_field_tosa(strbuf *sa, resolve_tree_master_t *mres, resolve_tree_enum_table_t table) ;

/** Seed API */
extern int tree_seed_file_isvalid(char const *seedpath, char const *treename) ;
extern void tree_seed_free(tree_seed_t *seed) ;
extern int tree_seed_get_group_permissions(tree_seed_t *seed) ;
extern ssize_t tree_seed_get_key(char *table,char const *str) ;
extern int tree_seed_isvalid(char const *seed) ;
extern int tree_seed_parse_file(tree_seed_t *seed, char const *seedpath) ;
extern int tree_seed_resolve_path(strbuf *sa, char const *seed) ;
extern int tree_seed_setseed(tree_seed_t *seed, char const *treename) ;

/** HASH API*/
extern int hash_add_tree(hash_t *hash, char const *name, resolve_tree_t res) ;
extern struct resolve_hash_tree_s *hash_search_tree(hash_t *hash, char const *name) ;
extern int hash_count_tree(hash_t *hash) ;
extern void hash_free_tree(hash_t *hash) ;

/** signal */
extern void tree_init_ctx(tree_ctx_t *atree, tree_graph_t *g, uint8_t requiredby, uint32_t flag) ;
extern int tree_launch(tree_ctx_t *atree, uint32_t ntree, uint8_t operation, ssexec_t *info) ;
extern int tree_send(uint8_t operation, char const *treename, uint8_t bygroup, uint8_t dofork, ssexec_t *info) ;

#endif
