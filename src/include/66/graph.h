/*
 * graph.h
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
 * */

#ifndef SS_GRAPH_H
#define SS_GRAPH_H

#include <stdint.h>
#include <stdbool.h>

#include <oblibs/queue.h>
#include <oblibs/graph.h>

#include <66/ssexec.h>

#define GRAPH_WANT_EARLIER (1 << 1)
#define GRAPH_WANT_SUPERVISED (1 << 2)
#define GRAPH_WANT_DEPENDS (1 << 3)
#define GRAPH_WANT_REQUIREDBY (1 << 4)
#define GRAPH_WANT_LOGGER (1 << 5)

#define GRAPH_SKIP_MODULECONTENTS (1 << 6)
#define GRAPH_SKIP_EARLIER (1 << 7)

#define GRAPH_COLLECT_PARSE (1 << 8)
// tree flag
#define GRAPH_WANT_ENABLED (1 << 9)

/**
 * @struct service_graph_s
 * @brief Represents a service dependency graph.
 *
 * This structure encapsulates the necessary components to manage a service graph,
 * including the graph itself, a hash table of resolved services, and sorting data.
 *
 * @param g
 * The internal graph representation storing vertices and edges.
 *
 * @param hres
 * A hash table containing resolved services and their associated metadata.
 */
typedef struct service_graph_s service_graph_t, *service_graph_t_ref ;
struct service_graph_s
{
    graph g ;
    struct resolve_hash_s *hres ;
} ;

#define GRAPH_SERVICE_ZERO { .g = GRAPH_ZERO, .hres = NULL }

typedef struct tree_graph_s tree_graph_t, *tree_graph_t_ref ;
struct tree_graph_s
{
    graph g ;
    struct resolve_hash_tree_s *hres ;
} ;

#define GRAPH_TREE_ZERO { GRAPH_ZERO, NULL }

#define FOREACH_GRAPH_SORT(type,gn,pos) \
    type *__g__##__LINE__ = gn; \
    for (; (uint32_t)pos < __g__##__LINE__->g.nsort; pos++)

#define graph_new(gr, len) graph_init(&(gr)->g, len)

/**
 * @brief Adds a vertex to the graph.
 *
 * This function attempts to add a vertex with the specified name to the graph.
 * If the vertex already exists, no action is taken.
 *
 * @param g
 * Pointer to the graph where the vertex will be added.
 *
 * @param name
 * Name of the vertex to be added.
 *
 * @return 1
 * If the vertex is successfully added.
 *
 * @return 0
 * If an error occurs during the addition process.
 *
 * @note
 * - If the vertex cannot be added, the function sets `errno` to `EINVAL`.
 *
 * @errno
 * - `EINVAL` if the vertex addition fails.
 */
extern int graph_add(graph *g, const char *name) ;

/**
 * @brief Adds dependencies to a vertex in the graph.
 *
 * This function processes the provided edge string, cleans it, and adds the
 * dependencies to the specified vertex in the graph.
 *
 * @param g
 * Pointer to the graph where the dependencies will be added.
 *
 * @param vertex
 * Name of the vertex to which dependencies should be added.
 *
 * @param edge
 * String containing the dependencies to be processed.
 *
 * @param requiredby
 * Boolean flag indicating the direction of the dependencies:
 *   - `false`: Adds edges from `vertex` to dependencies.
 *   - `true`: Adds edges from dependencies to `vertex`.
 *
 * @return 1
 * If the dependencies are successfully added.
 *
 * @return 0
 * If an error occurs.
 *
 * @note
 * - If the edge string cannot be cleaned or dependencies cannot be added, a warning is logged.
 */
extern int graph_add_deps(graph *g, const char *vertex, char const *edge, bool requiredby) ;

/**
 * @brief Retrieves and stores the edges of a given vertex in a stack.
 *
 * This function retrieves all edges connected to a given vertex and stores
 * them in the provided stack. It can be used to collect either dependency edges
 * or required-by edges based on the `requiredby` parameter.
 *
 * @param stk
 * Pointer to the stack where the edges will be stored.
 *
 * @param g
 * Pointer to the graph structure.
 *
 * @param v
 * Pointer to the vertex whose edges should be retrieved.
 *
 * @param requiredby
 * Boolean flag indicating whether to retrieve required-by edges (`true`) or dependency edges (`false`).
 *
 * @return 1
 * If edges are successfully retrieved and stored.
 *
 * @return 0
 * If memory allocation fails (`errno` is set to `ENOMEM`).
 *
 * @note
 * - If memory allocation for storing edges fails, the function returns immediately with an error.
 */
extern int graph_get_stkedge(stack *stk, graph *g, vertex_t *v, bool requiredby) ;

/**
 * @brief Initializes a service graph structure.
 *
 * This function initializes the service graph by allocating necessary resources
 * and preparing it for further operations.
 *
 * @param g
 * Pointer to the service graph structure to initialize.
 *
 * @param len
 * The initial length of the graph.
 *
 * @return 1
 * If the vertex is successfully added.
 *
 * @return 0
 * If an error occurs during the addition process.
 *
 * @note
 * - If memory allocation for the sorted list fails, the graph is cleaned up to prevent memory leaks.
 * - A fatal log message is generated if initialization fails.
 */
extern int service_graph_new(service_graph_t *g, uint32_t len) ;
extern int tree_graph_new(tree_graph_t *g, uint32_t len) ;

/**
 * @brief Frees resources allocated for a service graph.
 *
 * This function releases memory allocated for the service graph structure,
 * including the internal graph, hash table, and sorted list.
 *
 * @param g
 * Pointer to the service graph structure to be freed.
 *
 * @note
 * After calling this function, the service graph structure should not be accessed.
 */
extern void service_graph_destroy(service_graph_t *g) ;
extern void tree_graph_destroy(tree_graph_t *g) ;

/**
 * @brief Collects and processes a single service.
 *
 * This function processes an individual service based on the provided name and
 * collection flags, ensuring that dependencies are properly handled.
 *
 * @param g
 * Pointer to the service graph structure.
 *
 * @param name
 * Name of the service to collect.
 *
 * @param info
 * Pointer to the execution context structure containing service-related information
 *
 * @param flag
 * Flags indicating the collection options.
 *
 * @return The total number of services processed, including dependencies.
 *
 * @note
 * If the service does not exist and the `CONDITION_COLLECT_TOPARSE` flag is set,
 * it will attempt to resolve and sanitize it.
 */
extern uint32_t service_graph_collect(service_graph_t *g, const char *name, ssexec_t *info, uint32_t flag) ;
extern uint32_t tree_graph_collect(tree_graph_t *g, const char *treename, ssexec_t *info) ;

/**
 * @brief Collects and processes a list of services from the provided list.
 *
 * This function iterates through a list of service names and processes each
 * entry according to the specified flag options.
 *
 * @param g
 * Pointer to the service graph structure.
 *
 * @param list
 * Pointer to the string list of service names to collect.
 *
 * @param len
 * Length of the service list.
 *
 * @param info
 * Pointer to the execution context structure containing service-related information
 *
 * @param flag
 * Flags indicating the collection options.
 *
 * @return The total number of services processed.
 *
 * @note
 * This function call graph_collect for each service define at @param slist.
 */
extern uint32_t service_graph_ncollect(service_graph_t *g, const char *list, size_t len, ssexec_t *info, uint32_t flag) ;
extern uint32_t tree_graph_ncollect(tree_graph_t *g, const char *list, size_t len, ssexec_t *info) ;

/**
 * @brief Builds a service graph with specified conditions.
 *
 * This function iterates through services and builds the service graph based on the
 * provided conditions and dependencies.
 *
 * @param g
 * Pointer to the service graph.
 *
 * @param flag
 * Bitmask flag specifying the conditions for graph construction.
 *
 * @return 1
 * If the graph is successfully built.
 *
 * @return 0
 * If an error occurs.
 */
extern int service_graph_resolve(service_graph_t *g, const char *name, uint32_t flag) ;
extern int tree_graph_resolve(tree_graph_t *g, const char *treename, uint32_t flag) ;

/**
 * @brief Builds a service graph for a specific service name.
 *
 * This function constructs a dependency graph for a given service name, processing
 * conditions and dependencies as necessary.
 *
 * @param g
 * Pointer to the service graph.
 *
 * @param name
 * Name of the service to build the graph for.
 *
 * @param flag
 * Bitmask flag specifying the conditions for graph construction.
 *
 * @return 1
 * If the graph is successfully built.
 *
 * @return 0
 * If an error occurs.
 *
 * @note
 * this function call service_graph_resolve for each service define at @param list
 */
extern int service_graph_nresolve(service_graph_t *g, const char *list, size_t len, uint32_t flag) ;
extern int tree_graph_nresolve(tree_graph_t *g, const char *list, size_t len, uint32_t flag) ;

/**
 * @brief Collect a list of services and builds a service graph.
 *
 * This function collects services from the provided list and builds the
 * service graph according to the given flags.
 *
 * @param g
 * Pointer to the service graph structure.
 *
 * @param list
 * Pointer to the list of services to be processed.
 *
 * @param len
 * Length of the service list.
 *
 * @param info
 * Pointer to the execution context structure containing service-related information.
 *
 * @param flag
 * Flags indicating the processing options.
 *
 * @return The number of services successfully processed.
 *
 * @return 0
 * If for an empty graph and sets `errno` to `EINVAL` in case of syscall failure.
 *
 * @note
 * This is a convenient function around service_graph_ncollect and service_graph_nresolve function.
 */
extern uint32_t service_graph_build_list(service_graph_t *g, const char *list, size_t len, ssexec_t *info, uint32_t flag) ;
extern uint32_t tree_graph_build_list(tree_graph_t *g, const char *list, size_t len, ssexec_t *info, uint32_t flag) ;

/**
 * @brief Builds a service graph from command-line arguments.
 *
 * This function processes command-line arguments, extracts relevant service names,
 * and builds a service dependency graph accordingly.
 *
 * @param g
 * Pointer to the service graph structure.
 *
 * @param argv
 * Array of command-line argument strings.
 *
 * @param argc
 * Number of arguments in the array.
 *
 * @param info
 * Pointer to the execution context structure containing service-related information.
 *
 * @param flag
 * Flags indicating the conditions for graph construction.
 *
 * @return The number of services successfully added to the graph.
 *
 * @note
 * - This function extracts service names from the provided arguments and
 *   processes them into a graph structure.
 * - If argument processing fails, the function terminates execution with an error.
 */
extern uint32_t service_graph_build_arguments(service_graph_t *g, char const *const *argv, int argc, ssexec_t *info, uint32_t flag) ;

/**
 * @brief Builds a service graph using system-wide service definitions.
 *
 * This function scans system directories for service definitions, extracts relevant
 * service information, and constructs a service dependency graph.
 *
 * @param g
 * Pointer to the service graph structure.
 *
 * @param info
 * Pointer to the execution context structure containing service-related information.
 *
 * @param flag
 * Flags indicating the conditions for graph construction.
 *
 * @return The number of services successfully added to the graph.
 *
 * @note
 * - This function recursively retrieves service resolve files from the system directories.
 */
extern uint32_t service_graph_build_system(service_graph_t *g, ssexec_t *info, uint32_t flag) ;
extern uint32_t tree_graph_build_system(tree_graph_t *g, ssexec_t *info, uint32_t flag) ;

/**
 * @brief Builds a service graph for a specific service name.
 *
 * This function collects service dependencies and resolves the graph for the given service name.
 *
 * @param g
 * Pointer to the service graph structure.
 *
 * @param name
 * Name of the service for which the graph is built.
 *
 * @param info
 * Pointer to the execution context structure containing service-related information.
 *
 * @param flag
 * Flags indicating the conditions for graph construction.
 *
 * @return The number of services successfully added to the graph.
 *
 * @return 0
 * If an error occurs (`errno` is set to `EINVAL`).
 *
 * @note
 * - The function first collects the service dependencies.
 * - If no services are collected, it returns immediately.
 * - If graph resolution fails, an error is logged and the function returns 0.
 */
extern uint32_t service_graph_build_name(service_graph_t *g, const char *name, ssexec_t *info, uint32_t flag) ;
extern uint32_t tree_graph_build_name(tree_graph_t *g, const char *name, ssexec_t *info, uint32_t flag) ;

/**
 * @brief Builds a tree graph from the master service definitions.
 *
 * This function retrieves tree master service definitions, collects their service contents,
 * and resolves the graph for proper execution.
 *
 * @param g
 * Pointer to the tree graph structure.
 *
 * @param info
 * Pointer to the execution context structure containing service-related information.
 *
 * @param flag
 * Flags indicating the conditions for graph construction.
 *
 * @return The number of services successfully added to the graph.
 *
 * @return 0
 * If an error occurs (`errno` is set to `EINVAL`).
 *
 * @note
 * - The function retrieves the master tree service definitions and collects their service contents.
 * - If no services are collected, it returns immediately.
 * - If graph resolution fails, an error is logged and the function returns 0.
 */

extern uint32_t tree_graph_build_master(tree_graph_t *g, ssexec_t *info, uint32_t flag) ;

#endif
