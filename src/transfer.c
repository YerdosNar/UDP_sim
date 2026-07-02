#include "../include/types.h"
#include "../include/packet.h"
#include "../include/transfer.h"
#include <arpa/inet.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

/*
 * Sends the metadata of a file (name, and size)
 * and sets the pointer to the beginning.
 *
 * On success: returns 0
 * On failure: returns status_t code
 */
static i8 _send_metadata_and_confirm(FILE               *file,
                                     char               *fname,
                                     i32                fd)
{
        packet_t metadata = {0};

        char *clean_name = basename(fname);
        fseek(file, 0, SEEK_END);
        u64 fsize = ftell(file);
        rewind(file);

        i32 n = snprintf((char*)metadata.data, MAX_PLD_LEN,
                        "%s|%lu", clean_name, fsize);
        if (n < 0 || n >= (i32)MAX_PLD_LEN) return ERR_PKT_MALFORMED;

        packet_hdr_init(&metadata, FILE_META, n, packet_increment_seq_num());
        u64 seq_num = metadata.header.seq_num;

        if (send(fd, &metadata, PKT_SZ(n), 0) == -1) return ERR_NETWORK;

        /* Let's confirm */
        packet_t metadata_ack = {0};
        ssize_t received = recv(fd, &metadata_ack, MAX_PKT_LEN, 0);
        i8 validate = packet_validate(&metadata_ack, received);
        if (validate != OK) return validate;

        u8 type = metadata_ack.header.type;
        if (type != ACK) {
                fprintf(stderr, "ERROR: Type mismatch. Received: %d,"
                                " Expected: %d\n", type, ACK);
                return ERR_PKT_TYPE_MISMATCH;
        }

        u64 ack_seq = metadata_ack.header.seq_num;
        if (ack_seq != seq_num) {
                fprintf(stderr, "ERROR: ACK seq mismatch. Received: %lu,"
                                " Expected: %lu\n", ack_seq, seq_num);
                return ERR_SEQ_MISMATCH;
        }

        return OK;
}

i8 send_file(i32 fd, char *filename)
{
        if (!filename || fd <= 0) {
                fprintf(stderr, "ERROR: NULL pointer\n");
                return ERR_NULL_PTR;
        }
        FILE *file = fopen(filename, "rb");
        if (!file) {
                fprintf(stderr, "ERROR: fopen(%s) failed\n", filename);
                return ERR_FILE_OPEN;
        }

        i8 ret = _send_metadata_and_confirm(file, filename, fd);
        if (ret) {
                fclose(file);
                return ret;
        }

        u64 total_sent = 0;
        while (1) {
                packet_t send_pkt = {0};
                u16 read_bytes = fread(send_pkt.data, 1, MAX_PLD_LEN, file);
                if (read_bytes == 0) {
                        if (feof(file)) break;
                        fprintf(stderr, "ERROR: file read failed at %luB\n",
                                        total_sent);
                        fclose(file);
                        return ERR_FILE_READ;
                }
                u64 seq_num = packet_increment_seq_num();
                packet_hdr_init(&send_pkt, FILE_DATA, read_bytes, seq_num);

                total_sent += read_bytes;

                if (send(fd, &send_pkt, PKT_SZ(read_bytes), 0) == -1)
                        return ERR_NETWORK;

                packet_t recv_pkt = {0};
                ssize_t received = recv(fd, &recv_pkt, MAX_PKT_LEN, 0);
                i8 validate = packet_validate(&recv_pkt, received);
                if (validate != OK) { fclose(file); return validate; }

                i8 type = recv_pkt.header.type;
                if (type != ACK) {
                        fprintf(stderr, "ERROR: Type mismatch. Received: %d,"
                                        " Expected: %d\n", type, ACK);
                        fclose(file);
                        return ERR_PKT_TYPE_MISMATCH;
                }

                u64 ack_seq = recv_pkt.header.seq_num;
                if (ack_seq != seq_num) {
                        fprintf(stderr, "ERROR: ACK seq mismatch. Received: %lu,"
                                        " Expected: %lu\n", ack_seq, seq_num);
                        fclose(file);
                        return ERR_SEQ_MISMATCH;
                }

                printf("\rSent: %lu, ACKed seq_num: %lu", total_sent, seq_num);
                fflush(stdout);
        }

        packet_t eof       = {0};
        packet_hdr_init(&eof, FILE_EOF, 0, packet_increment_seq_num());
        if (send(fd, &eof, PKT_SZ(0), 0) == -1) return ERR_NETWORK;

        printf("\nFile is delivered successfully\n");

        fclose(file);
        return OK;
}

i8 _recv_metadata_and_send_ack(i32                       fd,
                               char                     *out_name)
{
        packet_t metadata = {0};
        ssize_t received = recv(fd, &metadata, MAX_PKT_LEN, 0);
        i8 validate = packet_validate(&metadata, received);

        if (validate != OK)                     return validate;
        if (metadata.header.type != FILE_META)  return ERR_PKT_TYPE_MISMATCH;
        if (metadata.header.length < 3)         return ERR_PKT_MALFORMED;

        u16 last_idx = 0;
        for (u16 i = metadata.header.length-1; i > 0; i--) {
                if (metadata.data[i] == '|') {
                        last_idx = i;
                        break;
                }
        }

        if (last_idx < 1) {
                fprintf(stderr, "ERROR: Empty filename\n");
                return ERR_PKT_MALFORMED;
        }

        for (u16 i = 0; i < last_idx; i++) {
                if (metadata.data[i] == '/') {
                        fprintf(stderr, "ERROR: Filename contains '/'\n");
                        return ERR_PKT_MALFORMED;
                }
                out_name[i] = metadata.data[i];
        }
        out_name[last_idx] = '\0';
        if (!strcmp(out_name, ".") || !strcmp(out_name, "..")) {
                fprintf(stderr, "ERROR: Invalid filename\n");
                return ERR_PKT_MALFORMED;
        }

        /* Now let's ACK        */
        packet_t ack            = {0};
        packet_hdr_init(&ack, ACK, 0, metadata.header.seq_num);

        if (send(fd, &ack, PKT_SZ(0), 0) == -1) return ERR_NETWORK;

        return OK;
}

i8 recv_file(i32 fd)
{
        char fname[MAX_PLD_LEN];
        i8 ret = _recv_metadata_and_send_ack(fd, fname);
        if (ret) {
                return ret;
        }

        FILE *file = fopen(fname, "wb");
        if (!file) {
                fprintf(stderr, "ERROR: fopen(fname) failed\n");
                return ERR_FILE_OPEN;
        }

        u64 total_wrt = 0;
        while (1) {
                packet_t recv_pkt = {0};
                ssize_t received = recv(fd, &recv_pkt, MAX_PKT_LEN, 0);
                if (packet_validate(&recv_pkt, received) != OK) {
                        continue; // Just skip this packet
                }

                if (recv_pkt.header.type == FILE_EOF) {
                        printf("\nReceived fully\n");
                        break;
                }
                u16 len = recv_pkt.header.length;
                u64 seq_num = recv_pkt.header.seq_num;
                i32 wrt = fwrite((const void*)recv_pkt.data, 1, len, file);
                if (wrt <= 0) {
                        fprintf(stderr, "ERROR: write failed at %luB\n",
                                        total_wrt);
                        fclose(file);
                        return ERR_FILE_WRITE;
                }
                total_wrt += wrt;

                packet_t ack            = {0};
                packet_hdr_init(&ack, ACK, 0, seq_num);

                if (send(fd, &ack, PKT_SZ(0), 0) == -1) {
                        fclose(file);
                        return ERR_NETWORK;
                }

                printf("\rReceived: %lu, ACKed seq_num: %lu",
                                total_wrt, seq_num);
                fflush(stdout);
        }

        fclose(file);
        return OK;
}
