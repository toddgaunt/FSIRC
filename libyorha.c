#include <string.h>
#include <unistd.h>
#include <stdio.h>
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
		if (read(fd, &ch, 1) != 1)
			return -1;
		buf[*len] = ch;
		*len += 1;
	} while (ch != '\n' && *len < maxrecv);
	buf[*len] = '\0';

	return 0;
}
