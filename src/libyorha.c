#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "libyorha.h"

int
yorha_tcpopen(int *sockfd, const char *host, const char *port, 
		int (*open)(int, const struct sockaddr *, socklen_t))
{
	struct addrinfo hints;
	struct addrinfo *res;
	struct addrinfo *ptr;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	// Get the ip address of 'host'.
	if (getaddrinfo(host, port, &hints, &res)) {
		return -1;
	}
	
	// Attempt to connect to any available address.
	for (ptr=res; ptr; ptr=ptr->ai_next) {
		if (0 > (*sockfd = socket(ptr->ai_family, ptr->ai_socktype,
				ptr->ai_protocol)))
			continue;

		if (0 > open(*sockfd, ptr->ai_addr, ptr->ai_addrlen))
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
yorha_readline(char *buf, size_t *len, size_t maxrecv, int fd)
{
	char ch;

	*len = 0;
	do {
		// Unable to read anymore, as the buffer is full.
		if (*len >= maxrecv)
			return maxrecv;

		if (1 != read(fd, &ch, 1))
			return -1;

		buf[*len] = ch;
		*len += 1;
	} while (ch != '\n');

	return 0;
}

int
yorha_send_msg(int sockfd, const char *buf, size_t len)
{
	return 0;
}
	
static int
daemonize_fork()
{
	int pid = fork();

	if (0 > pid)
		return -1;

	if (0 < pid)
		exit(EXIT_SUCCESS);

	return 0;
}

/**
 * Daemonize a process. If nochdir is 1, the process won't switch to '/'.
 * If noclose is 1, standard streams won't be redirected to /dev/null.
 */
int
yorha_daemonize()
{
	// Fork once to go into the background.
	if (0 > daemonize_fork())
		return -1;

	if (0 > setsid())
		return -1;

	// Fork once more to ensure no TTY can be required.
	if (0 > daemonize_fork())
		return -1;

	int fd;
	if (!(fd = open("/dev/null", O_RDWR)))
		return -1;
	dup2(fd, STDIN_FILENO);
	dup2(fd, STDOUT_FILENO);
	dup2(fd, STDERR_FILENO);
	// Only close the fd if it isn't one of the std streams.
	if (fd > 2)
		close(fd);

	chdir("/");

	return 0;
}
