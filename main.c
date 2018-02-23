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
#include <stdarg.h>

#include "sys.h"

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
#define IRC_NAME_MAX 32
#define IRC_CHAN_MAX 200

#include "config.h"

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

typedef enum {
	/* Prefix */
	TOK_NICK = 0,
	TOK_USER,
	TOK_HOST,
	/* Commands */
	TOK_CMD,
	TOK_CHAN, /* First argument is often the channel, but not always */
	TOK_ARG,
	TOK_TEXT,
	TOK_LAST,
} Token;

typedef struct Channel Channel;
struct Channel {
	size_t n;
	int fd;
	char name[IRC_CHAN_MAX];
	char inpath[PATH_MAX];
	char outpath[PATH_MAX];
	Channel *next;
	Channel *prev;
};

typedef struct {
	int sockfd;
	char nickname[32];
	char realname[32];
	Channel *chan;
	/* Message buffer used for communications */
	char buf[MSG_MAX];
} ServerConnection;

static Channel *channel_create(char const *name);
static void channel_destroy(Channel const *garbage);
static Channel *channel_find(Channel *root, char const *name);
static void channel_link(Channel *new, Channel *prev, Channel *next);
static void channel_printf(Channel *chan, char const *fmt, ...);
static Channel *channel_join(Channel *root, char const *name);
static void channel_part(Channel *root, char const *name);
void login(const int sockfd, char const *nick, char const *real, char const *host);
void logtime(FILE *fp);
static void poll_fds(ServerConnection *sc);
bool proc_server_cmd(char reply[MSG_MAX], ServerConnection *sc);
bool proc_client_cmd(char reply[MSG_MAX], char const *name, char const *msg);
static int readline(char dest[MSG_MAX], int fd);
size_t tokenize(char const *tok[TOK_LAST], char *buf);

/* Program name */
char const *argv0 = "";

/* Root channel path */
char const *root_path = ".";

/* Display a usage message and then exit */
static void
usage()
{
	printf("usage: %s [-v <version>] [-d <dir>] [-n <nickname>]"
			"[-r <realname>] [-p <port>] [-D] <host>\n",
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

static Channel *
channel_create(char const *name)
{
	Channel *new = calloc(1, sizeof(*new));

	strncpy(new->name, name, IRC_CHAN_MAX);
	snprintf(new->inpath, PATH_MAX, "%s/in", new->name);
	snprintf(new->outpath, PATH_MAX, "%s/out", new->name);
	if (0 > mkdirpath(new->name))
		LOGFATAL("Unable to create directory for channel %s\n", new->name);
	remove(new->inpath);
	if (0 > mkfifo(new->inpath, S_IRWXU))
		LOGFATAL("Unable to create input file for channel %s\n", new->name);
	new->fd = open(new->inpath, O_RDWR | O_NONBLOCK, 0);
	if (0 > new->fd)
		LOGFATAL("Unable to open input file for channel %s\n", new->name);
	return new;
}

static void
channel_destroy(Channel const *garbage)
{

	garbage->next->prev = garbage->prev;
	garbage->prev->next = garbage->next;
	close(garbage->fd);
	free((void *)garbage);
}

static void
channel_link(Channel *new, Channel *prev, Channel *next)
{
	assert(NULL != new);
	assert(NULL != prev);
	assert(NULL != next);
		
	new->next = next;
	new->prev = prev;
	prev->next = new;
	next->prev = new;
}

static Channel *
channel_find(Channel *root, char const *name)
{
	Channel *cp = root;

	cp = root;
	do {
		if (0 == strcmp(name, cp->name))
			return cp;
		cp = cp->next;
	} while (cp != root);
	return NULL;
}

static Channel *
channel_join(Channel *root, char const *name)
{
	Channel *new;
	
	if (!root) {
		new = channel_create(name);
		channel_link(new, new, new);
	} else {
		new = channel_find(root, name);
		if (!new) {
			new = channel_create(name);
			channel_link(new, root->prev, root);
		}
	}
	return new;
}

static void
channel_part(Channel *root, char const *name)
{
	Channel const *garbage;
	
	garbage = channel_find(root, name);
	if (garbage)
		channel_destroy(garbage);
}

static void
channel_printf(Channel *chan, char const *fmt, ...)
{
	FILE *fp;
	va_list ap;
	char buf[MSG_MAX];

	if (!(fp = fopen(chan->outpath, "a"))) {
		LOGERROR("Output file \"%s\" failed to open.\n", chan->outpath);
	} else {
		logtime(fp);
		va_start(ap, fmt);
		vsnprintf(buf, MSG_MAX, fmt, ap);
		fputs(buf, fp);
		va_end(ap);
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
proc_server_cmd(char reply[MSG_MAX], ServerConnection *sc)
{
	char tmp[MSG_MAX];
	char const *argv[TOK_LAST] = {0};
	char const *channel = NULL;

	strncpy(tmp, sc->buf, MSG_MAX);
	tokenize(argv, tmp);
	if (0 == strcmp("PING", argv[TOK_CMD])) {
		snprintf(reply, MSG_MAX, "PONG %s\r\n", argv[TOK_TEXT]);
		return true;
	} else if (0 == strcmp("JOIN", argv[TOK_CMD])) {
		snprintf(sc->buf, MSG_MAX, "--> %s joined %s\n", argv[TOK_NICK], argv[TOK_ARG]);
	} else if (0 == strcmp("PART", argv[TOK_CMD])) {
		snprintf(sc->buf, MSG_MAX, "<-- %s parted from %s\n", argv[TOK_NICK], argv[TOK_ARG]);
	} else if (0 == strcmp("PRIVMSG", argv[TOK_CMD])) {
		snprintf(sc->buf, MSG_MAX, "<%s> %s\n", argv[TOK_NICK], argv[TOK_TEXT]);
	} else if (0 == strcmp("NOTICE", argv[TOK_CMD])) {
		snprintf(sc->buf, MSG_MAX, "-!- %s\n", argv[TOK_TEXT]);
	} else if (0 == strcmp("MODE", argv[TOK_CMD])) {
		snprintf(sc->buf, MSG_MAX, "-!- %s changed mode %s -> %s %s\n",
				argv[TOK_NICK],
				argv[TOK_CHAN],
				argv[TOK_ARG],
				argv[TOK_TEXT]);
		channel = root_path;
	} else if (0 == strcmp("KICK", argv[TOK_CMD])) {
		snprintf(sc->buf, MSG_MAX, "-!- %s kicked %s (%s)\n", argv[TOK_NICK], argv[TOK_ARG], argv[TOK_TEXT]);
	} else {
		channel = root_path;
		/* Can't read this command */
		LOGINFO("Cannot read command: %s\n", argv[TOK_CMD]);
		return false;
	}

	if (!channel)  {
		if ('\0' == argv[TOK_CHAN][0]
		|| 0 == strcmp("*", argv[TOK_CHAN])) {
			channel = root_path;
		} else {
			channel = argv[TOK_CHAN];
		}
	}

	sc->chan = channel_join(sc->chan, channel);
	channel_printf(sc->chan, sc->buf);
	return false;
}

/**
 * Process a Ustring into a message to be sent to an IRC channel.
 */
bool
proc_channel_cmd(char reply[MSG_MAX], Channel *chan, ServerConnection *sc)
{
	size_t i;
	size_t buf_len = strlen(sc->buf);
	char const *body;

	if (sc->buf[0] != '/') {
		channel_printf(chan, "[%s] %s\n", sc->nickname, sc->buf);
		snprintf(reply, MSG_MAX, "PRIVMSG %s :%s\r\n", chan->name, sc->buf);
	} else if (sc->buf[0] == '/' && buf_len > 2) {
		/* Remove leading whitespace. */
		for (i = 2; i < buf_len && isspace(sc->buf[i]); ++i)
			;
		body = &sc->buf[i];
		printf("body: \"%s\"\n", body);
		switch (sc->buf[1]) {
		/* Join a channel */
		case 'j':
			snprintf(reply, MSG_MAX, "JOIN %s\r\n", body);
			break;
		/* Part from a channel */
		case 'p': 
			snprintf(reply, MSG_MAX, "PART %s\r\n", body);
			break;
		/* Send a "me" message */
		case 'm':
			channel_printf(chan, "* %s %s\n", sc->nickname, body);
			snprintf(reply, MSG_MAX, "PRIVMSG %s :\001ACTION %s\001\r\n",
					chan->name, body);
			break;
		/* Change nick name */
		case 'n':
			strncpy(sc->nickname, body, IRC_NAME_MAX);
			snprintf(reply, MSG_MAX, "NICK %s\r\n", body);
		/* Set status to "away" */
		case 'a':
			if (3 < buf_len) {
				snprintf(reply, MSG_MAX, "AWAY\r\n");
			} else {
				snprintf(reply, MSG_MAX, "AWAY :%s\r\n", body);
			}
			break;
		/* Send raw IRC protocol */
		case 'r':
			snprintf(reply, MSG_MAX, "%s\r\n", body);
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
poll_fds(ServerConnection *sc)
{
	fd_set rd;
	int maxfd;
	int rv;
	struct timeval tv;
	char reply[MSG_MAX];
	Channel *cp;

	/* Add the root channel for the connection */
	sc->chan = channel_join(NULL, root_path);
	while (1) {
		FD_ZERO(&rd);
		maxfd = sc->sockfd;
		FD_SET(sc->sockfd, &rd);
		cp = sc->chan;
		do {
			if (maxfd < cp->fd)
				maxfd = cp->fd;
			FD_SET(cp->fd, &rd);
			cp = cp->next;
		} while (cp != sc->chan);
		tv.tv_sec = ping_timeout;
		tv.tv_usec = 0;
		rv = select(maxfd + 1, &rd, 0, 0, &tv);
		if (rv < 0) {
			LOGFATAL("Error running select().\n");
		} else if (rv == 0) {
			//TODO(todd): handle timeout with ping message.
		}
		/* Check for messages from remote host */
		if (FD_ISSET(sc->sockfd, &rd)) {
			do {
				rv = readline(sc->buf, sc->sockfd);
				if (0 > rv)
					LOGFATAL("Unable to read from socket");
				LOGINFO("server: %s\n", sc->buf);
				if (proc_server_cmd(reply, sc)) {
					write(sc->sockfd, reply, strlen(reply));
				}
			} while (0 < rv);
		}
		cp = sc->chan;
		do {
			if (FD_ISSET(cp->fd, &rd)) {
				do {
					rv = readline(sc->buf, cp->fd);
					if (0 > rv)
						LOGFATAL("Unable to read from channel \"%s\"",
							cp->name);
					LOGINFO("%s: %s\n", cp->name, sc->buf);
					if (proc_channel_cmd(reply, cp, sc))
						write(sc->sockfd, reply, strlen(reply));
				} while (0 < rv);
			}
			cp = cp->next;
		} while (cp != sc->chan);
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
	Token n;
	char *tmp;
	char *saveptr = buf;
	static const char *empty = "";

	/* Initialize each token result to a null string for safety */
	for (i = 0; i < TOK_LAST; ++i) {
		tok[i] = empty;
	}
	for (n = (saveptr[0] == ':' ? TOK_NICK : TOK_CMD); n < TOK_LAST; ++n) {
		switch (n) {
		case TOK_NICK:
			/* Ignore the leading ':' character */
			saveptr += 1;
			tok[n] = m_tok(&saveptr, " ");
			n = TOK_CMD - 1;
			break;
		case TOK_CMD:
		case TOK_CHAN:
			tok[n] = m_tok(&saveptr, " ");
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
		case TOK_USER:
		case TOK_HOST:
		case TOK_LAST:
			LOGFATAL("Tokenization error, attempt to parse token that shouldn't be\n");
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
	ServerConnection sc;
	char const *host = NULL;
	char const *port = default_port;
	char const *directory = default_directory;
	char prefix[PATH_MAX];

	memset(&sc, 0, sizeof(sc));
	strncpy(sc.nickname, default_nickname, sizeof(sc.nickname));
	strncpy(sc.realname, default_realname, sizeof(sc.realname));
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
					strncpy(sc.nickname, opt_arg, IRC_NAME_MAX);
					break;
				case 'r':
					if (!opt_arg) usage();
					strncpy(sc.realname, opt_arg, IRC_NAME_MAX);
					break;
				case 'p':
					if (!opt_arg) usage();
					port = opt_arg;
					break;
				case 'D':
					daemonize();
					break;
				default:
					usage();
				}
			}
		} else {
			host = argv[i];
		}
	}
	if (!host)
		usage();
	/* Open the irc-server connection */
	if (0 > tcpopen(&sc.sockfd, host, port, connect))
		LOGFATAL("Unable to connect to host \"%s\" on port \"%s\"\n",
				host, port);
	/* Create the prefix from the directory */
	if (PATH_MAX <= snprintf(prefix, PATH_MAX, "%s/%s", directory, host))
		LOGFATAL("Runtime directory path too long\n");
	if (0 > mkdirpath(prefix))
		LOGFATAL("Unable to create runtime directory directory\n");
	LOGINFO("Runtime directory created.\n");
	/* Change to the prefix directory */
	if (0 > chdir(prefix))
		LOGFATAL("Unable to chdir to runtime directory\n");
	LOGINFO("Successfully initialized\n");
	login(sc.sockfd, sc.nickname, sc.realname, host);
	poll_fds(&sc);
	return EXIT_SUCCESS;
}
