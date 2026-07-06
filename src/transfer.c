#include "../include/types.h"
#include "../include/packet.h"
#include "../include/transfer.h"
#include <arpa/inet.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

/*
 * Sends the metadata of a file (name, and size)
 * and sets the pointer to the beginning.
 *
 * On success: returns 0
 * On failure: returns status_t code
 */
static i8 _send_metadata_and_confirm(char *fname, u64 fsize, i32 fd)
{
        packet_t metadata = {0};

        char *clean_name = basename(fname);
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

        fseek(file, 0, SEEK_END);
        u64 fsize = ftell(file);
        rewind(file);

        i8 ret = _send_metadata_and_confirm(filename, fsize, fd);
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

                printf("\rSent: %lu/%lu, ACKed seq_num: %lu",
                        total_sent, fsize, seq_num);
                fflush(stdout);
        }
        fclose(file);

        packet_t eof       = {0};
        packet_hdr_init(&eof, FILE_EOF, 0, packet_increment_seq_num());
        return packet_send_and_recv_ack(fd, &eof, 0);
}

i8 _recv_metadata_and_send_ack(i32      fd,
                               char     *out_name,
                               u64      *out_size,
                               u64      *seq_num)
{
        packet_t metadata       = {0};
        ssize_t received        = recv(fd, &metadata, MAX_PKT_LEN, 0);
        i8 validate             = packet_validate(&metadata, received);
        header_t hdr            = metadata.header;

        if (validate != OK)             return validate;
        if (hdr.type != FILE_META)      return ERR_PKT_TYPE_MISMATCH;
        if (hdr.length < 3)             return ERR_PKT_MALFORMED;

        char *data              = (char*)metadata.data;
        data[hdr.length]        = '\0';

        char *fname = strtok(data, "|");
        size_t name_len = strlen(fname);
        if (name_len < 1 || name_len == hdr.length)
                return ERR_PKT_MALFORMED;

        char *end       = NULL;
        errno           = 0;
        char *size_str  = strtok(NULL, "|");

        *out_size       = strtoull(size_str, &end, 10);
        if (errno == EINVAL || end == size_str || *end != '\0')
                return ERR_PKT_MALFORMED;

        out_name[name_len] = '\0';
        for (u16 i = 0; i < name_len; i++) {
                if (fname[i] == '/')            return ERR_PKT_MALFORMED;
                out_name[i] = fname[i];
        }
        if (!strcmp(out_name, ".") || !strcmp(out_name, ".."))
                return ERR_PKT_MALFORMED;

        *seq_num        = hdr.seq_num;
        packet_t ack    = {0};
        packet_hdr_init(&ack, ACK, 0, *seq_num);
        if (send(fd, &ack, PKT_SZ(0), 0) == -1) return ERR_NETWORK;

        return OK;
}

i8 transfer_recv_file(i32 fd)
{
        char fname[MAX_PLD_LEN];
        u64 fsize;
        u64 last_seq;
        i8 ret = _recv_metadata_and_send_ack(fd, fname, &fsize, &last_seq);
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

                /* Send ACK regardless of type */
                if (seq_num <= last_seq) {
                        if (send(fd, &ack, PKT_SZ(0), 0) == -1) {
                                fclose(file);
                                return ERR_NETWORK;
                        }
                        continue;
                }

                if (recv_pkt.header.type == FILE_EOF) {
                        fclose(file);
                        if (send(fd, &ack, PKT_SZ(0), 0) == -1)
                                return ERR_NETWORK;
                        return OK;
                }

                i32 wrt = 0;
                if (recv_pkt.header.type == FILE_DATA) {
                        wrt = fwrite(recv_pkt.data, 1, len, file);
                        if (wrt <= 0) {
                                fprintf(stderr, "ERROR: write fail at %luB\n",
                                                total_wrt);
                                fclose(file);
                                return ERR_FILE_WRITE;
                        }
                        total_wrt += wrt;
                        last_seq = seq_num;

#ifdef DROP_TEST
                        if (rand() % 5 == 0) {
                                printf("\n[DROP_TEST] dropped ACK seq %lu\n", seq_num);
                                continue;
                        }
#endif

                        if (send(fd, &ack, PKT_SZ(0), 0) == -1) {
                                fclose(file);
                                return ERR_NETWORK;
                        }
                }

                printf("\rReceived: %lu/%lu, ACKed seq_num: %lu",
                                total_wrt, fsize, seq_num);
                fflush(stdout);
        }

        fclose(file);
        return OK;
}
