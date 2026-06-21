#include "../include/logging.h"
#include "../include/packet.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT            8888
#define ADDR            "127.0.0.1"

int main(void) {
        i32     sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0) {
                fprintf(stderr, "ERROR: socket() failed\n");
                return ERR;
        }

        struct sockaddr_in servaddr = {0};
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(PORT);
        servaddr.sin_addr.s_addr = inet_addr(ADDR);
        socklen_t slen = sizeof(servaddr);

        const char *msg = "Hello from the Client";
        i8 msg_len = strlen(msg);
        packet_t pkt = {0};
        pkt.header.seq_num = 0;
        pkt.header.type = DATA;
        pkt.header.length = msg_len;
        strncpy((char*)pkt.data, msg, msg_len);
        sendto(sockfd, (const void*)&pkt,
               msg_len + sizeof(header_t),
               MSG_CONFIRM,
               (const struct sockaddr *)&servaddr, slen);
        printf("Sent: \"%s\" to %s:%d\n", msg, ADDR, PORT);

        packet_t rcv_pkt = {0};
        recvfrom(sockfd, (void*)&rcv_pkt, MAX_PKT_LEN, 0,
                 (struct sockaddr*)&servaddr, &slen);

        rcv_pkt.data[rcv_pkt.header.length] = '\0';
        printf("Received: %s from %s:%d\n", rcv_pkt.data, ADDR, PORT);

        packet_t metadata = {0};
        recvfrom(sockfd, (void*)&metadata,
                 MAX_PKT_LEN, 0,
                 (struct sockaddr*)&servaddr, &slen);
        char *fileinfo = (char*)metadata.data;
        printf("Received metadata: %s\n", fileinfo);
        fileinfo[metadata.header.length] = '\0';
        char *filename = strtok(fileinfo, "|");
        printf("Receiving %s\n\n", filename);

        FILE *file = fopen(filename, "wb");
        if (!file) {
                fprintf(stderr, "ERROR: Could not open file for write\n");
                return ERR;
        }

        u64 total_wrt = 0;
        while (1) {
                packet_t recv_pkt = {0};
                recvfrom(sockfd, (void*)&recv_pkt, MAX_PKT_LEN, 0,
                         (struct sockaddr*)&servaddr, &slen);
                if (recv_pkt.header.type == BYE) {
                        printf("\nReceived fully\n");
                        break;
                }
                u32 wrt = fwrite(recv_pkt.data, 1, recv_pkt.header.length, file);
                total_wrt += wrt;

                packet_t ack_pkt = {0};
                ack_pkt.header.type = ACK;
                u16 n = snprintf((char*)ack_pkt.data, MAX_PLD_LEN,
                                "%lu", recv_pkt.header.seq_num);
                ack_pkt.header.length = n;
                sendto(sockfd, (const void*)&ack_pkt,
                                n + sizeof(header_t), 0,
                                (const struct sockaddr*)&servaddr, slen);

                printf("\rReceived: seq_num=%lu, size=%lu, Sent: ack_num=%s",
                        recv_pkt.header.seq_num, total_wrt, ack_pkt.data);
                fflush(stdout);
        }

        close(sockfd);
        return OK;
}
