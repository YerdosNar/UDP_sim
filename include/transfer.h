#ifndef TRANSFER_H
#define TRANSFER_H

#include "packet.h"
#include "types.h"
#include <netinet/in.h>

i8 send_file(i32 fd, char *filename);
i8 recv_file(i32 fd);

#endif
