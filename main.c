/* See LICENSE file for copyright and license details */
#include <arpa/inet.h> 
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <malloc.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
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
#include <assert.h>

#include "sys.h"
#include "config.h"

#ifndef VERSION
#define VERSION "???"
#endif

#ifndef PREFIX
#define PREFIX "/usr/local"
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define CHANNELS_MAX 64
#define MSG_MAX (512 + 1)

/* Logging macros */
#define LOGINFO(...)\
	do { \
		fprintf(stderr, "%s", argv0); \
		fprintf(stderr, ": info: "__VA_ARGS__); \
	} while (0)

#define LOGERROR(...)\
	do { \
		fprintf(stderr, "%s", argv0); \
		fprintf(stderr, ": error: "__VA_ARGS__); \
	} while (0)

#define LOGFATAL(...)\
	do { \
		fprintf(stderr, "%s", argv0); \
		fprintf(stderr, ": fatal: "__VA_ARGS__); \
		exit(EXIT_FAILURE); \
	} while (0)

enum {
	/* Prefix */
	TOK_NICK = 0,
	TOK_USER,
	TOK_HOST,
	/* Commands */
	TOK_CMD,
	TOK_ARG,
	TOK_TEXT,
	TOK_LAST,
};

typedef struct {
	size_t n;
	int fds[CHANNELS_MAX];
	char *paths[CHANNELS_MAX];
} Channels;

static int channels_add(Channels *ch, char const *path);
static int channels_remove(Channels *ch, char const *path);
static void channels_log(char const *path, char const *msg);
void login(const int sockfd, char const *nick, char const *real, char const *host);
void logtime(FILE *fp);
static void poll_fds(int sockfd);
bool proc_server_cmd(char reply[MSG_MAX], Channels *ch, char buf[MSG_MAX]);
bool proc_client_cmd(char reply[MSG_MAX], char const *name, char const *msg);
static int readline(char dest[MSG_MAX], int fd);
size_t tokenize(char const *tok[TOK_LAST], char *buf);

/* Program name */
char const *argv0;

/* Root channel path */
char const *root_path = ".";

/* Display a usage message and then exit */
static void
usage()
{
	printf("usage: %s [-v <version>] [-d <dir>] [-n <nickname>]"
			"[-r <realname>] [-p <port>] [-D] <host>",
			argv0);
	exit(EXIT_FAILURE);
}

/**
 * Display program version and then exit.
 */
static void
version()
{
	fprintf(stderr, "%s version "VERSION"\n", argv0);
	exit(EXIT_FAILURE);
}

/**
 * Opens a fifo file named "in" at the end of the given path.
 *
 * Return: The open fifo file descriptor. -1 if an error opening it occured.
 */
static int
channels_open_fifo(char const *path)
{
	int fd;
	char tmp[PATH_MAX];

	snprintf(tmp, PATH_MAX, "%s/in", path);
	remove(tmp);
	if (0 > (fd = mkfifo(tmp, S_IRWXU))) {
		LOGERROR("Input fifo creation at \"%s\" failed.\n", tmp);
		return -1;
	}
	LOGINFO("Input fifo created at \"%s\" successfully.\n", tmp);
	return open(tmp, O_RDWR | O_NONBLOCK, 0);
}

/**
 * Add a channel to a Channels struct, resize it if necessary.
 */
static int
channels_add(Channels *ch, char const *path)
{
	if (!path)
		return -1;
	ch->paths[ch->n] = malloc(strlen(path) + 1);
	strcpy(ch->paths[ch->n], path);
	if (0 > mkdirpath(ch->paths[ch->n])) {
		LOGERROR("Directory creation for path \"%s\" failed.\n",
				ch->paths[ch->n]);
		return -1;
	}
	if (0 > (ch->fds[ch->n] = channels_open_fifo(ch->paths[ch->n])))
		return -1;
	ch->n += 1;
	return 0;
}

/**
 * Remove a channel from a Channels struct.
 */
static int
channels_remove(Channels *ch, char const *path)
{
	size_t i;

	for (i = 0; i < ch->n; ++i) {
		if (0 == strcmp(ch->paths[i], path)) {
			// Close any open OS resources.
			close(ch->fds[i]);
			free(ch->paths[i]);
			// Move last channel to removed channel index.
			ch->fds[i] = ch->fds[ch->n - 1];
			ch->paths[i] = ch->paths[ch->n - 1];
			ch->n -= 1;
			return 0;
		}
	}
	return -1;
}

/**
 * Log to the output file with the given channel name.
 */
static void
channels_log(char const *path, char const *msg)
{
	FILE *fp;
	char tmp[PATH_MAX];

	snprintf(tmp, PATH_MAX, "%s/out", path);
	if (!(fp = fopen(tmp, "a"))) {
		LOGERROR("Output file \"%s\" failed to open.\n", tmp);
	} else {
		logtime(fp);
		fprintf(fp, " %s\n", msg);
		fclose(fp);
	}
}

static int
rstrip(char *str, char const *chs)
{
	size_t removed = 0;
	size_t len = strlen(str);
	char *begin = str + len - 1;
	char *end = str;

	if (0 == len)
		return 0;
	while (begin != end && strchr(chs, *begin)) {
		++removed;
		--begin;
	}
	str[len - removed] = '\0';
	return removed;
}

static int
readline(char dest[MSG_MAX], int fd)
{
	char ch = '\0';
	size_t i;

	for (i = 0; i < MSG_MAX && '\n' != ch; ++i) {
		if (1 != read(fd, &ch, 1))
			return -1;
		dest[i] = ch;
	}
	dest[i] = '\0';
	/* Remove line delimiters as they make logging difficult, and
	 * aren't always standard, so its easier to add them again later */
	rstrip(dest, "\r\n");
	if (i >= MSG_MAX)
		return 1;
	return 0;
}

/**
 * Initial login to the IRC server.
 */
void
login(const int sockfd, char const *nick, char const *real, char const *host)
{
	assert(NULL != nick);
	assert(NULL != real);
	assert(NULL != host);

	char buf[MSG_MAX];

	snprintf(buf, MSG_MAX, "NICK %s\r\nUSER %s localhost %s : %s\r\n",
			nick, nick, host, real);
	write(sockfd, buf, strlen(buf));
}

/* Log the time to a file */
void
logtime(FILE *fp)
{
	char buf[64];
	time_t t;
	const struct tm *tm;

	t = time(NULL);
	tm = localtime(&t);
	strftime(buf, sizeof(buf), date_format, tm);
	fputs(buf, fp);
}

/**
 * Process and incoming message from the IRC server connection.
 */
bool
proc_server_cmd(char reply[MSG_MAX], Channels *ch, char buf[MSG_MAX])
{
	char tmp[MSG_MAX];
	char const *argv[TOK_LAST] = {0};
	char const *channel;
	size_t argc;

	strncpy(tmp, buf, MSG_MAX);
	argc = tokenize(argv, tmp);
	channel = root_path;
	if (0 == strcmp(argv[TOK_CMD], "PING")) {
		snprintf(reply, MSG_MAX, "PONG %s\r\n", argv[TOK_ARG]);
		return true;
	} else if (0 == strcmp(argv[TOK_CMD], "JOIN")) {
		channels_add(ch, argv[TOK_ARG]);
		snprintf(buf,MSG_MAX, "Joining %s", argv[TOK_ARG]);
	} else if (0 == strcmp(argv[TOK_CMD], "PART")) {
		channels_remove(ch, argv[TOK_ARG]);
		snprintf(buf,MSG_MAX, "Parting from %s", argv[TOK_ARG]);
	} else if (0 == strcmp(argv[TOK_CMD], "PRIVMSG")) {
		channel = argv[TOK_ARG];
		snprintf(buf, MSG_MAX, "<%s> %s\n", argv[TOK_NICK], argv[TOK_TEXT]);
	} else {
		snprintf(buf, MSG_MAX, "<%s> %s\n", argv[TOK_NICK], argv[TOK_TEXT]);
	}
	channels_log(channel, buf);
	return false;
}

/**
 * Process a Ustring into a message to be sent to an IRC channel.
 */
bool
proc_client_cmd(char reply[MSG_MAX], char const *path, char const *msg)
{
	size_t i;
	size_t msg_len = strlen(msg);
	char const *slice;

	if (msg[0] != '/') {
		LOGINFO("Sending message to \"%s\"", path);
		snprintf(reply, MSG_MAX, "PRIVMSG %s :%s\r\n", path, msg);
	} else if (msg[0] == '/' && msg_len > 1) {
		/* Remove leading whitespace. */
		for (i = 0; i < msg_len && msg[i] != ' '; ++i)
			;
		slice = msg + i;
		switch (msg[1]) {
		/* Join a channel */
		case 'j':
			snprintf(reply, MSG_MAX, "JOIN %s\r\n", slice);
			break;
		/* Part from a channel */
		case 'p': 
			snprintf(reply, MSG_MAX, "PART %s\r\n", slice);
			break;
		/* Send a "me" message */
		case 'm':
			snprintf(reply, MSG_MAX, "ME %s\r\n", slice);
			break;
		/* Set status to "away" */
		case 'a':
			snprintf(reply, MSG_MAX, "AWAY %s\r\n", slice);
			break;
		/* Send raw IRC protocol */
		case 'r':
			snprintf(reply, MSG_MAX, "%s\r\n", slice);
			break;
		/* Send a ping */
		case 'P': 
			snprintf(reply, MSG_MAX, "PING %s\r\n", slice);
			break;
		default: 
			LOGERROR("Invalid command entered\n.");
			return false;
		}
	} else {
		return false;
	}
	return true;
}

/**
 * Poll all open file descriptors for incoming messages, and act upon them.
 */
static void
poll_fds(int sockfd)
{
	size_t i;
	fd_set rd;
	int maxfd;
	int rv;
	struct timeval tv;
	Channels ch = {0, {0}, {0}};
	char reply[MSG_MAX];
	char buf[MSG_MAX];

	/* Add the root channel for the connection */
	channels_add(&ch, root_path);
	while (1) {
		FD_ZERO(&rd);
		maxfd = sockfd;
		FD_SET(sockfd, &rd);
		for (i = 0; i < ch.n; ++i) {
			if (maxfd < ch.fds[i])
				maxfd = ch.fds[i];
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
		/* Check for messages from remote host */
		if (FD_ISSET(sockfd, &rd)) {
			do {
				rv = readline(buf, sockfd);
				if (0 > rv)
					LOGFATAL("Unable to read from socket");
				LOGINFO("server: %s\n", buf);
				if (proc_server_cmd(reply, &ch, buf)) {
					write(sockfd, reply, strlen(reply));
				}
			} while (0 < rv);
		}
		for (i = 0; i < ch.n; ++i) {
			if (FD_ISSET(ch.fds[i], &rd)) {
				do {
					rv = readline(buf, ch.fds[i]);
					if (0 > rv)
						LOGFATAL("Unable to read from channel \"%s\"",
							ch.paths[i]);
					LOGINFO("%s: %s\n", ch.paths[i], buf);
					if (proc_client_cmd(reply, ch.paths[i], buf))
						write(sockfd, reply, strlen(reply));
				} while (0 < rv);
			}
		}
	}
}

static char *
m_tok(char **pos, char const *delim)
{
	size_t i, j;
	size_t const n = strlen(delim);
	size_t const len = strlen(*pos);
	char *ret = *pos;

	for (i = 0; i < len; ++i) {
		for (j = i; j < i + n; ++j) {
			if ((*pos)[j] != delim[j-i])
				break;
		}
		if (j - i == n) {
			ret = *pos;
			(*pos)[i]= '\0';
			*pos += i + n;
			break;
		}
	}
	return ret;
}

/**
 * Tokenize an IRC message into parts.
 *
 * [ ':' prefix SPACE ] command params* "\r\n"
 */
size_t
tokenize(char const *tok[TOK_LAST], char *buf)
{
	size_t i;
	size_t n;
	char *tmp;
	char *saveptr = buf;

	for (n = (saveptr[0] == ':' ? TOK_NICK : TOK_CMD); n < TOK_LAST; ++n) {
		switch (n) {
		case TOK_NICK:
			/* Ignore the leading ':' character */
			saveptr += 1;
			tok[n] = m_tok(&saveptr, " ");
			n = TOK_CMD - 1;
			break;
		case TOK_ARG:
			/* Strip the whitespace */
			tmp = m_tok(&saveptr, ":");
			rstrip(tmp, " \r\n\t");
			/* Save the token */
			tok[n] = tmp;
			/* Strip the whitespace */
			for (i = strlen(saveptr) - 1; i == 0; --i)
				if (isspace(*saveptr))
					++saveptr;
			break;
		case TOK_TEXT:
			/* Grab the remainder of the text */
			tok[n] = saveptr;
			break;
		default:
			tok[n] = m_tok(&saveptr, " ");
			break;
		}
	}
	return 0;
}

int
main(int argc, char **argv) 
{
	/* Argument parsing */
	int i;
	char const *opt_arg;
	/* IRC connection context. */
	int sockfd;
	char prefix[PATH_MAX];
	/* Arguments */
	char const *host = NULL;
	char const *port = default_port;
	char const *directory = default_directory;
	char nickname[32];
	char realname[32];

	strncpy(nickname, default_nickname, sizeof(nickname));
	strncpy(realname, default_realname, sizeof(realname));
	/* Set the program name */
	argv0 = *argv;
	if (argc < 2)
		usage();
	/* Argument parsing */
	for (i = 1; i < argc; ++i) {
		if ('-' == argv[i][0]) {
			argv[i] += 1;
			while ('\0' != argv[i][0]) {
				argv[i] += 1;
				if ('\0' == argv[i][0]) {
					opt_arg = argv[i + 1];
				} else {
					opt_arg = argv[0];
				}
				switch (argv[i][-1]) {
				case 'v':
					version();
					break;
				case 'd':
					if (!opt_arg) usage();
					directory = opt_arg;
					break;
				case 'n':
					if (!opt_arg) usage();
					strncpy(nickname, opt_arg, sizeof(nickname));
					break;
				case 'r':
					if (!opt_arg) usage();
					strncpy(realname, opt_arg, sizeof(realname));
					break;
				case 'p':
					if (!opt_arg) usage();
					port = opt_arg;
					break;
				case 'D':
					daemonize();
					break;
				}
			}
		} else {
			host = argv[i];
		}
	}
	if (!host)
		usage();
	/* Create the prefix from the directoryAppend hostname to prefix and slice it */
	snprintf(prefix, PATH_MAX, "%s/%s", directory, host);
	if (0 > mkdirpath(prefix))
		LOGFATAL("Unable to create prefix directory\n");
	LOGINFO("Root directory created.\n");
	/* Change to the prefix directory */
	if (0 > chdir(prefix))
		return EXIT_FAILURE;
	/* Open the irc-server connection */
	if (0 > tcpopen(&sockfd, host, port, connect))
		LOGFATAL("Could not connect to host \"%s\" on port \"%s\".\n",
				host, port);
	LOGINFO("Successfully initialized.\n");
	login(sockfd, nickname, realname, host);
	poll_fds(sockfd);
	return EXIT_SUCCESS;
}
