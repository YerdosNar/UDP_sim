#ifndef TRANSFER_H
#define TRANSFER_H

#include "packet.h"
#include "types.h"
#include <netinet/in.h>

i8 send_file(i32 fd, struct sockaddr_in *addr, char *filename);
i8 recv_file(i32 fd, struct sockaddr_in *addr);

#endif
