/* See LICENSE file for copyright and license details */
#include <arpa/inet.h> 
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <malloc.h>
#include <netdb.h>
#include <ctype.h>
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
#include <libstx.h>

#include "arg.h"
#include "yorha_config.h"

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

#define MSG_MAX 512

#define LOGINFO(...)\
	do {logtime(stderr);\
	fprintf(stderr, " "PRGM_NAME": info: "__VA_ARGS__);} while (0)

#define LOGERROR(...)\
	do {logtime(stderr);\
	fprintf(stderr, " "PRGM_NAME": error: "__VA_ARGS__);} while (0)

#define LOGFATAL(...)\
	do {logtime(stderr);\
	fprintf(stderr, " "PRGM_NAME": fatal: "__VA_ARGS__);\
	exit(EXIT_FAILURE);} while (0)

struct channels {
	size_t size;
	size_t len;
	int *fds;
	stx *names;
};

static void
version()
{
	fprintf(stderr, PRGM_NAME" version "VERSION"\n");
	exit(-1);
}

/**
 * Log the current time in month-day-year format to the given stream.
 */
static void
logtime(FILE *fp)
{
	char buf[16];
	time_t t;
	const struct tm *tm;

	t = time(NULL);
	tm = localtime(&t);
	strftime(buf, sizeof(buf), "%m %d %T", tm);
	fprintf(fp, buf);
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
 * Daemonize a process by forking it twice, redirecting standard streams
 * to PATH_NULL, and changing directory to "/".
 */
static void
daemonize()
{
	// Fork once to go into the background.
	if (0 > daemonize_fork())
		exit(-1);

	if (0 > setsid())
		exit(-1);

	// Fork once more to ensure no TTY can be required.
	if (0 > daemonize_fork())
		exit(-1);

	int fd;
	if (!(fd = open(PATH_NULL, O_RDWR)))
		exit(-1);

	// Close standard streams.
	dup2(fd, STDIN_FILENO);
	dup2(fd, STDOUT_FILENO);
	dup2(fd, STDERR_FILENO);

	// Only close the fd if it isn't one of the std streams.
	if (fd > 2)
		close(fd);
}

/**
 * Open a tcp connection and save the file descriptor to "sockfd". The last
 * parameter is either connect() or bind() funtion depending on the desired
 * behavior.
 */
int
tcpopen(int *sockfd, const spx host, const spx port, 
		int (*opensocket)(int, const struct sockaddr *, socklen_t))
{
	struct addrinfo hints;
	struct addrinfo *res;
	struct addrinfo *ptr;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_CANONNAME;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	// Create null-terminated strings for legacy functions.
	char tmphost[host.len + 1];
	char tmpport[port.len + 1];
	memcpy(tmphost, host.mem, host.len);
	memcpy(tmpport, port.mem, port.len);
	tmphost[host.len] = '\0';
	tmpport[port.len] = '\0';

	// Get the ip address of 'host'.
	if (getaddrinfo(tmphost, tmpport, &hints, &res)) {
		return -1;
	}
	
	// Attempt to connect to any available address.
	for (ptr=res; ptr; ptr=ptr->ai_next) {
		if (0 > (*sockfd = socket(ptr->ai_family, ptr->ai_socktype,
				ptr->ai_protocol)))
			continue;

		if (0 > opensocket(*sockfd, ptr->ai_addr, ptr->ai_addrlen))
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


/**
 * Recursively make directories given a fullpath.
 *
 * Return: 0 if directory path is fully created. -1 if mkdir fails.
 */
int
mkdirpath(const spx path)
{
	char tmp[path.len + 1];
	memcpy(tmp, path.mem, path.len);
	tmp[path.len] = '\0';

	if('/' == tmp[path.len - 1])
		tmp[path.len - 1] = '\0';

	for (size_t i=1; i < path.len; ++i) {
		if('/' == tmp[i]) {
			tmp[i] = '\0';
			if (0 > mkdir(tmp, S_IRWXU))
				if (EEXIST != errno)
					return -1;
			tmp[i] = '/';
		}
	}

	if (0 > mkdir(tmp, S_IRWXU))
		if (EEXIST != errno)
			return -1;

	return 0;
}

/**
 * Opens a fifo file named "in" at the end of the given path.
 *
 * Return: The open fifo file descriptor. -1 if an error opening it occured.
 */
static int
channel_open_fifo(const spx path)
{
	int fd = -1;
	char tmp[path.len + sizeof("/in")];

	memcpy(tmp, path.mem, path.len);
	strcpy(tmp + path.len, "/in");

	remove(tmp);
	if (0 > (fd = mkfifo(tmp, S_IRWXU))) {
		LOGERROR("Input fifo creation at \"%s\" failed.\n", tmp);
		return -1;
	}

	LOGINFO("Input fifo created at \"%s\" successfully.\n", tmp);
	return open(tmp, O_RDWR | O_NONBLOCK, 0);
}

/**
 * Allocate and initialize memory for a struct channels.
 */
int
channels_init(struct channels *ch, size_t init_size)
{
	if (!(ch->fds = calloc(init_size, sizeof(*ch->fds))))
		goto except;

	if (!(ch->names = calloc(init_size, sizeof(*ch->names))))
		goto cleanup_ch_fds;

	ch->size = init_size;
	ch->len = 0;
	return 0;

cleanup_ch_fds:
	free(ch->fds);
except:
	return -1;
}

/**
 * Add a channel to a struct channels, and resize the lists if necessary.
 */
int
channels_add(struct channels *ch, const spx name)
{
	if (ch->len >= ch->size) {
		// Find the nearest power of two for reallocation.
		size_t next_size = 2;
		if (ch->size >= SIZE_MAX / 2) {
			next_size = SIZE_MAX;
		} else {
			while (next_size < ch->size)
				next_size <<= 1;
		}

		if (!realloc(ch->fds, sizeof(*ch->fds) * next_size))
			return -1;

		if (!realloc(ch->names, sizeof(*ch->names) * next_size))
			return -1;

		ch->size = next_size;
	}

	stx *sp = ch->names + ch->len;

	if (0 > stxensuresize(sp, name.len)) {
		LOGERROR("Allocation of path buffer for channel \"%.*s\" failed.\n",
				(int)name.len, name.mem);
		return -1;
	}

	stxcpy_str(sp, name.mem);

	if (0 > mkdirpath(stxref(sp))) {
		LOGERROR("Directory creation for path \"%.*s\" failed.\n",
				(int)sp->len, sp->mem);
		return -1;
	}


	if (0 > (ch->fds[ch->len] = channel_open_fifo(stxref(sp)))) {
		return -1;
	}

	++ch->len;

	return 0;
}

/**
 * Remove a channel from a struct channels.
 */
static void
channels_remove(struct channels *ch, size_t index)
{
	// Close any open OS resources.
	close(ch->fds[index]);

	// The last channel in the list replaces the channel to be removed.
	ch->fds[index] = ch->fds[ch->len - 1];
	ch->names[index] = ch->names[ch->len - 1];

	ch->len--;
}

/**
 * Free all memory used by a struct channels.
 */
static void
channels_del(struct channels *ch)
{
	// Close any open OS resources.
	for (size_t i=0; i<ch->len; ++i)
		close(ch->fds[i]);

	free(ch->fds);
	free(ch->names);
}

static void
channels_log(const spx name, const spx msg)
{
	FILE *fp;
	char tmp[name.len + sizeof("/out")];

	memcpy(tmp, name.mem, name.len);
	strcpy(tmp + name.len, "/out");

	if (!(fp = fopen(tmp, "a"))) {
		LOGERROR("Output file \"%s\" failed to open.\n", tmp);
		return;
	}

	logtime(fp);
	fprintf(fp, "\t%.*s", (int)msg.len, msg.mem);

	fclose(fp);
}

static int
send_msg(int sockfd, const stx *sp)
{
	return write(sockfd, sp->mem, sp->len);
}

static int
readline(stx *sp, int fd)
{
	char ch;

	sp->len = 0;
	do {
		if (1 != read(fd, &ch, 1))
			return -1;

		sp->mem[sp->len] = ch;
		sp->len += 1;
	} while (ch != '\n' && sp->len <= sp->size);

	// Removes and line delimiters
	stxrstrip(sp, "\r\n", 2);

	return 0;
}

/**
 */
void
fmt_login(stx *buf, const spx host, const spx nick, const spx realname)
{
	buf->len = snprintf(buf->mem, buf->size,
			"NICK %.*s\r\nUSER %.*s localhost %.*s :%.*s\r\n",
			(int)nick.len, nick.mem,
			(int)nick.len, nick.mem,
			(int)host.len, host.mem, 
			(int)realname.len, realname.mem);
}

/**
void
proc_irc_cmd(int sockfd, struct channels *ch, const stx *buf)
{
}
 */

int
tokenize(spx *toks, const stx *buf)
{
	return 0;
}

/**
 */
void
proc_channel_cmd(int sockfd, const spx name, const spx buf)
{
	if (buf.mem[0] != '/') {
		LOGINFO("Sending message to \"%.*s\"", (int)name.len, name.mem);
		write(sockfd, "PRIVMSG ", 8);
		write(sockfd, name.mem, name.len);
		write(sockfd, " :", 2);
		write(sockfd, buf.mem, buf.len);
		write(sockfd, "\r\n", 2);
	}

	if (buf.mem[0] == '/' && buf.len > 1) {
		spx slice = stxslice(buf, 2, buf.len);
		switch (buf.mem[1]) {
		// Join a channel.
		case 'j':
			write(sockfd, "JOIN ", 5);
			write(sockfd, slice.mem, slice.len);
			write(sockfd, "\r\n", 2);
			break;
		// Part from a channel.
		case 'p': 
			write(sockfd, "PART", 4);
			write(sockfd, slice.mem, slice.len);
			write(sockfd, "\r\n", 2);
			break;
		// Send a "me" message.
		case 'm':
			write(sockfd, "ME", 2);
			write(sockfd, slice.mem, slice.len);
			write(sockfd, "\r\n", 2);
			break;
		// Set status to "away".
		case 'a':
			write(sockfd, "AWAY", 4);
			write(sockfd, slice.mem, slice.len);
			write(sockfd, "\r\n", 2);
			break;
		// Send raw IRC protocol.
		case 'r':
			write(sockfd, slice.mem, slice.len);
			break;
		default: 
			LOGERROR("Invalid command entered\n.");
			break;
		}
	}
}

/**
 * Function used as an argument parsing callback function.
 */
void
assign_stx(struct stx *sp, size_t n, const char *arg) {
	for (size_t i=0; i<n; ++i) {
		if (0 < stxensuresize(sp + i, strlen(arg) + 1)) {
			LOGFATAL("Allocation of argument string failed.\n");
		}

		stxterm(stxcpy_str(sp + i, arg));
	}
}

int
main(int argc, char **argv) 
{
	// Message buffer
	stx buf = {0};
	// Channel connection data.
	struct channels ch = {0};

	// Irc server connection info.
	int sockfd = 0;
	stx prefix = {0};
	spx host = {0};
	stx port = {0};
	stx nickname = {0};
	stx realname = {0};

	struct arg_option opts[7] = {
		{
			'h', "help", opts, sizeof(opts)/sizeof(*opts),
			arg_help, NULL,
			"Show this help message and exit"
		},
		{
			'v', "version", NULL, 0,
			version, NULL,
			"Show the program version and exit"
		},
		{
			'd', "directory", &prefix, 1,
			assign_stx, default_prefix,
			"Specify the runtime [d]irectory to use"
		},
		{
			'n', "nickname", &nickname, 1,
			assign_stx, default_nickname,
			"Specify the [n]ickname to login with"
		},
		{
			'r', "realname", &realname, 1,
			assign_stx, default_realname,
			"Specify the [r]ealname to login with"
		},
		{
			'p', "port", &port, 1,
			assign_stx, default_port,
			"Specify the [p]ort of the remote irc server"
		},
		{
			'D', "daemonize", NULL, 0,
			daemonize, NULL,
			"Allow the program to [d]aemonize"
		},
	};

	argv = arg_sort(argv + 1);
	argv = arg_parse(argv, opts, sizeof(opts)/sizeof(*opts));

	if (!argv[0])
		LOGFATAL("No host argument provided.\n");

	// Append hostname to prefix and slice it.
	{
		size_t index = prefix.len;
		if (0 < stxensuresize(&prefix, index + strlen(argv[0]) + 2))
			return EXIT_FAILURE;

		stxterm(stxapp_str(stxapp_str(&prefix, "/"), argv[0]));
		host = stxslice(stxref(&prefix), index + 1, prefix.len);
	}

	// Change to the supplied prefix.
	if (0 > mkdirpath(stxref(&prefix))) {
		LOGFATAL("Creation of prefix directory \"%s\" failed.\n",
				prefix.mem);
	}

	if (0 > chdir(prefix.mem)) {
		exit(EXIT_FAILURE);
		LOGERROR("Could not change directory: ");
		perror("");
		return EXIT_FAILURE;
	}

	// Open the irc-server connection.
	if (0 > tcpopen(&sockfd, host, stxref(&port), connect))
		LOGFATAL("Failed to connect to host \"%s\" on port \"%s\".\n",
				host.mem, port.mem);

	LOGINFO("Successfully initialized.\n");

	// Initialize the message buffer and login to the remote host.
	stxalloc(&buf, MSG_MAX);
	fmt_login(&buf, host, stxref(&nickname), stxref(&realname));
	send_msg(sockfd, &buf);

	// Initialize the channel list.
	if (0 > channels_init(&ch, 2))
		LOGFATAL("Allocation of channels list failed.\n");

	// Root directory channel
	channels_add(&ch, (spx){1, "."});

	while (sockfd) {
		int maxfd;
		fd_set rd;
		struct timeval tv;
		int rv;

		FD_ZERO(&rd);
		maxfd = sockfd;
		FD_SET(sockfd, &rd);
		for (size_t i=0; i<ch.len; ++i) {
			if (maxfd < ch.fds[i]) {
				maxfd = ch.fds[i];
			}
			FD_SET(ch.fds[i], &rd);
		}

		tv.tv_sec = ping_timeout;
		tv.tv_usec = 0;
		rv = select(maxfd + 1, &rd, 0, 0, &tv);

		if (rv < 0) {
			LOGFATAL("Error running select().\n");
		} else if (rv == 0) {
			//TODO(todd): handle timeout with ping message.
		}

		// Check for messages from remote host.
		if (FD_ISSET(sockfd, &rd)) {
			if (0 > readline(&buf, sockfd)) {
				LOGFATAL("Unable to read from %s: ", host.mem);
			} else {
				fprintf(stderr, "%.*s\n", (int)buf.len, buf.mem);
				//char tokens[7] = parse_irc_cmd(&buf);
				//proc_irc_cmd(sockfd, &ch, &buf);
			}
		}

		for (size_t i=0; i<ch.len; ++i) {
			if (FD_ISSET(ch.fds[i], &rd)) {
				if (0 > readline(&buf, ch.fds[i])) {
					LOGFATAL("Unable to read from channel \"%.*s\"",
						(int)ch.names[i].len,
						ch.names[i].mem);
				} else {
					proc_channel_cmd(sockfd,
							stxref(ch.names + i),
							stxref(&buf));
				}
			}
		}
	}

	channels_del(&ch);
	stxfree(&buf);

	return 0;
}
