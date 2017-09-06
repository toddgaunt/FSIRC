/* See LICENSE file for copyright and license details */
#include <fcntl.h>
#include <libstx.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "src/sys.h"

#ifndef PATH_NULL
#define PATH_NULL "/dev/null"
#endif

static inline int
daemonize_fork()
{
	int pid = fork();

	if (0 > pid)
		return -1;

	if (0 < pid)
		exit(EXIT_SUCCESS);

	return 0;
}

void
daemonize()
{
	// Fork once to go into the background.
	if (0 > daemonize_fork())
		exit(-1);

	if (0 > setsid())
		exit(-1);

	// Fork once more to ensure no TTY can be required.
	if (0 > daemonize_fork())
		exit(-1);

	int fd;
	if (!(fd = open(PATH_NULL, O_RDWR)))
		exit(-1);

	// Close standard streams.
	dup2(fd, STDIN_FILENO);
	dup2(fd, STDOUT_FILENO);
	dup2(fd, STDERR_FILENO);

	// Only close the fd if it isn't one of the std streams.
	if (fd > 2)
		close(fd);
}

int
tcpopen(int *sockfd, const spx host, const spx port, 
		int (*opensocket)(int, const struct sockaddr *, socklen_t))
{
	struct addrinfo hints;
	struct addrinfo *res;
	struct addrinfo *ptr;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_CANONNAME;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	// Create null-terminated strings for legacy functions.
	char tmphost[host.len + 1];
	char tmpport[port.len + 1];
	memcpy(tmphost, host.mem, host.len);
	memcpy(tmpport, port.mem, port.len);
	tmphost[host.len] = '\0';
	tmpport[port.len] = '\0';

	// Get the ip address of 'host'.
	if (getaddrinfo(tmphost, tmpport, &hints, &res)) {
		return -1;
	}
	
	// Attempt to connect to any available address.
	for (ptr=res; ptr; ptr=ptr->ai_next) {
		if (0 > (*sockfd = socket(ptr->ai_family, ptr->ai_socktype,
				ptr->ai_protocol)))
			continue;

		if (0 > opensocket(*sockfd, ptr->ai_addr, ptr->ai_addrlen))
			continue;

		// Successful connection.
		break;
	}

	// No connection was made
	if (!ptr) {
		if (res)
			freeaddrinfo(res);
		return -1;
	}

	freeaddrinfo(res);
	return 0;
}
