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

#include "channels.h"
#include "arg.h"
#include "config.h"
#include "strbuf.h"

#ifndef PRGM_NAME
#define PRGM_NAME "NULL"
#endif

#ifndef VERSION
#define VERSION "NULL"
#endif

#ifndef PREFIX
#define PREFIX "/usr/local"
#endif

#define MSG_MAX 512

/* Logging macros */
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

/* Enums and Structures */
/* Used in tokenize as indices for a token array. */
enum {
	TOK_PREFIX = 0,
	TOK_CMD,
	TOK_ARG,
	TOK_TEXT,
	TOK_LAST
};

/* Function Declarations */
void login(const int sockfd, struct strbuf const *nick,
		struct strbuf const *real,
		struct strbuf const *host);
void logtime(FILE *fp);
static void poll_fds(int sockfd);
bool proc_server_cmd(struct strbuf *reply, Channels *ch, Ustr msg);
bool proc_client_cmd(struct strbuf *reply, Ustr name, Ustr msg);
static int readline(struct strbuf *sp, int fd);
void tokenize(Ustr *tok, size_t ntok, const Ustr buf);
static void version();
static int write_Ustr(int sockfd, const Ustr sp);

/* Global constants */
const Ustr root = {1, "."};

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
	arg_printhelp(optc, optv);
	exit(EXIT_FAILURE);
}

/* Create a new strbuf from a C string */
void
set_strbuf(struct strbuf *dest, char const *str)
{
	sb_init(dest);
	sb_cat_str(dest, str);
}

int
main(int argc, char **argv) 
{
	size_t tmp;
	/* IRC connection context. */
	int sockfd;
	struct strbuf host = SB_INIT;
	struct strbuf port = SB_INIT;
	struct strbuf nickname = SB_INIT;
	struct strbuf realname = SB_INIT;
	struct strbuf prefix = SB_INIT;
	struct arg_option opt[6] = {
		{
			'v', "version", 0, NULL,
			version, NULL,
			"Show the program version and exit"
		},
		{
			'd', "directory", 1, &prefix,
			set_strbuf, default_prefix,
			"Specify the runtime directory to use"
		},
		{
			'n', "nickname", 1, &nickname,
			set_strbuf, default_nickname,
			"Specify the nickname to login with"
		},
		{
			'r', "realname", 1, &realname,
			set_strbuf, default_realname,
			"Specify the realname to login with"
		},
		{
			'p', "port", 1, &port,
			set_strbuf, default_port,
			"Specify the port of the remote irc server"
		},
		{
			'D', "daemonize", 0, NULL,
			daemonize, NULL,
			"Allow the program to daemonize"
		},
	};

	if (argc < 2)
		arg_usage(sizeof(opt) / sizeof(*opt), opt);
	argv = arg_parse(argv + 1, sizeof(opt) / sizeof(*opt), opt);
	if (!argv[0])
		LOGFATAL("No host argument provided.\n");
	/* Append hostname to prefix and slice it */
	tmp = prefix.len;
	sb_cat_str(&prefix, "/");
	sb_cat_str(&prefix, argv[0]);
	/* Change to the supplied prefix */
	if (0 > mkdirpath(prefix))
		return EXIT_FAILURE;
	LOGINFO("Root directory created.\n");
	if (0 > chdir(prefix.mem))
		return EXIT_FAILURE;
	sb_slice(&host, &prefix, tmp + 1, prefix.len);
	/* Open the irc-server connection */
	if (0 > tcpopen(&sockfd, host, Ustringref(&port), connect))
		LOGFATAL("Could not connect to host \"%s\" on port \"%s\".\n",
				host.mem, port.mem);
	LOGINFO("Successfully initialized.\n");
	login(sockfd, nickname.mem, realname.mem, host.mem);
	poll_fds(sockfd);
	return 0;
}

/**
 * Display program version and then exit.
 */
static void
version()
{
	fprintf(stderr, PRGM_NAME" version "VERSION"\n");
	exit(-1);
}

static int
write_sb(int sockfd, struct strbuf const *buf)
{
	//TODO(todd): Split up buffers larger than MSG_MAX
	return write(sockfd, buf.mem, buf.len);
}

static int
readline(struct strbuf *sp, int fd)
{
	char ch;

	sp->len = 0;
	do {
		if (1 != read(fd, &ch, 1))
			return -1;
		sp->mem[sp->len] = ch;
		sp->len += 1;
	} while (ch != '\n' && sp->len <= sp->size);
	/* Remove line delimiters as they make logging difficult */
	sb_rstrip(sp, "\r\n", 2);
	return 0;
}

/**
 * Initial login to the IRC server.
 */
void
login(const int sockfd, struct strbuf const *nick, struct strbuf const *real, struct strbuf const *host)
{

	size_t len;
	struct strbuf buf = SB_INIT;

	//TODO(todd): Error handling
	sb_cat_sb(&buf, SB_LIT("NICK "));
	sb_cat_sb(&buf, nick);
	sb_cat_sb(&buf, SB_LIT("\r\nUSER "));
	sb_cat_sb(&buf, nick);
	sb_cat_sb(&buf, SB_LIT(" localhost "));
	sb_cat_sb(&buf, host);
	sb_cat_sb(&buf, SB_LIT(" :"));
	sb_cat_sb(&buf, real);
	sb_cat_sb(&buf, SB_LIT("\r\n"));
	write_sb(sockfd, USTR(buf));
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
	fprintf(fp, buf);
}

/**
 * Process and incoming message from the IRC server connection.
 */
bool
proc_server_cmd(struct strbuf *reply, Channels *ch, Ustr msg)
{
	struct strbuf tok[TOK_LAST];
	tokenize(tok, TOK_LAST, msg);

	if (sb_eq(&tok[TOK_CMD], &SB_LIT("PING"))) {
		snprintf(reply->mem, reply->size,
				"PONG %.*s\r\n",
				(int)tok[TOK_ARG].len,
				tok[TOK_ARG].mem);
		return true;
	} else if (sb_eq(tok[TOK_CMD].mem, &SB_LIT("PART"))) {
		channels_del(ch, tok[TOK_ARG]);
	} else if (sb_eq(tok[TOK_CMD].mem, &SB_LIT("JOIN"))) {
		channels_add(ch, tok[TOK_ARG]);
	} else if (sb_eq(tok[TOK_CMD].mem, &SB_LIT("PRIVMSG"))) {
		channels_log(tok[TOK_ARG], msg);
	} else {
		channels_log(root, msg);
	}
	return false;
}

/**
 * Process a Ustring into a message to be sent to an IRC channel.
 */
bool
proc_client_cmd(struct strbuf *reply, Ustr name, Ustr msg)
{
	size_t i;
	Ustr slice;

	if (msg.mem[0] != '/') {
		LOGINFO("Sending message to \"%.*s\"", (int)name.len, name.mem);
		snprintf(reply->mem, reply->size,
				"PRIVMSG %.*s :%.*s\r\n", 
				(int)name.len,
				name.mem,
				(int)msg.len,
				msg.mem);
	} else if (msg.mem[0] == '/' && msg.len > 1) {
		/* Remove leading whitespace. */
		for (i = 0; i < msg.len && msg.mem[i] != ' '; ++i);
		slice = Ustringslice(msg, i, msg.len);
		switch (msg.mem[1]) {
		/* Join a channel */
		case 'j':
			snprintf(reply->mem, reply->size,
					"JOIN %.*s\r\n",
					(int)slice.len,
					slice.mem);
			break;
		/* Part from a channel */
		case 'p': 
			snprintf(reply->mem, reply->size,
					"PART %.*s\r\n",
					(int)slice.len,
					slice.mem);
			break;
		/* Send a "me" message */
		case 'm':
			snprintf(reply->mem, reply->size,
					"ME %.*s\r\n",
					(int)slice.len,
					slice.mem);
			break;
		/* Set status to "away" */
		case 'a':
			snprintf(reply->mem, reply->size,
					"AWAY %.*s\r\n",
					(int)slice.len,
					slice.mem);
			break;
		/* Send raw IRC protocol */
		case 'r':
			snprintf(reply->mem, reply->size,
					"%.*s\r\n",
					(int)slice.len,
					slice.mem);
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
	struct strbuf reply = SB_INIT;
	struct strbuf buf = SB_INIT;
	Channels ch = {0};

	/* Each tree begins with a root */
	channels_add(&ch, root);
	/* Initialize the string buffers to IRC maximum message size */
	ustr_alloc(&buf, MSG_MAX + 1);
	ustr_alloc(&reply, MSG_MAX + 1);
	while (1) {
		size_t i;
		fd_set rd;
		int maxfd;
		int rv;
		struct timeval tv;

		FD_ZERO(&rd);
		maxfd = sockfd;
		FD_SET(sockfd, &rd);
		for (i = 0; i < ch.len; ++i) {
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
			if (0 > readline(&buf, sockfd)) {
				LOGFATAL("Unable to read from socket");
			} else {
				/* TMP */
				fprintf(stderr, "%.*s\n", (int)buf.len, buf.mem);
				/* ENDTMP */
				if (proc_server_cmd(&reply, &ch, USTR(buf)))
					write_ustr(sockfd, USTR(reply));
			}
		}
		for (i = 0; i < ch.len; ++i) {
			if (FD_ISSET(ch.fds[i], &rd)) {
				if (0 > readline(&buf, ch.fds[i])) {
					LOGFATAL("Unable to read from channel \"%.*s\"",
						(int)ch.names[i].len,
						ch.names[i].mem);
				} else {
					if (proc_client_cmd(&reply,
							USTR(ch.names[i]),
							USTR(buf)))
						write_ustr(sockfd, USTR(reply));
				}
			}
		}
	}
}

/**
 * TODO(todd): Document this.
 */
void
tokenize(struct strbuf *tok, size_t ntok, struct strbuf *buf)
{
	size_t i;
	size_t n;
	char const *saveptr = buf.mem;

	for (n = (slice.mem[0] == ':' ? 0 : 1); n < ntok; ++n) {
		switch (n) {
		case TOK_PREFIX:
			tok[n] = Ustringtok(&slice, " ", 1);
			/* Remove the leading ':' character */
			tok[n].len -= 1;
			tok[n].mem += 1;
			break;
		case TOK_ARG:
			tok[n] = Ustringtok(&slice, ":", 1);
			/* Strip the whitespace */
			for (i = slice.len - 1; i <= 0; --i)
				if (isspace(slice.mem[i]))
					--slice.len;
			break;
		case TOK_TEXT:
			/* Grab the remainder of the text */
			tok[n] = slice;
			break;
		default:
			tok[n] = Ustringtok(&slice, " ", 1);
			break;
		}
	}
}