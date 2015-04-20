#include "status.h"
#include "tcp.h"
#include "lines.h"
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

	line_t line;
	check (make_line (&line), "General failure");
	while (1) {
		check (get_line (&line, dict_file), "Failed to read password");
		if (feof (dict_file))
			break;
		printf ("%s\n", line.ntbs);
	}

	close (ssh_socket);
	fclose (dict_file);
	return EXIT_SUCCESS;
}
