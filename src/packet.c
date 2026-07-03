#include "../include/packet.h"
#include "../include/transfer.h"

#include <errno.h>
#include <stdio.h>

u64 seq_num = 0;

u64 packet_increment_seq_num() {return seq_num++;}

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

void packet_hdr_init(packet_t *pkt, type_t type, u16 length, u64 seq_num)
{
        pkt->header.type        = type;
        pkt->header.length      = length;
        pkt->header.seq_num     = seq_num;
}

i8 packet_send_and_recv_ack(i32 fd, packet_t *pkt, u16 length)
{
        u64 seq = pkt->header.seq_num;
        i8 acked = 0;
        for (i8 attempt = 0; attempt < MAX_RETRIES && !acked; attempt++) {
                if (send(fd, pkt, PKT_SZ(length), 0) == -1) return ERR_NETWORK;

                packet_t recv_pkt = {0};
                ssize_t recvd = recv(fd, &recv_pkt, MAX_PKT_LEN, 0);
                if (recvd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                printf(" RET: %lu", seq);
                                continue;
                        }
                        return ERR_NETWORK;
                }
                if (packet_validate(&recv_pkt, recvd) != OK)  continue;
                if (recv_pkt.header.type              != ACK) continue;
                if (recv_pkt.header.seq_num           != seq) continue;

                acked = 1;
        }

        return acked ? OK : ERR_CONN_TIMEOUT;
}
