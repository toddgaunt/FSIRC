#include <netdb.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>

/**
 * Establish a new tcp connection as a server or client.
 *
 * The last argument of this function should be either bind() or connect(). If
 * the function provided is bind(), then 'sockfd' will be bound to a new tcp
 * socket. If instead connect() is provided, then 'sockfd' will be assigned
 * tcp socket connected to the 'host':'port'.
 *
 * Return: 0 upon success, -1 if there was an error using the sockt.
 */
int
yorha_tcpopen(int *sockfd, const char *host, const char *port, 
		int (*open)(int, const struct sockaddr *, socklen_t));

/**
 * Read characters from a file descriptor into a buffer.
 *
 * Read characters from 'fd' into 'buf' until either 'maxrecv' characters have
 * been read or a newline character is read. The newline character is included
 * in the read.
 * 
 * Return: Number of characters read into 'buf'.
 */
int
yorha_readline(char *buf, size_t *len, size_t maxrecv, int fd);
