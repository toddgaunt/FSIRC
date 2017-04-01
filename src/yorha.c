/* See LICENSE file for copyright and license details */
#include <arpa/inet.h> 
#include <errno.h>
#include <fcntl.h>
#include <libstx.h>
#include <limits.h>
#include <malloc.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "libyorha.h"
#include "yorha_config.h"
#include "yorha_list.h"

#ifndef PRGM_NAME
#define PRGM_NAME "null"
#endif

#ifndef VERSION
#define VERSION "0.0.0"
#endif

#ifndef PREFIX
#define PREFIX "/usr/local"
#endif

#ifndef PATH_NULL
#define PATH_NULL "/dev/null"
#endif

#define LOGINFO(...)\
	{logtime(stderr);\
	fprintf(stderr, " "PRGM_NAME": info: "__VA_ARGS__);}

#define LOGERROR(...)\
	{logtime(stderr);\
	fprintf(stderr, " "PRGM_NAME": error: "__VA_ARGS__);}

#define LOGFATAL(...)\
	{logtime(stderr);\
	fprintf(stderr, " "PRGM_NAME": fatal: "__VA_ARGS__);\
	exit(EXIT_FAILURE);}

// Bitmask flags for program arguments.
enum {
	FLAG_DAEMON = 1 << 0,
};

struct irc {
	int fd;
	int port;
	stx nick;
	stx realname;
	stx host;
};

struct client {
	int fd;
	socklen_t addrlen;
	struct sockaddr_in addr;
};

LIST_DEFINE(irc_list, struct irc)
LIST_DEFINE(client_list, struct client)

static void
usage()
{
	fprintf(stderr, "usage: "PRGM_NAME" [-h] [-p] [-v] [-D]\n");
	exit(EXIT_FAILURE);
}

void
logtime(FILE *fp)
{
	char date[16];
	time_t t;
	const struct tm *tm;

	t = time(NULL);
	tm = localtime(&t);
	strftime(date, sizeof(date), "%m %d %T", tm);
	fprintf(fp, date);
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
static int
daemonize(int nochdir, int noclose) 
{
	// Fork once to go into the background.
	if (0 > daemonize_fork())
		return -1;

	if (0 > setsid())
		return -1;

	if (!nochdir) {
		chdir("/");
	}

	// Fork once more to ensure no TTY can be reaquired.
	if (0 > daemonize_fork())
		return -1;

	if (!noclose) {
		int fd;
		if (!(fd = open(PATH_NULL, O_RDWR)))
			return -1;
		dup2(fd, STDIN_FILENO);
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);
		// Only close the fd if it isn't one of the std streams.
		if (fd > 2)
			close(fd);
	}

	return 0;
}

void
run(const char *port)
{
	int sockfd = 0;
	struct irc_list *irc = NULL;
	struct client_list *cli = NULL;
	stx buf;

	LOGINFO("Successfully initialized.\n");

	if (0 > yorha_tcpopen(&sockfd, "localhost", port, bind))
		LOGFATAL("Failed to bind to port %s.\n", port);

	if (0 > listen(sockfd, 4))
		LOGFATAL("Failed to listen on port %s.\n", port);

	stxnew(&buf, MSG_MAX);

	while (sockfd) {
		int maxfd;
		fd_set rd;
		struct timeval tv;
		int rv;

		FD_ZERO(&rd);

		// Reset all file descriptors and find the largest. 
		maxfd = sockfd;
		FD_SET(sockfd, &rd);
		for (struct irc_list *sp = irc; sp; sp = sp->next) {
			if (sp->node.fd > maxfd)
				maxfd = sp->node.fd;
			FD_SET(sp->node.fd, &rd);
		}

		for (struct client_list *cp = cli; cp; cp = cp->next) {
			if (cp->node.fd > maxfd)
				maxfd = cp->node.fd;
			FD_SET(cp->node.fd, &rd);
		}

		tv.tv_sec = 120;
		tv.tv_usec = 0;
		select(maxfd + 1, &rd, 0, 0, &tv);

		if (rv < 0) {
		} else if (rv == 0) {
		}

		// Check for any new client connections.
		if (FD_ISSET(sockfd, &rd)) {
			struct client_list *tmp;
			tmp = malloc(sizeof(*tmp));
			if (!tmp) {
				LOGERROR("Unable to allocate memory for new client connection\n.");
			} else {
				tmp->node.fd = accept(tmp->node.fd,
					(struct sockaddr *)&tmp->node.addr,
					&tmp->node.addrlen);
				client_list_prepend(cli, tmp);
			}
		}

		// Check for new server messages
		for (struct irc_list *sp = irc; sp; sp = sp->next) {
			if (FD_ISSET(sp->node.fd, &rd)) {
				if (0 > yorha_readline(buf.mem, &buf.len,
							buf.size,
							sp->node.fd)) {
					//TODO(todd): error
					LOGERROR("Unable to read message from server\n");
				} else {
					//TODO(todd): do something
				}
			}
		}

		// Check for new client messages
		for (struct client_list *cp = cli; cp; cp = cp->next) {
			if (FD_ISSET(cp->node.fd, &rd)) {
				if (0 > yorha_readline(buf.mem, &buf.len,
							buf.size,
							cp->node.fd)) {
					//TODO(todd): error
				} else {
					//TODO(todd): do something
				}
			}
		}
	}
	
	stxdel(&buf);
}

int
main(int argc, char **argv) 
{
	int flags = 0;
	const char *port = NULL;

	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
			case 'h': 
				usage(); 
				break;
			case 'v': 
				//version();
				break;
			case 'p':
				++i;
				port = argv[i];
				break;
			case 'D': 
				flags |= FLAG_DAEMON; 
				break;
			default: 
				usage();
			}
		} else {
			usage();
		}
	}
	
	if (!port)
		port = default_port;

	// Daemonize last so that any argument errors can be printed to caller.
	if (FLAG_DAEMON == (flags & FLAG_DAEMON))
		daemonize(0, 0);

	run(port);

	return 0;
}
