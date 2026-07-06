#include "../include/packet.h"
#include "../include/transfer.h"

#include <endian.h>
#include <errno.h>
#include <stdio.h>

u64 seq_num = 0;

u64 packet_increment_seq_num() {return seq_num++;}

/* Returns OK if the packet is sane, an error code otherwise */
i8 packet_validate(packet_t *pkt, ssize_t bytes_received)
{
        if (bytes_received < 0)                 return ERR_NETWORK;
        if (bytes_received < (ssize_t)HDR_SZ)   return ERR_NETWORK;

        /* Convert received packet to mirror 'packet_hdr_init' */
        pkt->header.length = ntohs(pkt->header.length);
        pkt->header.seq_num = be64toh(pkt->header.seq_num);

        if (pkt->header.length > MAX_PLD_LEN)   return ERR_PKT_MALFORMED;
        if (bytes_received != (ssize_t)(HDR_SZ + pkt->header.length))
                return ERR_PKT_MALFORMED;

        return OK;
}

void packet_hdr_init(packet_t *pkt, type_t type, u16 length, u64 seq_num)
{
        pkt->header.type        = type;
        pkt->header.length      = htons(length);
        pkt->header.seq_num     = htobe64(seq_num);
}

i8 packet_send_and_recv_ack(i32 fd, packet_t *send_pkt, u16 length)
{
        /* Because packet_validate will convert to host */
        u64 seq = be64toh(send_pkt->header.seq_num);
        i8 acked = 0;
        for (i8 attempt = 0; attempt < MAX_RETRIES && !acked; attempt++) {
                if (send(fd, send_pkt, PKT_SZ(length), 0) == -1)
                        return ERR_NETWORK;

                errno = 0;
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
