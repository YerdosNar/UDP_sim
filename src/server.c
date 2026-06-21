#include "../include/packet.h"
#include "../include/logging.h"
#include <arpa/inet.h>
#include <libgen.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT            8888
#define BUFF_LEN        1024

int main(int argc, char **argv) {
        char *filename = argv[1];
        char *base = basename(filename);
        u32 namesize = strlen(filename);
        if (namesize > 0xff) {
                fprintf(stderr, "ERROR: Why the name is so large?\n");
                return ERR;
        }

        i32     sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0) {
                fprintf(stderr, "ERROR: socket() failed\n");
                return ERR;
        }

        struct sockaddr_in servaddr = {0},
                           cli_addr = {0};
        servaddr.sin_family         = AF_INET;
        servaddr.sin_port           = htons(PORT);
        servaddr.sin_addr.s_addr    = INADDR_ANY;
        socklen_t slen              = sizeof(servaddr);
        socklen_t clen              = sizeof(cli_addr);

        if (bind(sockfd, (const struct sockaddr*)&servaddr, slen) < 0) {
                fprintf(stderr, "ERROR: bind() failed\n");
                return ERR;
        }

        packet_t recv_pkt = {0};
        recvfrom(sockfd, (void*)&recv_pkt, MAX_PKT_LEN, MSG_WAITALL,
                        (struct sockaddr *)&cli_addr, &clen);
        u16 pkt_len = recv_pkt.header.length;
        char recv_msg[pkt_len + 1];
        recv_msg[pkt_len] = '\0';
        strncpy(recv_msg, (const char*)recv_pkt.data, sizeof(recv_msg));
        printf("Received: %s\n", recv_msg);

        const char *send_msg = "Hello from the Server";
        i8 msg_len = strlen(send_msg);
        packet_t send_pkt = {0};
        send_pkt.header.seq_num = 0;
        send_pkt.header.length = msg_len;
        send_pkt.header.type   = DATA;
        memcpy(send_pkt.data, send_msg, msg_len);
        sendto(sockfd, (const void*)&send_pkt,
               msg_len + sizeof(header_t),
               MSG_CONFIRM,
               (const struct sockaddr*)&cli_addr, clen);

        printf("Sent: %s\n", send_msg);

        FILE *file = fopen(filename, "rb");
        if (!file) {
                fprintf(stderr, "ERROR: fopen() failed\n");
                return ERR;
        }
        fseek(file, 0, SEEK_END);
        u64 fsize = ftell(file);
        rewind(file);

        packet_t metadata = {0};
        i32 n = snprintf((char*)metadata.data, MAX_PLD_LEN,
                        "%s|%lu", base, fsize);
        metadata.header.type = DATA;
        metadata.header.length = n;
        metadata.header.seq_num = 0;
        sendto(sockfd, (const void*)&metadata,
               sizeof(header_t) + n,
               0,
               (const struct sockaddr*)&cli_addr, clen);
        printf("Sent metadata: %s\n", metadata.data);

        u64 total_read = 0;
        u64 seq_num = 0;
        while (total_read < fsize) {
                u8 buffer[MAX_PLD_LEN];
                u16 read_b = fread(buffer, 1, MAX_PLD_LEN, file);
                if (read_b == 0) {
                        if (feof(file)) break;
                        fprintf(stderr, "ERROR: read() failed\n");
                        return ERR;
                }
                total_read += read_b;

                packet_t pkt = {0};
                pkt.header.type = DATA;
                pkt.header.seq_num = seq_num;
                pkt.header.length = read_b;
                memcpy(pkt.data, buffer, read_b);

                sendto(sockfd, (const void*)&pkt,
                       pkt.header.length + sizeof(header_t), 0,
                       (const struct sockaddr*)&cli_addr, clen);

                packet_t ack = {0};
                recvfrom(sockfd, (void*)&ack, sizeof(packet_t),
                         0, (struct sockaddr*)&cli_addr, &clen);
                u64 ack_num = strtoull((char*)ack.data, NULL, 0);

                printf("\rSent: seq_num=%lu, size=%lu, Recv: ack_num=%lu",
                                seq_num++, total_read, ack_num);
                fflush(stdout);
        }

        packet_t pkt_end = {0};
        pkt_end.header.type = BYE;
        sendto(sockfd, (const void*)&pkt_end,
               sizeof(header_t),
               MSG_CONFIRM,
               (const struct sockaddr*)&cli_addr, clen);
        printf("\nSent: BYE\n");

        fclose(file);
        close(sockfd);
        return OK;
}
