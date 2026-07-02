#include "../include/packet.h"

#include <stdlib.h>

u64 seq_num = 0;

u64 increment_pkt_seq_num() {return seq_num++;}

i8 packet_hdr_create(type_t type, u16 length, header_t *hdr_ptr)
{
        if (!length || length > MAX_PLD_LEN) return ERR_PKT_TOO_LARGE;
        header_t *hdr = malloc(HDR_SZ);
        if (!hdr) return ERR_MEM_ALLOC;
        hdr->type       = type;
        hdr->length     = length;
        hdr->seq_num    = increment_pkt_seq_num();

        hdr_ptr = hdr;
        return OK;
}
