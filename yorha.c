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
#include <stdbool.h>

#include "src/arg.h"
#include "src/sys.h"
#include "src/math.h"
#include "src/channels.h"
#include "src/log.h"
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

#define MSG_INIT 512

/* Enums and Structures */
/* Used in tokenize as indices for a token array. */
enum {
	TOK_PREFIX = 0,
	TOK_CMD,
	TOK_ARG,
	TOK_TEXT,
	TOK_LAST
};

typedef struct {
	int sockfd;
	spx host;
	stx port;
	stx nickname;
	stx realname;
	Channels ch;
	/* Message buffer */
	stx buf;
} IRConnection;

/* Function Declarations */
static void version();

/* Global constants */
const spx root = {1, "."};

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
write_spx(int sockfd, const spx sp)
{
	return write(sockfd, sp.mem, sp.len);
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
	/* Remove line delimiters as they make logging difficult */
	stxrstrip(sp, "\r\n", 2);
	return 0;
}

/**
 * Initial login to the IRC server.
 */
void
yorha_login(IRConnection *conn)
{
	conn->buf.len = snprintf(conn->buf.mem, conn->buf.size,
			"NICK %.*s\r\nUSER %.*s localhost %.*s :%.*s\r\n",
			(int)conn->nickname.len, conn->nickname.mem,
			(int)conn->nickname.len, conn->nickname.mem,
			(int)conn->host.len, conn->host.mem, 
			(int)conn->realname.len, conn->realname.mem);
	write_spx(conn->sockfd, stxref(&conn->buf));
}

/**
 * TODO(todd): Document this.
 */
void
tokenize(spx *tok, size_t ntok, const spx buf)
{
	size_t i;
	size_t n;
	spx slice = stxslice(buf, 0, buf.len);

	for (n = (slice.mem[0] == ':' ? 0 : 1); n < ntok; ++n) {
		switch (n) {
		case TOK_PREFIX:
			tok[n] = stxtok(&slice, " ", 1);
			/* Remove the leading ':' character */
			tok[n].len -= 1;
			tok[n].mem += 1;
			break;
		case TOK_ARG:
			tok[n] = stxtok(&slice, ":", 1);
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
			tok[n] = stxtok(&slice, " ", 1);
			break;
		}
	}
}

/**
 * Process and incoming message from the IRC server connection.
 * TODO(todd): Refactor write calls into a single, buffered write call.
 */
void
proc_server_cmd(IRConnection *conn)
{
	spx tok[TOK_LAST] = {0};
	tokenize(tok, TOK_LAST, stxr(conn->buf));

	spx cmd = tok[TOK_CMD];
	if (stxcmp(cmd, (spx){4, "PING"})) {
		write(conn->sockfd, "PONG", 4);
		write_spx(conn->sockfd, tok[TOK_ARG]);
		write(conn->sockfd, "\r\n", 2);
	} else if (stxcmp(cmd, (spx){4, "PART"})) {
		channels_del(&conn->ch, tok[TOK_ARG]);
	} else if (stxcmp(cmd, (spx){4, "JOIN"})) {
		channels_add(&conn->ch, tok[TOK_ARG]);
	} else if (stxcmp(cmd, (spx){7, "PRIVMSG"})) {
		channels_log(tok[TOK_ARG], stxr(conn->buf));
	} else {
		channels_log(root, stxr(conn->buf));
	}
}

/**
 * Process a stx into a message to be sent to an IRC channel.
 * TODO(todd): Refactor write calls into a single, buffered write call.
 */
void
proc_client_cmd(IRConnection *conn, const spx name)
{
	size_t i;
	spx slice;
	spx buf = stxr(conn->buf);

	if (buf.mem[0] != '/') {
		LOGINFO("Sending message to \"%.*s\"", (int)name.len, name.mem);
		write(conn->sockfd, "PRIVMSG ", 8);
		write(conn->sockfd, name.mem, name.len);
		write(conn->sockfd, " :", 2);
		write(conn->sockfd, buf.mem, buf.len);
		write(conn->sockfd, "\r\n", 2);
	}
	if (buf.mem[0] == '/' && buf.len > 1) {
		/* Remove leading whitespace. */
		for (i = 0; i < buf.len && buf.mem[i] != ' '; ++i);
		slice = stxslice(buf, i, buf.len);
		switch (buf.mem[1]) {
		/* Join a channel */
		case 'j':
			write(conn->sockfd, "JOIN ", 5);
			break;
		/* Part from a channel */
		case 'p': 
			write(conn->sockfd, "PART", 4);
			break;
		/* Send a "me" message */
		case 'm':
			write(conn->sockfd, "ME", 2);
			break;
		/* Set status to "away" */
		case 'a':
			write(conn->sockfd, "AWAY", 4);
			break;
		/* Send raw IRC protocol */
		case 'r':
			break;
		default: 
			LOGERROR("Invalid command entered\n.");
			return;
		}
		write(conn->sockfd, slice.mem, slice.len);
		write(conn->sockfd, "\r\n", 2);
	}
}

/**
 * Function used as an argument parsing callback function.
 */
void
setstx(size_t argc, struct stx *argv, const char *arg) {
	size_t i;

	for (i=0; i<argc; ++i) {
		if (0 < stxensuresize(&argv[i], strlen(arg) + 1)) {
			LOGFATAL("Allocation of argument string failed.\n");
		}

		stxterm(stxcpy_str(&argv[i], arg));
	}
}

/**
 * Poll all open file descriptors for incoming messages, and act upon them.
 */
static void
yorha_poll(IRConnection *conn)
{
	size_t i;
	fd_set rd;
	int maxfd;
	int rv;
	struct timeval tv;

	FD_ZERO(&rd);
	maxfd = conn->sockfd;
	FD_SET(conn->sockfd, &rd);
	for (i=0; i<conn->ch.len; ++i) {
		if (maxfd < conn->ch.fds[i]) {
			maxfd = conn->ch.fds[i];
		}
		FD_SET(conn->ch.fds[i], &rd);
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
	if (FD_ISSET(conn->sockfd, &rd)) {
		if (0 > readline(&conn->buf, conn->sockfd)) {
			LOGFATAL("Unable to read from %s: ", conn->host.mem);
		} else {
			/* TMP */
			fprintf(stderr, "%.*s\n", (int)conn->buf.len, conn->buf.mem);
			/* ENDTMP */
			proc_server_cmd(conn);
		}
	}

	for (i=0; i<conn->ch.len; ++i) {
		if (FD_ISSET(conn->ch.fds[i], &rd)) {
			if (0 > readline(&conn->buf, conn->ch.fds[i])) {
				LOGFATAL("Unable to read from channel \"%.*s\"",
					(int)conn->ch.names[i].len,
					conn->ch.names[i].mem);
			} else {
				proc_client_cmd(conn, stxr(conn->ch.names[i]));
			}
		}
	}
}

int
main(int argc, char **argv) 
{
	size_t tmp;
	/* IRC connection context. */
	IRConnection conn = {0};
	/* Runtime prefix to run in. */
	stx prefix = {0};
	struct arg_option opt[7] = {
		{
			'h', "help", sizeof(opt) / sizeof(*opt), opt,
			arg_help, NULL,
			"Show this help message and exit"
		},
		{
			'v', "version", 0, NULL,
			version, NULL,
			"Show the program version and exit"
		},
		{
			'd', "directory", 1, &prefix,
			setstx, default_prefix,
			"Specify the runtime directory to use"
		},
		{
			'n', "nickname", 1, &conn.nickname,
			setstx, default_nickname,
			"Specify the nickname to login with"
		},
		{
			'r', "realname", 1, &conn.realname,
			setstx, default_realname,
			"Specify the realname to login with"
		},
		{
			'p', "port", 1, &conn.port,
			setstx, default_port,
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
	//TODO(todd): Implement this.
	//argv = arg_sort(argv + 1);
	argv = arg_parse(argv, sizeof(opt) / sizeof(*opt), opt);
	if (!argv[0])
		LOGFATAL("No host argument provided.\n");
	/* Initialize the message buffer, connect, and login to IRC server. */
	stxalloc(&conn.buf, MSG_INIT);
	/* Append hostname to prefix and slice it */
	tmp = prefix.len;
	if (0 < stxensuresize(&prefix, tmp + strlen(argv[0]) + 2))
		return EXIT_FAILURE;
	stxterm(stxapp_str(stxapp_char(&prefix, '/'), argv[0]));
	/* Change to the supplied prefix */
	if (0 > mkdirpath(stxref(&prefix)))
		LOGFATAL("Creation of prefix directory \"%s\" failed.\n",
				prefix.mem);
	if (0 > chdir(prefix.mem)) {
		exit(EXIT_FAILURE);
		LOGERROR("Could not change directory: ");
		perror("");
		return EXIT_FAILURE;
	}
	conn.host = stxslice(stxref(&prefix), tmp + 1, prefix.len);
	/* Open the irc-server connection */
	if (0 > tcpopen(&conn.sockfd, conn.host, stxref(&conn.port), connect))
		LOGFATAL("Failed to connect to host \"%s\" on port \"%s\".\n",
				conn.host.mem, conn.port.mem);
	LOGINFO("Successfully initialized.\n");
	/* Root directory channel for representing default server channel. */
	channels_add(&conn.ch, root);
	yorha_login(&conn);
	while (1) {
		yorha_poll(&conn);
	}
	return 0;
}
