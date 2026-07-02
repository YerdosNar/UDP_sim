#include "../include/types.h"
#include "../include/packet.h"
#include "../include/transfer.h"
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
                return ERR_SOCKET;
        }

        struct sockaddr_in servaddr     = {0};
        servaddr.sin_family             = AF_INET;
        servaddr.sin_port               = htons(PORT);
        servaddr.sin_addr.s_addr        = inet_addr(ADDR);
        socklen_t slen                  = sizeof(servaddr);

        const char *msg                 = "Hello from the Client";
        i8 msg_len                      = strlen(msg);
        packet_t pkt                    = {0};
        packet_hdr_init(&pkt, HELLO, msg_len, packet_increment_seq_num());
        memcpy(pkt.data, msg, msg_len);
        sendto(sockfd, (const void*)&pkt, msg_len + HDR_SZ,
               0, (const struct sockaddr *)&servaddr, slen);
        printf("Sent: \"%s\" to %s:%d\n", msg, ADDR, PORT);

        if (connect(sockfd, (const struct sockaddr*)&servaddr, slen) == -1) {
                fprintf(stderr, "ERROR: connect(servaddr) failed\n");
                return ERR_CONN_REFUSED;
        }

        packet_t rcv_pkt = {0};
        ssize_t received = recv(sockfd, &rcv_pkt, MAX_PKT_LEN, 0);

        i8 validate = packet_validate(&rcv_pkt, received);
        if (validate != OK) return validate;
        if (rcv_pkt.header.type != HELLO_ACK) return ERR_PKT_TYPE_MISMATCH;

        printf("Received: %.*s from %s:%d\n",
                        rcv_pkt.header.length, rcv_pkt.data, ADDR, PORT);
        i8 ret = recv_file(sockfd);
        if (ret) {
                return ret;
        }

        close(sockfd);
        return OK;
}
