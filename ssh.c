#include "ssh.h"

const char* ssh_strerror (LIBSSH2_SESSION* session)
{
	char* errmsg;
	int errmsg_len;
	libssh2_session_last_error (session, &errmsg, &errmsg_len, 0);
	return errmsg;
}
