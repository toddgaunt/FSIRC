#include <netdb.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>

/**
 * Establish a new tcp connection as a server or client.
 *
 * The last argument of this function should be either bind() or connect(). If
 * the function provided is bind(), then 'sockfd' will point to a newly bound
 * tcp socket. If instead connect() is provided, then 'sockfd' will point to a
 * tcp socket connected to the remote host:port.
 */
int
yorha_tcpopen(int *sockfd, const char *host, const char *port, 
		int (*open)(int, const struct sockaddr *, socklen_t));

int
yorha_readline(char *buf, size_t *len, size_t maxrecv, int fd);
