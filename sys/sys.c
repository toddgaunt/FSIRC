/* See LICENSE file for copyright and license details */
#include <string.h>
#include <stdlib.h>
#include <libstx.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

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
tcpopen(
		int *sockfd,
		char const *host,
		char const *port, 
		int (*opensocket)(int, const struct sockaddr *, socklen_t))
{
	struct addrinfo hints;
	struct addrinfo *res;
	struct addrinfo *ptr;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_CANONNAME;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	// Get the ip address of 'host'.
	if (getaddrinfo(host, port, &hints, &res))
		return -1;
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

/**
 * Recursively make directories down a fullpath.
 *
 * Return: 0 if directory path is fully created. -1 if mkdir fails.
 */
int
mkdirpath(struct strbuf const *path)
{
	size_t i;
	size_t len;
	char tmp[path->len + 1];

	len = path->len;
	memcpy(tmp, path->mem, path->len);
	tmp[path->len] = '\0';
	if('/' == tmp[len - 1])
		tmp[len - 1] = '\0';
	for (i = 1; i < len; ++i) {
		if('/' == tmp[i]) {
			tmp[i] = '\0';
			if (0 > mkdir(tmp, S_IRWXU))
				if (EEXIST != errno)
					return -1;
			tmp[i] = '/';
		}
	}
	if (0 > mkdir(tmp, S_IRWXU))
		if (EEXIST != errno)
			return -1;
	return 0;
}
