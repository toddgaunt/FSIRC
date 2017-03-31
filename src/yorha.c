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

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

#define NICK_MAX 32
#define REALNAME_MAX 32
#define MSG_MAX 512

#define DPRINT(...) fprintf(stderr, PRGM_NAME": "__VA_ARGS__)

// Array indices for tokenize_irc_output().
enum {TOK_NICKSRV, TOK_USER, TOK_CMD, TOK_CHAN, TOK_ARG, TOK_TEXT, TOK_LAST};

// Bitmask flags for program arguments.
enum {
	FLAG_DAEMON = 1 << 0,
};

struct irccon {
	int fd;
	int port;
	stx nick;
	stx realname;
	stx host;
};

struct clicon {
	int fd;
	socklen_t addrlen;
	struct sockaddr_in addr;
};

DEFINE_LIST(irccon, struct irccon)
DEFINE_LIST(clicon, struct clicon)

static void usage()
{
	fprintf(stderr, PRGM_NAME" - irc client daemon - %s\n"
		"usage: "PRGM_NAME" [-h] [-p] [-d] HOSTNAME\n", VERSION);
	exit(EXIT_FAILURE);
}

static int daemonize_fork()
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
static int daemonize(int nochdir, int noclose) 
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

static int
run_server(int sockfd) {
	stx buf;
	irccon_list *irc = NULL;
	clicon_list *cli = NULL;

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
		for (irccon_list *sp = irc; sp; sp = sp->next) {
			if (sp->node.fd > maxfd)
				maxfd = sp->node.fd;
			FD_SET(sp->node.fd, &rd);
		}

		for (clicon_list *cp = cli; cp; cp = cp->next) {
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
			clicon_list *tmp;
			tmp = malloc(sizeof(*tmp));
			tmp->node.fd = accept(tmp->node.fd,
					(struct sockaddr *)&tmp->node.addr,
					&tmp->node.addrlen);
			clicon_listadd(cli, tmp);
		}

		// Check for new server messages
		for (irccon_list *sp = irc; sp; sp = sp->next) {
			if (FD_ISSET(sp->node.fd, &rd)) {
				if (0 > yorha_readline(buf.mem, &buf.len,
							buf.size,
							sp->node.fd)) {
				}
			}
		}

		// Check for new client messages
		for (clicon_list *cp = cli; cp; cp = cp->next) {
			if (FD_ISSET(cp->node.fd, &rd)) {
				if (0 > yorha_readline(buf.mem, &buf.len,
							buf.size,
							cp->node.fd)) {
					DPRINT("Failed reading line from client\n");
				}
			}
		}
	}
	
	stxdel(&buf);

	return 0;
}

int main(int argc, char **argv) 
{
	int flags = 0;
	int sockfd = 0;
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

	if (0 > yorha_tcpopen(&sockfd, "localhost", port, bind)) {
		DPRINT("Failed to bind to port '%s'\n", port);
		exit(EXIT_FAILURE);
	}

	if (0 > listen(sockfd, 4)) {
		DPRINT("Failed to listen on port '%s'\n", port);
		exit(EXIT_FAILURE);
	}

	return run_server(sockfd); 
}
