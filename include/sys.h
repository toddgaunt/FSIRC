/* See LICENSE file for copyright and license details */
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

int mkdirpath(Ustr const *path);
