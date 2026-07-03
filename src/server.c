#include "../include/types.h"
#include "../include/packet.h"
#include "../include/transfer.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define PORT            8888
#define BUFF_LEN        1024

int main(int argc, char **argv) {
        if (2 > argc) {
                fprintf(stderr, "ERROR: No file provided\n");
                printf("Usage: %s <filename>\n", argv[0]);
                return ERR_FILE_OPEN;
        }
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

        packet_t recv_pkt       = {0};
        ssize_t received        = recvfrom(sockfd, (void*)&recv_pkt,
                                  MAX_PKT_LEN, MSG_WAITALL,
                                  (struct sockaddr *)&cli_addr, &clen);
        i8 validate             = packet_validate(&recv_pkt, received);
        if (validate != OK) return validate;

        printf("Received: %.*s\n", recv_pkt.header.length, recv_pkt.data);
        const char *check       = "Hello from the Client";
        const u16   check_len   = strlen(check);
        if (recv_pkt.header.type   != HELLO     ||
            recv_pkt.header.length != check_len ||
            memcmp(check, recv_pkt.data, check_len)) {
                fprintf(stderr, "ERROR: Not protocol packet arrived\n");
                return ERR_PKT_MALFORMED;
        }

        struct timeval tv = {.tv_sec = 0, .tv_usec = 500*1000};
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        if (connect(sockfd, (const struct sockaddr*)&cli_addr, clen) == -1) {
                fprintf(stderr, "ERROR: connect(cli_addr) failed\n");
                return ERR_CONN_REFUSED;
        }

        const char *send_msg    = "Hello from the Server";
        i8 msg_len              = strlen(send_msg);
        packet_t send_pkt       = {0};
        packet_hdr_init(&send_pkt, HELLO_ACK,
                        msg_len, packet_increment_seq_num());
        memcpy(send_pkt.data, send_msg, msg_len);
        if (send(sockfd, &send_pkt, PKT_SZ(msg_len), 0) == -1)
                return ERR_NETWORK;

        printf("Sent: %s\n", send_msg);


        i8 ret = transfer_send_file(sockfd, filename);

        printf("\nSent: BYE\n");
        close(sockfd);

        return ret;
}
