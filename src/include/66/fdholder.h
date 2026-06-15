/**
 * fdholder.h
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
 **/


#ifndef SS_FDHOLDER_H
#define SS_FDHOLDER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <oblibs/sse.h>
#include <oblibs/sse_stream.h>
#include <oblibs/stream_message.h>
#include <oblibs/io_rb.h>

#define FDHOLDER_VERSION            1
#define FDHOLDER_HDR_SIZE           8
#define FDHOLDER_NAME_MAX           255
#define FDHOLDER_MAXFDS_DEFAULT     1000
#define FDHOLDER_MAXCLIENTS_DEFAULT 64
#define FDHOLDER_BACKLOG_DEFAULT    64

// largest payload that fits, with the header, in one io_rb message buffer
#define FDHOLDER_PAYLOAD_MAX        (IO_RB_BUFFER_SIZE - FDHOLDER_HDR_SIZE)

// wire commands ; responses always carry FDHOLDER_CMD_RESPONSE
enum fdholder_cmd_e
{
    FDHOLDER_CMD_RESPONSE = 0,
    FDHOLDER_CMD_STORE,
    FDHOLDER_CMD_RETRIEVE,
    FDHOLDER_CMD_DELETE,
    FDHOLDER_CMD_LIST,
    FDHOLDER_CMD_PIPE,          // get-or-create one end of a named pipe pair
    FDHOLDER_CMD_PIPE_DELETE    // destroy a named pipe pair (closes both ends)
} ;

// pipe ends (FDHOLDER_CMD_PIPE payload)
#define FDHOLDER_END_READ   0u
#define FDHOLDER_END_WRITE  1u

// response status codes
enum fdholder_status_e
{
    FDHOLDER_OK = 0,
    FDHOLDER_ERR,
    FDHOLDER_NOTFOUND,
    FDHOLDER_EXISTS,
    FDHOLDER_FULL,
    FDHOLDER_PROTO
} ;

// header flags
#define FDHOLDER_FLAG_DELETE  0x01u   // retrieve: delete the entry after handing the fd out

/**
 * Wire header layout (FDHOLDER_HDR_SIZE bytes, explicit big-endian):
 *   [0] version
 *   [1] command
 *   [2] status   (responses only ; 0 in requests)
 *   [3] flags
 *   [4..7] payload_len (uint32, big-endian)
 * File descriptors travel out-of-band through SCM_RIGHTS, never in the payload.
 *
 * Every frame is the 8-byte header above followed by payload_len payload bytes.
 * All multi-byte integers are big-endian. The payload layout per command:
 *
 *   request                                                      ancillary fds
 *   -------------------------------------------------------------------------
 *   STORE     +--------------------+----------------------------+   1 fd
 *             | expire u64 (8)     | name (payload_len - 8)     |   (the fd to
 *             +--------------------+----------------------------+    store)
 *             expire = seconds from now, 0 = never ; name is NOT NUL-terminated
 *
 *   RETRIEVE  +----------------------------+                        0
 *             | name (payload_len)         |   delete bit lives in
 *             +----------------------------+   header flags (FDHOLDER_FLAG_DELETE)
 *
 *   DELETE    +----------------------------+                        0
 *             | name (payload_len)         |
 *             +----------------------------+
 *
 *   LIST      (empty payload)                                       0
 *
 *   PIPE      +--------+-----------------------+                      0
 *             | end u8 | name (payload_len-1)  |   end: 0=read 1=write
 *             +--------+-----------------------+   get-or-create the pair
 *
 *   PIPE_DELETE +----------------------------+                        0
 *             | name (payload_len)         |
 *             +----------------------------+
 *
 *   response  header.command = FDHOLDER_CMD_RESPONSE, header.status = result.
 *   -------------------------------------------------------------------------
 *   RETRIEVE ok : empty payload + 1 ancillary fd (the retrieved descriptor)
 *   PIPE     ok : empty payload + 1 ancillary fd (the requested pipe end)
 *   LIST     ok : payload = names, each NUL-terminated, concatenated ; 0 fds
 *   others      : empty payload, 0 fds (status carries the outcome)
 */

/**
 * Multi-byte integers use oblibs' u32_pack_big/u64_pack_big and their _unpack_big
 * counterparts (oblibs/types.h). The only one-byte field (namelen, <= 255) is
 * written/read as a raw byte.
 * */


// wire helpers (fdholder_wire.c)

extern void fdholder_hdr_pack(char *hdr, uint8_t command, uint8_t status, uint8_t flags, uint32_t payload_len) ;
extern int fdholder_parse_header(void const *header, size_t header_len, size_t *payload_len, void *data) ;
extern char const *fdholder_status_str(uint8_t status) ;

// client library (fdholder_client.c)

typedef struct fdholder_client_s fdholder_client_t ;
struct fdholder_client_s
{
    sse_epoll_t epoll ;
    sse_stream_t *stream ;
    stream_message_t reader ;
    sse_watcher_t wsignal ;
    sse_watcher_t wtimer ;
    char *hdrbuf ;
    char *paybuf ;

    bool timer_active ;
    bool request_sent ;
    bool response_received ;
    uint8_t status ;          // fdholder_status_e of the last response
    int received_fd ;         // fd handed out by a retrieve, -1 otherwise
    size_t resp_payload_len ; // length of a response payload sitting in paybuf (list)
} ;

extern int fdholder_client_init(fdholder_client_t *c, char const *socket) ;
extern void fdholder_client_end(fdholder_client_t *c) ;
extern int fdholder_client_request(fdholder_client_t *c, uint8_t cmd, uint8_t flags, void const *payload, size_t paylen, int const *fds, int nfd, int timeout) ;

extern int fdholder_store(fdholder_client_t *c, char const *name, int fd, uint64_t expire, int timeout) ;
extern int fdholder_retrieve(fdholder_client_t *c, char const *name, bool dodelete, int timeout) ;
extern int fdholder_delete(fdholder_client_t *c, char const *name, int timeout) ;
extern int fdholder_list(fdholder_client_t *c, int timeout) ;

/* Get-or-create one end of the named pipe pair `name`. `end` is FDHOLDER_END_READ
 * or FDHOLDER_END_WRITE. The pair is created atomically on first request and held
 * by the daemon (survives either side restarting). The requested end lands in
 * c->received_fd. Returns 1 on success, 0 on failure (status in c->status). */
extern int fdholder_pipe(fdholder_client_t *c, char const *name, uint8_t end, int timeout) ;
extern int fdholder_pipe_delete(fdholder_client_t *c, char const *name, int timeout) ;

#endif
