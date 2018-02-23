/* See LICENSE file for copyright and license details */
#ifndef FSIRCC_SYS_H
#define FSIRCC_SYS_H

#include <stdlib.h>

/**
 * Daemonize a process by forking it twice, redirecting standard streams
 * to PATH_NULL, and changing directory to "/".
 */
void daemonize();
/**
 * Open a tcp connection and save the file descriptor to "sockfd". The last
 * parameter is either the connect() or bind() fucntion depending on the
 * desired behavior.
 */
int tcpopen(
		int *sockfd,
		char const *host,
		char const *port, 
		int (*opensocket)(int, const struct sockaddr *, socklen_t)
		);

/**
 * Recursively make directories down a fullpath.
 *
 * Return: 0 if directory path is fully created. -1 if mkdir fails.
 */
int mkdirpath(char const *path);

/* Read characters from a file descriptor until a newline is reached. The line
 * delimiters '\r' and '\n' are removed from the line
 */
int readline(char *dest, size_t n, int fd);

#endif
