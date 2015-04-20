#ifndef SSH_H
#define SSH_H

#include "status.h"
#include <libssh2.h>

const char* ssh_strerror (LIBSSH2_SESSION* session);

static inline
status_t sshfailure (LIBSSH2_SESSION* session)
{
	return failure (ssh_strerror (session));
}

#endif
