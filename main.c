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

#include "sys.h"
#include "arg.h"
#include "config.h"

#ifndef PRGM_NAME
#define PRGM_NAME "NULL"
#endif

#ifndef VERSION
#define VERSION "NULL"
#endif

#ifndef PREFIX
#define PREFIX "/usr/local"
#endif

#define CHANNELS_MAX 64
#define MSG_MAX 512

/* Logging macros */
#define LOGINFO(...)\
	do { \
		logtime(stderr); \
		fprintf(stderr, " "PRGM_NAME": info: "__VA_ARGS__); \
	} while (0)

#define LOGERROR(...)\
	do { \
		logtime(stderr); \
		fprintf(stderr, " "PRGM_NAME": error: "__VA_ARGS__); \
	} while (0)

#define LOGFATAL(...)\
	do { \
		logtime(stderr); \
		fprintf(stderr, " "PRGM_NAME": fatal: "__VA_ARGS__); \
		exit(EXIT_FAILURE); \
	} while (0)

enum {
	TOK_PREFIX = 0,
	TOK_CMD,
	TOK_ARG,
	TOK_TEXT,
	TOK_LAST
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
bool proc_server_cmd(char reply[MSG_MAX], Channels *ch, char const *msg);
bool proc_client_cmd(char reply[MSG_MAX], char const *name, char const *msg);
static int readline(char dest[MSG_MAX], int fd);
void tokenize(char const *tok[TOK_LAST], char *buf);

/* Root channel path */
char const *root_path = ".";

/* Display a usage message and then exit */
static void
usage(size_t optc, ArgOption const *optv)
{
	arg_print_usage(optc, optv);
	fprintf(stderr, " host\n");
	exit(EXIT_FAILURE);
}

/* Display a help message and then exit */
static void
help(size_t optc, ArgOption const *optv)
{
	arg_print_usage(optc, optv);
	fprintf(stderr, " host\n");
	arg_print_help(optc, optv);
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
	int fd = -1;
	int len = strlen(path);
	char tmp[len + sizeof("/in")];

	snprintf(tmp, sizeof(tmp), "%s/in", path);
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
	int len = strlen(path);
	char tmp[len + sizeof("/out")];

	snprintf(tmp, sizeof(tmp), "%s/out", msg);
	if (!(fp = fopen(tmp, "a"))) {
		LOGERROR("Output file \"%s\" failed to open.\n", tmp);
	} else {
		logtime(fp);
		fprintf(fp, " %s\n", msg);
		fclose(fp);
	}
}

/**
 * Display program version and then exit.
 */
static void
version()
{
	fprintf(stderr, PRGM_NAME" version "VERSION"\n");
	exit(EXIT_FAILURE);
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
	while (begin != end && memchr(chs, *begin, len)) {
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
	/* Remove line delimiters as they make logging difficult */
	rstrip(dest, "\r\n");
	return 0;
}

/**
 * Initial login to the IRC server.
 */
void
login(const int sockfd, char const *nick, char const *real, char const *host)
{

	char buf[MSG_MAX];

	snprintf(buf, MSG_MAX, "NICK %s\r\nUSER %s localhost %s : %s\r\n",
			nick, nick, host, real);
	write(sockfd, buf, strlen(buf));
}

/* Log the time to a file */
void
logtime(FILE *fp)
{
	char buf[16];
	time_t t;
	const struct tm *tm;

	t = time(NULL);
	tm = localtime(&t);
	strftime(buf, sizeof(buf), "%m %d %T", tm);
	fputs(buf, fp);
}

/**
 * Process and incoming message from the IRC server connection.
 */
bool
proc_server_cmd(char reply[MSG_MAX], Channels *ch, char const *msg)
{
	char tmp[MSG_MAX];
	char const *tok[TOK_LAST];

	strncpy(tmp, msg, MSG_MAX);
	tokenize(tok, tmp);
	if (0 == strcmp(tok[TOK_CMD], "PING")) {
		snprintf(reply, MSG_MAX, "PONG %s\r\n", tok[TOK_ARG]);
		return true;
	} else if (0 == strcmp(tok[TOK_CMD], "PART")) {
		channels_remove(ch, tok[TOK_ARG]);
	} else if (0 == strcmp(tok[TOK_CMD], "JOIN")) {
		channels_add(ch, tok[TOK_ARG]);
	} else if (0 == strcmp(tok[TOK_CMD], "PRIVMSG")) {
		channels_log(tok[TOK_ARG], msg);
	} else {
		channels_log(root_path, msg);
	}
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
	Channels ch = {0};
	char reply[MSG_MAX + 1];
	char buf[MSG_MAX + 1];

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
			if (0 > readline(buf, sockfd)) {
				LOGFATAL("Unable to read from socket");
			} else {
				/* TMP */
				fprintf(stderr, "%s\n", buf);
				/* ENDTMP */
				if (proc_server_cmd(reply, &ch, buf))
					write(sockfd, reply, strlen(reply));
			}
		}
		for (i = 0; i < ch.n; ++i) {
			if (FD_ISSET(ch.fds[i], &rd)) {
				if (0 > readline(buf, ch.fds[i])) {
					LOGFATAL("Unable to read from channel \"%s\"",
						ch.paths[i]);
				} else {
					if (proc_client_cmd(reply, ch.paths[i], buf))
						write(sockfd, reply, strlen(reply));
				}
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
	char *ret;

	for (i = 0; i < len; ++i) {
		for (j = i; j < i + n; ++j) {
			if ((*pos)[j] != delim[j-i]) {
				break;
			}
		}
		if (j-i == n) {
			ret = *pos;
			*pos += i + n;
			**pos = '\0';
			return ret;
		}
	}
	return NULL;
}

/**
 * TODO(todd): Document this.
 */
void
tokenize(char const *tok[TOK_LAST], char *buf)
{
	size_t i;
	size_t n;
	char *saveptr = buf;

	for (n = (saveptr[0] == ':' ? 0 : 1); n < TOK_LAST; ++n) {
		switch (n) {
		case TOK_PREFIX:
			/* Remove the leading ':' character */
			++saveptr;
			tok[n] = m_tok(&saveptr, " ");
			break;
		case TOK_ARG:
			tok[n] = m_tok(&saveptr, ":");
			/* Strip the whitespace */
			for (i = strlen(saveptr) - 1; i == 0; --i)
				if (isspace(saveptr[i]))
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
}

int
main(int argc, char **argv) 
{
	size_t tmp;
	/* IRC connection context. */
	int sockfd;
	char const *host;
	char const *port;
	char const *nickname;
	char const *realname;
	char const *prefix;
	ArgOption opt[6] = {
		{
			'v', "version", version,
			NULL, NULL,
			"Show the program version and exit"
		},
		{
			'd', "directory", arg_setptr,
			&prefix, default_prefix,
			"Specify the runtime prefix directory to use"
		},
		{
			'n', "nickname", arg_setptr, 
			&nickname, default_nickname,
			"Specify the nickname to login with"
		},
		{
			'r', "realname", arg_setptr,
			&realname, default_realname,
			"Specify the realname to login with"
		},
		{
			'p', "port", arg_setptr,
			&port, default_port,
			"Specify the port of the remote irc server"
		},
		{
			'D', "daemonize", daemonize,
			NULL, NULL,
			"Allow the program to daemonize"
		},
	};

	if (argc < 2)
		usage(sizeof(opt) / sizeof(*opt), opt);
	argv = arg_parse(argv + 1, sizeof(opt) / sizeof(*opt), opt, usage, help);
	if (!argv[0])
		LOGFATAL("No host argument provided.\n");
	/* Append hostname to prefix and slice it */
	tmp = prefix.len;
	sb_cat_str(&prefix, "/");
	sb_cat_str(&prefix, argv[0]);
	if (0 > mkdirpath(prefix))
		LOGFATAL("Unable to create prefix directory\n");
	LOGINFO("Root directory created.\n");
	/* Change to the prefix directory */
	if (0 > chdir(prefix.mem))
		return EXIT_FAILURE;
	sb_slice(&host, &prefix, tmp + 1, prefix.len);
	/* Open the irc-server connection */
	if (0 > tcpopen(&sockfd, host, Ustringref(&port), connect))
		LOGFATAL("Could not connect to host \"%s\" on port \"%s\".\n",
				host.mem, port.mem);
	LOGINFO("Successfully initialized.\n");
	login(sockfd, nickname, realname, host);
	poll_fds(sockfd);
	return 0;
}
