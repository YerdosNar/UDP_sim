#include "../include/logging.h"
#include "../include/packet.h"
#include <arpa/inet.h>
#include <netinet/in.h>
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
        pkt.header.seq_id = 0;
        pkt.header.type = DATA;
        pkt.header.length = msg_len;
        strncpy((char*)pkt.data, msg, msg_len);
        sendto(sockfd, (const void*)&pkt,
               msg_len + sizeof(header_t),
               MSG_CONFIRM,
               (const struct sockaddr *)&servaddr, slen);
        printf("Sent: \"%s\" to %s:%d\n", msg, ADDR, PORT);

        packet_t rcv_pkt;
        recvfrom(sockfd, (void*)&rcv_pkt, MAX_PKT_LEN, MSG_WAITALL,
                 (struct sockaddr*)&servaddr, &slen);

        rcv_pkt.data[rcv_pkt.header.length] = '\0';
        printf("Received: %s from %s:%d\n", rcv_pkt.data, ADDR, PORT);

        return OK;
}
