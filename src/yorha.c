/* See LICENSE file for copyright and license details */
#include <arpa/inet.h> 
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <malloc.h>
#include <netdb.h>
#include <oystr.h>
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

#define PING_TIMEOUT 300

#define MSG_MAX 512

#define DPRINT(...) fprintf(stderr, PRGM_NAME": "__VA_ARGS__)
#define DFLUSH() fflush(stderr)

#define EPRINT(x) perror(PRGM_NAME": "x)

// Array indices for tokenize_irc_output().
enum {TOK_NICKSRV, TOK_USER, TOK_CMD, TOK_CHAN, TOK_ARG, TOK_TEXT, TOK_LAST};

// Bitmask flags for program arguments.
enum flags {
	FLAG_DAEMON = 1 << 0,
};

struct channel {
	int fd;
	struct oystr name;
	struct channel *next;
};

// Structure containing irc server connection information.
struct irc_conn {
	int sockfd;
	int port;
	struct oystr nick;
	struct oystr realname;
	struct oystr host;
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

static int
tcpopen(char *host, int port)
{
	int sockfd;
	struct addrinfo hints;
	struct addrinfo *res;
	struct addrinfo *ptr;
	char portstr[16];

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	snprintf(portstr, sizeof(portstr), "%d", port);

	// Get the host ip address.
	if (getaddrinfo(host, portstr, &hints, &res)) {
		return -1;
	}
	
	// Attempt to connect to any available address.
	for (ptr=res; ptr; ptr=ptr->ai_next) {
		if (0 > (sockfd = socket(ptr->ai_family, ptr->ai_socktype,
				ptr->ai_protocol)))
			continue;

		if (0 > connect(sockfd, ptr->ai_addr, ptr->ai_addrlen))
			continue;

		// Successful connection.
		break;
	}

	// No connection was made
	if (!ptr)
		sockfd = -1;

	if (res)
		freeaddrinfo(res);

	return sockfd;
}

static int
readline(struct oystr *recvln, int sockfd)
{
	char c;
	do {
		if (read(sockfd, &c, sizeof(c)) != sizeof(c))
			return -1;

		if (0 > oystr_append(recvln, &c, 1))
			return -1;

	} while (c != '\n' && MSG_MAX >= recvln->len);

	return 0;
}

/**
 * Create a new channel struct, and open a directory containing all input/output
 * files used to by the channel to communicate.
 *
 * The path is provied by the 'name' parameter. 'len' is the length of 'name'.
 */
static struct channel*
channel_open(char *name, size_t namelen)
{
	struct channel *chan;
	struct oystr path;

	if (namelen > PATH_MAX) {
		DPRINT("Pathname '%s' too long", name);
		goto except;
	}

	if (!(chan = malloc(sizeof(*chan))))
		goto except;

	memset(chan, 0, sizeof(*chan));

	if (0 > oystr_assign(&chan->name, name, namelen))
		goto cleanup_chan;

	// Create the channel directory.
	oystr_init(&path);
	if (0 > oystr_ensure_size(&path, namelen + 3)) {
		goto cleanup_chan_name;
	}

	oystr_assign(&path, name, namelen);
	if (mkdir(path.buf, S_IRWXU)) {
		if (EEXIST != errno) {
			EPRINT("Cannot create directory");
			goto cleanup_path;
		}
	}

	// Open up the channel input file.
	oystr_append(&path, "/in", 3);

	if (0 > mkfifo(path.buf, S_IRWXU)) {
		if (EEXIST != errno) {
			EPRINT("Cannot create FIFO file");
			goto cleanup_path;
		}
	}

	if (!(chan->fd = open(path.buf, O_RDONLY | O_NONBLOCK)))
		goto cleanup_path;

	oystr_deinit(&path);

	return chan;

cleanup_path:
	oystr_deinit(&path);
cleanup_chan_name:
	oystr_deinit(&chan->name);
cleanup_chan:
	free(chan);
except:
	return NULL;
}

/**
 * Close a channel struct and cleanup it's corresponding directory on the
 * filesystem.
 */
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

/**
 * Cleanup all channels and exit.
 */
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
channel_write(const char *name, size_t namelen, struct oystr *msg)
{
	FILE *fp;
	struct oystr path;

	oystr_init(&path);
	if (0 > oystr_ensure_size(&path, namelen + 4))
		goto except;

	oystr_append(&path, name, namelen);
	oystr_append(&path, "/out", 4);

	if (!(fp = fopen(path.buf, "a")))
		goto cleanup_path;

	fwrite(msg->buf, msg->len, 1, fp);

	oystr_deinit(&path);
	fclose(fp);

	return 0;

cleanup_path:
	oystr_deinit(&path);
except:
	return -1;
}

static int
tokenize_irc_output(struct oystr *tok, size_t toksize, struct oystr *msg)
{
}

static int
proc_irc_cmd(struct oystr **tok, size_t toksize)
{
}

/**
 * Login to an irc server.
 */
static int
irc_login(struct oystr *msg, struct irc_conn *irc)
{
	if (0 > oystr_snprintf(msg, MSG_MAX, "NICK %s\r\nUSER %s localhost %s :%s\r\n",
			irc->nick.buf,
			irc->nick.buf,
			irc->host.buf,
			irc->realname.buf))
		return -1;

	if (0 > write(irc->sockfd, msg->buf, msg->len))
		return -1;

	return 0;
}

int main(int argc, char **argv) 
{
	bool running;
	char *path;
	enum flags flags;
	struct irc_conn irc;
	// Singular Message buffer used for sending and receiving messages
	// from the irc server.
	struct oystr msg;

	path = NULL;
	flags = 0;
	memset(&irc, 0, sizeof(irc));

	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
			case 'h': 
				usage(); 
				break;
			case 'p': 
				++i;
				irc.port = strtoul(argv[i], NULL, 10);
				break;
			case 'n': 
				++i;
				oystr_assign_cstr(&irc.nick, argv[i]); 
				break;
			case 'r':
				++i;
				oystr_assign_cstr(&irc.realname, argv[i]);
				break;
			case 'd':
				++i;
				path = argv[i];
				break;
			case 'D': 
				flags |= FLAG_DAEMON; 
				break;
			default: 
				usage();
			}
		} else {
			oystr_assign_cstr(&irc.host, argv[i]);
		}
	}

	// An initial host must be specified.
	if (!irc.host.buf)
		usage();

	// Assign default argument values if none were given.
	if (!irc.nick.buf) {
		if (0 > oystr_assign(&irc.nick, "user", sizeof("user"))) {
			DPRINT("Failed to assign requested nickname, probably too long.\n");
			exit(EXIT_FAILURE);
		}
	}

	if (!irc.realname.buf) {
		if (0 > oystr_assign(&irc.realname, "user", sizeof("user"))) {
			DPRINT("Failed to assign requested realname, probably too long.\n");
			exit(EXIT_FAILURE);
		}
	}

	if (!irc.port)
		irc.port = 6667;

	// Change the directory only if specified to.
	if (path) {
		if (0 > mkdir(path, S_IRWXU)) {
			if (EEXIST != errno) {
				EPRINT("Cannot create directory");
				exit(EXIT_FAILURE);
			}
		}

		if (0 > chdir(path)) {
			EPRINT("Cannot change directory");
			exit(EXIT_FAILURE);
		}
	}

	// Daemonize last so that any argument errors can be printed to caller.
	if (FLAG_DAEMON == (flags & FLAG_DAEMON))
		daemonize(1, 0);

	// Set the message buffer to the maximum irc message size.
	oystr_init(&msg);
	oystr_ensure_size(&msg, MSG_MAX);

	running = true;
	while(running) {
		int maxfd;
		int ret;
		fd_set fds;
		struct timeval tv;
		// Joined Channel linked list.
		struct channel *chan_head;
		// Current active channel.
		struct channel *chan_active;

		if (!(irc.sockfd = tcpopen(irc.host.buf, irc.port))) {
			DPRINT("Unable to connect to host %s\n", irc.host.buf);
			channel_cleanup(chan_head);
		}

		// The root channel. Represents the main server channel.
		if (!(chan_head = channel_open("./", 2))) {
			DPRINT("Unable to create root channel.\n");
			channel_cleanup(chan_head);
		}

		if (irc_login(&msg, &irc)) {
			DPRINT("Unable to login to irc server.\n");
			channel_cleanup(chan_head);
		}

		while(irc.sockfd) {
			FD_ZERO(&fds);

			// Reset all file descriptors and find the largest. 
			maxfd = irc.sockfd;
			FD_SET(irc.sockfd, &fds);
			for (struct channel *chan=chan_head; chan; chan=chan->next) {
				if (chan->fd > maxfd)
					maxfd = chan->fd;
				FD_SET(chan->fd, &fds);
			}

			tv.tv_sec = 120;
			tv.tv_usec = 0;
			ret = select(maxfd + 1, &fds, 0, 0, &tv);

			if (0 > ret) {
				DPRINT("select() failed.\n");
			}

			// Check if the irc server has new messages.
			if (FD_ISSET(irc.sockfd, &fds)) {
				struct oystr tokens[TOK_LAST];
				readline(&msg, irc.sockfd);
				printf(msg.buf);
				/*
				tokenize_irc_output(tokens, sizeof(tokens),
						&msg);
				proc_irc_cmd(tokens, sizeof(tokens));
				channel_write(tokens[TOK_CHAN].buf,
						tokens[TOK_CHAN].len, &msg);
				*/
				channel_write("./", 2, &msg);
			}

			// Check if any channel's input FIFO has new messages.
			for (struct channel *chan=chan_head; chan; chan=chan->next) {
				if (FD_ISSET(chan->fd, &fds)) {
					readline(&msg, chan->fd);
					/*
					proc_client_cmd(&msg);
					*/
				}
			}
		}

		// Free the message buffer
		oystr_deinit(&msg);
	}

	oystr_deinit(&irc.nick);
	oystr_deinit(&irc.realname);
	oystr_deinit(&irc.host);

	return EXIT_SUCCESS;
}
