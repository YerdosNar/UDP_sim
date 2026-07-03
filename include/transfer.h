#ifndef TRANSFER_H
#define TRANSFER_H

#include "packet.h"
#include "types.h"
#include <netinet/in.h>

#define MAX_RETRIES     5

i8 transfer_send_file(i32 fd, char *filename);
i8 transfer_recv_file(i32 fd);

#endif
