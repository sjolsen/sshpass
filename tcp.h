#ifndef TCP_H
#define TCP_H

#include "status.h"

status_t connect_tcp (int (*the_socket),
                      const char* address,
                      const char* port);

#endif
