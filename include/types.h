#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

typedef uint64_t        u64;
typedef uint32_t        u32;
typedef uint16_t        u16;
typedef uint8_t          u8;

typedef int64_t         i64;
typedef int32_t         i32;
typedef int16_t         i16;
typedef int8_t           i8;

/*
 * ERROR and WARNING codes:
 *      Positive numbers are for warnings
 *      Negative numbers are for errors
 */
typedef enum {
        /* SUCCESS                              */
        OK                      = 0,

        /* Network and Socket ERRORs            */
        ERR_SOCKET              = -1,
        ERR_BIND                = -2,
        ERR_NETWORK             = -3,
        ERR_PORT_INVALID        = -4,

        /* Connection and Protocol ERRORs       */
        ERR_CONN_TIMEOUT        = -10,
        ERR_CONN_REFUSED        = -11,
        ERR_CONN_CLOSED         = -12,
        ERR_INVALID_STATE       = -13,

        /* Packet and Memory ERRORs             */
        ERR_PKT_MALFORMED       = -20,
        ERR_PKT_TOO_LARGE       = -21,
        ERR_PKT_TYPE_MISMATCH   = -22,
        ERR_BUFFER_OVERFLOW     = -23,
        ERR_MEM_ALLOC           = -24,
        ERR_NULL_PTR            = -25,
        ERR_SEQ_MISMATCH        = -26,

        /* File related ERRORs                  */
        ERR_FILE_OPEN           = -30,
        ERR_FILE_READ           = -31,
        ERR_FILE_WRITE          = -32,
} status_t;

#endif
