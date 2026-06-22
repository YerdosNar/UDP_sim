#include "../include/packet.h"
#include "../include/transfer.h"
#include "../include/logging.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT            8888
#define BUFF_LEN        1024

int main(int argc, char **argv) {
        char *filename = argv[1];

        i32     sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0) {
                fprintf(stderr, "ERROR: socket() failed\n");
                return ERR_SOCKET;
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
                return ERR_BIND;
        }

        packet_t recv_pkt = {0};
        recvfrom(sockfd, (void*)&recv_pkt, MAX_PKT_LEN, MSG_WAITALL,
                        (struct sockaddr *)&cli_addr, &clen);
        u16 pkt_len = recv_pkt.header.length;
        char recv_msg[pkt_len + 1];
        recv_msg[pkt_len] = '\0';
        strncpy(recv_msg, (const char*)recv_pkt.data, sizeof(recv_msg));
        printf("Received: %s\n", recv_msg);

        const char *send_msg    = "Hello from the Server";
        i8 msg_len              = strlen(send_msg);
        packet_t send_pkt       = {0};
        send_pkt.header.seq_num = 0;
        send_pkt.header.length  = msg_len;
        send_pkt.header.type    = DATA;
        memcpy(send_pkt.data, send_msg, msg_len);
        sendto(sockfd, (const void*)&send_pkt,
               msg_len + sizeof(header_t),
               MSG_CONFIRM,
               (const struct sockaddr*)&cli_addr, clen);

        printf("Sent: %s\n", send_msg);

        printf("\nSent: BYE\n");

        i8 ret = send_file(sockfd, &cli_addr, filename);
        if (ret) {
                return ret;
        }

        close(sockfd);
        return OK;
}
