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
static i8 _send_metadata_and_confirm(FILE *file, char *fname, i32 fd)
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

        return packet_send_and_recv_ack(fd, &metadata, n);
}

i8 transfer_send_file(i32 fd, char *filename)
{
        if (!filename || fd <= 0) return ERR_NULL_PTR;

        FILE *file = fopen(filename, "rb");
        if (!file) return ERR_FILE_OPEN;

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

                ret = packet_send_and_recv_ack(fd, &send_pkt, read_bytes);
                if (ret) {
                        fclose(file);
                        return ret;
                }

                printf("\rSent: %lu, ACKed seq_num: %lu", total_sent, seq_num);
                fflush(stdout);
        }
        fclose(file);

        packet_t eof       = {0};
        packet_hdr_init(&eof, FILE_EOF, 0, packet_increment_seq_num());
        return packet_send_and_recv_ack(fd, &eof, 0);
}

i8 _recv_metadata_and_send_ack(i32 fd, char *out_name)
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

i8 transfer_recv_file(i32 fd)
{
        char fname[MAX_PLD_LEN];
        i8 ret = _recv_metadata_and_send_ack(fd, fname);
        if (ret) return ret;

        FILE *file = fopen(fname, "wb");
        if (!file) return ERR_FILE_OPEN;

        u64 total_wrt = 0;
        while (1) {
                packet_t recv_pkt = {0};
                ssize_t received  = recv(fd, &recv_pkt, MAX_PKT_LEN, 0);
                if (packet_validate(&recv_pkt, received) != OK) continue;

                u64 seq_num       = recv_pkt.header.seq_num;
                u16 len           = recv_pkt.header.length;
                packet_t ack      = {0};
                packet_hdr_init(&ack, ACK, 0, seq_num);

                if (recv_pkt.header.type == FILE_EOF) {
                        if (send(fd, &ack, PKT_SZ(0), 0) == -1)
                                return ERR_NETWORK;
                        fclose(file);
                        return OK;
                }
                i32 wrt = fwrite((const void*)recv_pkt.data, 1, len, file);
                if (wrt <= 0) {
                        fprintf(stderr, "ERROR: write failed at %luB\n",
                                        total_wrt);
                        fclose(file);
                        return ERR_FILE_WRITE;
                }
                total_wrt += wrt;

                if (send(fd, &ack, PKT_SZ(0), 0) == -1) {
                        fclose(file);
                        return ret;
                }

                printf("\rReceived: %lu, ACKed seq_num: %lu",
                                total_wrt, seq_num);
                fflush(stdout);
        }

        fclose(file);
        return OK;
}
