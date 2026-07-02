#include "../include/packet.h"

u64 seq_num = 0;

u64 increment_pkt_seq_num() {return seq_num++;}

/* Returns OK if the packet is sane, an error code otherwise */
i8 packet_validate(const packet_t *pkt, ssize_t bytes_received)
{
        if (bytes_received < 0)                 return ERR_NETWORK;
        if (bytes_received < (ssize_t)HDR_SZ)   return ERR_NETWORK;
        if (pkt->header.length > MAX_PLD_LEN)   return ERR_PKT_MALFORMED;
        if (bytes_received != (ssize_t)(HDR_SZ + pkt->header.length))
                return ERR_PKT_MALFORMED;

        return OK;
}

void packet_hdr_init(packet_t *pkt, type_t type, u16 length)
{
        pkt->header.type        = type;
        pkt->header.length      = length;
        pkt->header.seq_num     = increment_pkt_seq_num();
}
