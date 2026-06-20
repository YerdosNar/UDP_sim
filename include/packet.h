#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>

typedef uint64_t        u64;
typedef uint32_t        u32;
typedef uint16_t        u16;
typedef uint8_t          u8;

typedef int64_t         i64;
typedef int32_t         i32;
typedef int16_t         i16;
typedef int8_t           i8;

typedef enum {
        /* Handshake packets */
        HELLO           = 0x1,
        HELLO_ACK       = 0x2,
        BYE             = 0x3,
        BYE_ACK         = 0x4,

        ACK             = 0x5,
} type_t;

typedef struct __attribute__((packed)){
        u64     seq_id;
        u16     length;
        type_t  type;
} packet_t;

#endif
