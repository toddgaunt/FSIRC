/* See LICENSE file for copyright and license details */
#include <arpa/inet.h> 
#include <errno.h>
#include <fcntl.h>
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
#include <unistd.h>
#include <time.h>

#include "libnerv.h"
#include "config.h"

#ifndef PRGM_NAME
#define PRGM_NAME "irccd"
#endif

#ifndef VERSION
#define VERSION "0.0.0"
#endif

#ifndef PREFIX
#define PREFIX "/usr/local"
#endif

#define PATH_DEVNULL "/dev/null"

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

#define NICK_MAX 32
#define REALNAME_MAX 32
#define MSG_MAX 512

#define DPRINT(...) fprintf(stderr, PRGM_NAME": "__VA_ARGS__)
#define EPRINT(x) perror(PRGM_NAME": "x)

// Array indices for tokenize_irc_output().
enum {TOK_NICKSRV, TOK_USER, TOK_CMD, TOK_CHAN, TOK_ARG, TOK_TEXT, TOK_LAST};

// Bitmask flags for program arguments.
enum flags {
	FLAG_DAEMON = 1 << 0,
};

struct irc_conn {
	struct server *next;
	int fd;
	int port;
	sds *nick;
	sds *realname;
	sds *host;
};

struct cli_conn {
	struct client *next;
	int fd;
	socklen_t addrlen;
	struct sockaddr_in addr;
};

static void usage()
{
	fprintf(stderr, PRGM_NAME" - irc client daemon - %s\n"
		"usage: "PRGM_NAME" [-h] [-p] [-d] HOSTNAME\n", VERSION);
	exit(EXIT_FAILURE);
}

static int daemonize_fork()
{
	switch (fork()) { 
	case -1:
		return -1;
	case 0: 
		return 0;
	default: 
		exit(EXIT_SUCCESS);
	}
}

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
		if (!(fd = open(PATH_DEVNULL, O_RDWR)))
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

/**
 * Create a new channel struct, and open a directory containing all input/output
 * files used to by the channel to communicate.
 *
 * The path is provied by the 'name' parameter. 'len' is the length of 'name'.
static struct channel*
irc_open(char *path, size_t pathlen)
{
	struct channel *chan;
	char path[namelen + 4];

	if (!(chan = malloc(sizeof(*chan))))
		goto except;

	memset(chan, 0, sizeof(*chan));

	// Create the channel directory.
	if (mkdir(name, S_IRWXU)) {
		if (EEXIST != errno) {
			EPRINT("Cannot create directory");
			goto cleanup_chan_name;
		}
	}

	// Make the channel's input FIFO.
	snprintf(path, namelen + 4, "%s/in", name);

	if (0 > mkfifo(path, S_IRWXU)) {
		if (EEXIST != errno) {
			EPRINT("Cannot create FIFO file");
			goto cleanup_chan_name;
		}
	}

	if (0 > oystr_assign(&chan->name, name, namelen))
		goto cleanup_chan;

	if (!(chan->fd = open(path, O_RDONLY | O_NONBLOCK)))
		goto cleanup_chan_name;

	return chan;

cleanup_chan_name:
	oystr_deinit(&chan->name);
cleanup_chan:
	free(chan);
except:
	return NULL;
}
*/

/**
 * Close a channel struct and cleanup it's corresponding directory on the
 * filesystem.
static void
channel_close(struct channel *chan)
{
	struct channel *garbage;
	struct channel **pp;

	pp = &chan;
	garbage = chan;

	*pp = chan->next;

	close(garbage->fd);
	oystr_deinit(&garbage->name);
	free(garbage);
}
 */

/**
 * Cleanup all channels and exit.
static void
channel_cleanup(struct channel *chan_head) {
	DPRINT("Cleaning up directory tree... ");
	DFLUSH();
	while (chan_head) {
		channel_close(chan_head);
	}
	DPRINT("done\n");
	exit(EXIT_FAILURE);
}


static int
log_write(const char *path, const char *msg, size_t msglen)
{
	FILE *fp;
	time_t rt;
	struct ts *tm;
	char tmp[16];

	if (!(fp = fopen(path, "a")))
		goto except;

	//NOTE: None of these are likely to fail.
	time(&rt);
	tm = localtime(&rt);
	tmplen = strftime(tmp, 20, "%Y-%m-%d", tm)
	fwrite(tmp, 20, msglen, fp);

	if (msglen != fwrite(msg, 1, msglen, fp))
		goto cleanup_fp;

	fclose(fp);

	return 0;

cleanup_fp:
	fclose(fp);
except:
	DPRINT("Unable to log file to %s.\n", path);
	return -1;
}
*/

clients_add(struct clients *clients, int sockfd)
{
}

static int
run_server(int sockfd) {
	int i;
	int maxfd;
	fd_set rd;
	struct timeval tv;
	time_t last_response;
	char buf[MSG_MAX + 1];
	size_t buflen;
	struct irc_conn *irc = NULL;
	struct cli_conn *cli = NULL;
	struct irc_conn *sp;
	struct cli_conn *cp;

	while (sockfd) {
		FD_ZERO(&rd);

		// Reset all file descriptors and find the largest. 
		maxfd = sockfd;
		FD_SET(sockfd, &rd);
		for (sp = irc; sp; sp = sp->next) {
			if (sp->fd > maxfd)
				maxfd = sp->fd;
			FD_SET(sp->fd, &rd);
		}

		for (cp = cli; cp; cp = cp->next) {
			if (cp->fd > maxfd)
				maxfd = cp->fd;
			FD_SET(cp->fd);
		}

		tv.tv_sec = 120;
		tv.tv_usec = 0;
		select(maxfd + 1, &rd, 0, 0, &tv);

		if (rv < 0) {
		} else if (rv == 0) {
		}

		// Check for any new client connections.
		if (FD_ISSET(sockfd, &rd)) {
			struct client *tmp;
			tmp = malloc(sizeof(*tmp));
			tmp->fd = accept(tmp->sockfd,
					(struct sockaddr *)&tmp->addr,
					&tmp->addrlen);
			tmp->next = cli;
			cli = tmp;
		}

		// Check for new server messages
		for (sp = irc; sp; sp = sp->next) {
			if (FD_ISSET(sp->fd, &rd)) {
				if (0 > readline(buf, &buflen, MSG_MAX, tmp->sockfd));
			}
			irc_proc_cmd();
		}

		// Check for new client messages
		for (cp = cli; cp; cp = cp->next) {
			if (FD_ISSET(cp->fd, &rd)) {
				if (0 > readline(buf, &buflen, MSG_MAX, tmp->sockfd)) {
					DPRINT("Failed reading line from client %s\n");
				}
				printf(buf);
				cli_proc_cmd();
			}
		}
	}

	return 0;
}

int main(int argc, char **argv) 
{
	int sockfd = 0;
	const char *port = NULL;
	enum flags flags = 0;

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

	if (0 > tcpopen(&sockfd, "localhost", port, bind)) {
		DPRINT("Failed to bind to port '%s'\n", port);
		exit(EXIT_FAILURE);
	}

	if (0 > listen(sockfd, 4)) {
		DPRINT("Failed to listen on port '%s'\n", port);
		exit(EXIT_FAILURE);
	}

	return run_server(sockfd); 
}
