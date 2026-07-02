#ifndef PACKET_H
#define PACKET_H

#include "types.h"
#include <stdlib.h>

typedef enum {
        /* Handshake packets */
        HELLO           = 0x1,
        HELLO_ACK       = 0x2,
        BYE             = 0x3,
        BYE_ACK         = 0x4,

        ACK             = 0x5,

        /* File transfer        */
        FILE_META       = 0x10,
        FILE_DATA       = 0x11,
        FILE_EOF        = 0x12,

        /* Chunk                */
        DATA            = 0x20,
} type_t;

/* sizeof(header_t) = 8+2+1 = 11 bytes */
typedef struct __attribute__((packed)){
        u64             seq_num;
        u16             length;
        u8              type;
} header_t;

#define HDR_SZ          sizeof(header_t)

/* Hardcoded max IPv6 MTU */
#define MAX_PKT_LEN     1280
#define MAX_PLD_LEN     (MAX_PKT_LEN - HDR_SZ)

typedef struct {
        header_t        header;
        /* MAX_PKT_LEN - 8(seq_id) - 2(length) - 1(type) = 1269 */
        u8              data[MAX_PLD_LEN];
} packet_t;

i8 packet_validate(const packet_t *pkt, ssize_t received);
u64 packet_increment_seq_num();
void packet_hdr_init(packet_t *pkt, type_t type, u16 length, u64 seq_num);

#endif
