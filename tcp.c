#define _POSIX_SOURCE
#include "tcp.h"
#include <stdlib.h>
#include <netdb.h>
#include <arpa/inet.h>

status_t connect_tcp (int (*the_socket),
                      const char* address,
                      const char* port)
{
	// Use TCP
	struct protoent* proto_tcp = getprotobyname ("tcp");
	if (proto_tcp == NULL)
		return failure ("TCP not supported");

	// Prepare a TCP/IPv4 socket
	int _the_socket = socket (PF_INET, SOCK_STREAM, proto_tcp->p_proto);
	if (_the_socket == -1)
		return efailure (errno);

	// Prepare IPv4 address given by address:port
	struct sockaddr_in svaddr = (struct sockaddr_in) {.sin_family = AF_INET};

	switch (inet_pton (AF_INET, address, &svaddr.sin_addr)) {
		case 1:  break;
		case 0:  return failure ("Invalid network address");
		default: return efailure (errno);
	}

	const char* endptr = port;
	long int port_l = strtoul (port, (char**)&endptr, 10);
	if ((*port == '\0' || *endptr != '\0') ||
	    (port_l < 0    || port_l > 65535))
		return failure ("Invalid port number");
	svaddr.sin_port = htons (port_l);

	// Connect TCP socket to address:port
	if (connect (_the_socket, (struct sockaddr*)&svaddr, sizeof (svaddr)) != 0)
		return efailure (errno);

	(*the_socket) = _the_socket;
	return success ();
}
