/* See LICENSE file for copyright and license details */
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include "sys.h"

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

int
mkdirpath(char const *path)
{
	size_t i;
	size_t len = strlen(path);
	char tmp[len + 1];

	memcpy(tmp, path, len);
	tmp[len] = '\0';
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

/* Used by readline to strip line delimiters */
static int
rstrip(char *str, char const *chs)
{
	size_t removed = 0;
	size_t len = strlen(str);
	char *begin = str + len - 1;
	char *end = str;

	if (0 == len)
		return 0;
	while (begin != end && strchr(chs, *begin)) {
		++removed;
		--begin;
	}
	str[len - removed] = '\0';
	return removed;
}

int
readline(char *dest, size_t n, int fd)
{
	char ch = '\0';
	size_t i;

	for (i = 0; i < n && '\n' != ch; ++i) {
		if (1 != read(fd, &ch, 1))
			return -1;
		dest[i] = ch;
	}
	dest[i] = '\0';
	/* Remove line delimiters as they make logging difficult, and
	 * aren't always standard, so its easier to add them again later */
	rstrip(dest, "\r\n");
	if (i >= n)
		return 1;
	return 0;
}

