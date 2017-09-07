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

#define MSG_MAX 512

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
yorha_login(const int sockfd, const spx nick, const spx real, const spx host)
{

	size_t len;
	char buf[MSG_MAX];

	len = snprintf(buf, MSG_MAX,
			"NICK %.*s\r\nUSER %.*s localhost %.*s :%.*s\r\n",
			(int)nick.len, nick.mem,
			(int)nick.len, nick.mem,
			(int)host.len, host.mem, 
			(int)real.len, real.mem);
	write(sockfd, buf, len);
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
bool
proc_server_cmd(stx *reply, Channels *ch, spx buf)
{
	spx tok[TOK_LAST] = {0};
	tokenize(tok, TOK_LAST, buf);

	spx cmd = tok[TOK_CMD];
	if (stxcmp(cmd, (spx){4, "PING"})) {
		snprintf(reply->mem, reply->size,
				"PONG %.*s\r\n",
				(int)tok[TOK_ARG].len,
				tok[TOK_ARG].mem);
		return true;
	} else if (stxcmp(cmd, (spx){4, "PART"})) {
		channels_del(ch, tok[TOK_ARG]);
	} else if (stxcmp(cmd, (spx){4, "JOIN"})) {
		channels_add(ch, tok[TOK_ARG]);
	} else if (stxcmp(cmd, (spx){7, "PRIVMSG"})) {
		channels_log(tok[TOK_ARG], buf);
	} else {
		channels_log(root, buf);
	}
	return false;
}

/**
 * Process a stx into a message to be sent to an IRC channel.
 * TODO(todd): Refactor write calls into a single, buffered write call.
 */
bool
proc_client_cmd(stx *reply, spx name, spx buf)
{
	size_t i;
	spx slice;

	if (buf.mem[0] != '/') {
		LOGINFO("Sending message to \"%.*s\"", (int)name.len, name.mem);
		snprintf(reply->mem, reply->size,
				"PRIVMSG %.*s :%.*s\r\n", 
				(int)name.len,
				name.mem,
				(int)buf.len,
				buf.mem);
	} else if (buf.mem[0] == '/' && buf.len > 1) {
		/* Remove leading whitespace. */
		for (i = 0; i < buf.len && buf.mem[i] != ' '; ++i);
		slice = stxslice(buf, i, buf.len);
		switch (buf.mem[1]) {
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
yorha_poll(int sockfd)
{
	stx reply = {0};
	stx buf = {0};
	Channels ch = {0};

	/* Each tree begins with a root */
	channels_add(&ch, root);
	/* Initialize the buffer to the IRC protocal message length */
	stxalloc(&buf, MSG_MAX);
	stxalloc(&reply, MSG_MAX);
	while (1) {
		size_t i;
		fd_set rd;
		int maxfd;
		int rv;
		struct timeval tv;

		FD_ZERO(&rd);
		maxfd = sockfd;
		FD_SET(sockfd, &rd);
		for (i=0; i<ch.len; ++i) {
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

		/* Check for messages from remote host */
		if (FD_ISSET(sockfd, &rd)) {
			if (0 > readline(&buf, sockfd)) {
				LOGFATAL("Unable to read from socket");
			} else {
				/* TMP */
				fprintf(stderr, "%.*s\n", (int)buf.len, buf.mem);
				/* ENDTMP */
				if (proc_server_cmd(&reply, &ch, stxr(buf)))
					write_spx(sockfd, stxr(reply));
			}
		}

		for (i=0; i<ch.len; ++i) {
			if (FD_ISSET(ch.fds[i], &rd)) {
				if (0 > readline(&buf, ch.fds[i])) {
					LOGFATAL("Unable to read from channel \"%.*s\"",
						(int)ch.names[i].len,
						ch.names[i].mem);
				} else {
					if (proc_client_cmd(&reply,
							stxr(ch.names[i]),
							stxr(buf)))
						write_spx(sockfd, stxr(reply));
				}
			}
		}
	}
}

int
main(int argc, char **argv) 
{
	size_t tmp = 0;
	/* IRC connection context. */
	int sockfd = 0;
	spx host = {0};
	stx port = {0};
	stx nickname = {0};
	stx realname = {0};
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
			'n', "nickname", 1, &nickname,
			setstx, default_nickname,
			"Specify the nickname to login with"
		},
		{
			'r', "realname", 1, &realname,
			setstx, default_realname,
			"Specify the realname to login with"
		},
		{
			'p', "port", 1, &port,
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
	argv = arg_sort(argv + 1);
	argv = arg_parse(argv, sizeof(opt) / sizeof(*opt), opt);
	if (!argv[0])
		LOGFATAL("No host argument provided.\n");
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
	host = stxslice(stxref(&prefix), tmp + 1, prefix.len);
	/* Open the irc-server connection */
	if (0 > tcpopen(&sockfd, host, stxref(&port), connect))
		LOGFATAL("Failed to connect to host \"%s\" on port \"%s\".\n",
				host.mem, port.mem);
	LOGINFO("Successfully initialized.\n");
	yorha_login(sockfd, stxr(nickname), stxr(realname), host);
	yorha_poll(sockfd);
	return 0;
}
