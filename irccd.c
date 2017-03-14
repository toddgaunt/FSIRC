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

#define DEBUG 1
#if DEBUG
#define LOG_ERR(...) fprintf(stderr, PRGM_NAME": "__VA_ARGS__)
#else
#define LOG_ERR(...)
#endif

#define EPRINT(x) perror(PRGM_NAME": "x)

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
	printf(portstr);

	// Get the host ip address.
	if (getaddrinfo(host, portstr, &hints, &res)) {
		return -1;
	}

	printf("hi\n");
	exit(1);
	
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
irc_readline(struct oystr *recvln, struct irc_conn *irc)
{
	char c;
	do {
		if (read(irc->sockfd, &c, sizeof(c)) != sizeof(c))
			return -1;

		if (0 > oystr_append(recvln, &c, 1))
			return -1;

	} while (c != '\n' && MSG_MAX >= recvln->len);
	// Removes the \r\n found at the end of irc messages.
	oystr_trunc(recvln, 2);

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
			goto cleanup_path;
		}
	}

	// Open up the channel input file.
	oystr_append(&path, "/in", 3);

	if (0 > mkfifo(path.buf, S_IRWXU)) {
		if (EEXIST != errno) {
			goto cleanup_path;
		}
	}

	if (!(chan->fd = open(path.buf, O_RDONLY | O_NONBLOCK)))
		goto cleanup_path;

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

	//TODO delete all files in directory first
	rmdir(garbage->name.buf);

	close(garbage->fd);
	oystr_deinit(&garbage->name);
	free(garbage);
}

static int
channel_write(const char *name, size_t namelen)
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

	return 0;

cleanup_path:
	oystr_deinit(&path);
except:
	return -1;
}

/**
 * Login to an irc server.
 */
static int
irc_login(struct oystr *msg, struct irc_conn *irc)
{
	if (0 > oystr_snprintf(msg, MSG_MAX+1, "USER %s 8 &:%s NICK %s\r\n",
			irc->nick, irc->realname, irc->nick))
		return -1;

	if (0 > write(irc->sockfd, msg->buf, msg->len))
		return -1;

	return 0;
}

static bool
run_server(struct irc_conn *irc)
{
	int maxfd;
	int ret;
	fd_set fds;
	struct timeval tv;
	// A Single Message bufer is used for communication, this way needless
	// extra memory allocations can be avoided.
	struct oystr msg;
	// Joined Channel linked list.
	struct channel *chan_head;
	// Current active channel.
	struct channel *chan_active;

	if (!(irc->sockfd = tcpopen(irc->host.buf, irc->port)))
		LOG_ERR("Unable to connect to %s\n", irc->host.buf);

	// The root channel. Represents the main server channel.
	if (!(chan_head = channel_open("./", 2)))
		LOG_ERR("Unable to create root channel.");

	oystr_init(&msg);
	if (!irc_login(&msg, irc))
		LOG_ERR("Unable to login to irc server.");

	while(irc->sockfd) {
		FD_ZERO(&fds);

		// Reset all file descriptors and find the largest. 
		maxfd = irc->sockfd;
		FD_SET(irc->sockfd, &fds);
		for (struct channel *chan=chan_head; chan; chan=chan->next) {
			if (chan->fd > maxfd)
				maxfd = chan->fd;
			FD_SET(chan->fd, &fds);
		}

		tv.tv_sec = 120;
		tv.tv_usec = 0;
		ret = select(maxfd + 1, &fds, 0, 0, &tv);

		if (0 > ret) {
			LOG_ERR("select() failed.\n");
		}

		// Check if the irc server has new messages.
		if (FD_ISSET(irc->sockfd, &fds)) {
			//parse_irc_input(&msg, irc, chan_head);
			//channel_write(irc->out);
		}

		// Check if any channel's input FIFO has new messages.
		for (struct channel *chan=chan_head; chan; chan=chan->next) {
			if (FD_ISSET(chan->fd, &fds)) {
				//parse_client_input(&msg, chan->fd);
				//irc_send(irc, &msg);
			}
		}
	}

	// Free all channels
	while (chan_head) {
		channel_close(chan_head);
	}

	return true;
}

int main(int argc, char **argv) 
{
	bool running;
	char *path;
	enum flags flags;
	struct irc_conn irc;

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
	if (!irc.nick.buf)
		oystr_assign(&irc.nick, "user", sizeof("user"));

	if (!irc.realname.buf)
		oystr_assign(&irc.realname, "user", sizeof("user"));

	if (!irc.port)
		irc.port = 6667;

	// Change the directory only if specified to.
	if (path) {
		if (0 > mkdir(path, S_IRWXU)) {
			if (EEXIST != errno) {
				EPRINT("Cannot create directory");
			}
		}

		if (0 > chdir(path)) {
			EPRINT("Cannot change directory");
		}
	}

	// Daemonize last so that any argument errors can be printed to caller.
	if (FLAG_DAEMON == (flags & FLAG_DAEMON))
		daemonize(1, 0);

	running = true;
	while(running) {
		running = run_server(&irc);
	}

	oystr_deinit(&irc.nick);
	oystr_deinit(&irc.realname);
	oystr_deinit(&irc.host);

	return EXIT_SUCCESS;
}
