/*
 * oneshot_wire.c
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
 */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <oblibs/types.h>

#include <66/oneshot.h>

void oneshot_hdr_pack(char *hdr, uint8_t command, uint8_t status, uint8_t flags, uint32_t payload_len)
{
    hdr[0] = (char)ONESHOT_VERSION ;
    hdr[1] = (char)command ;
    hdr[2] = (char)status ;
    hdr[3] = (char)flags ;
    u32_pack_big(hdr + 4, payload_len) ;
}

/*
 * Framing only: extract the payload length once the 8 header bytes are in.
 * Per-command validation belongs to the message handlers, not here.
 */
int oneshot_parse_header(void const *header, size_t header_len, size_t *payload_len, void *data)
{
    (void)data ;

    if (header_len < ONESHOT_HDR_SIZE)
        return 0 ;

    char const *h = header ;

    if ((uint8_t)h[0] != ONESHOT_VERSION)
        return (errno = EPROTO, -1) ;

    uint32_t len ;
    u32_unpack_big(h + 4, &len) ;

    if (len > ONESHOT_PAYLOAD_MAX)
        return (errno = EMSGSIZE, -1) ;

    *payload_len = len ;

    return ONESHOT_HDR_SIZE ;
}

char const *oneshot_status_str(uint8_t status)
{
    switch (status) {
        case ONESHOT_OK :     return "success" ;
        case ONESHOT_PROTO :  return "protocol error" ;
        default :             return "error" ;
    }
}
