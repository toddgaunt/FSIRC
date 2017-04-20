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
#include <time.h>
#include <unistd.h>
#include <libstx.h>

#include "libyorha.h"
#include "yorha_config.h"
#include "list.h"

#ifndef PATH_MAX
#define PATH_MAX
#endif

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

#define LOGINFO(...)\
	{logtime(stderr);\
	fprintf(stderr, " "PRGM_NAME": info: "__VA_ARGS__);}

#define LOGERROR(...)\
	{logtime(stderr);\
	fprintf(stderr, " "PRGM_NAME": error: "__VA_ARGS__);}

#define LOGFATAL(...)\
	{logtime(stderr);\
	fprintf(stderr, " "PRGM_NAME": fatal: "__VA_ARGS__);\
	exit(EXIT_FAILURE);}

// Bitmask flags for program arguments.
enum {
	FLAG_DAEMON = 1 << 0,
};

typedef struct channel {
	int fdin;
	int fdout;
	stx path;
	struct list node;
} channel;

static void
logtime(FILE *fp)
{
	char date[16];
	time_t t;
	const struct tm *tm;

	t = time(NULL);
	tm = localtime(&t);
	strftime(date, sizeof(date), "%m %d %T", tm);
}

static void
usage()
{
	fprintf(stderr, "usage: "PRGM_NAME" [-h] [-p] [-v] [-D]\n");
	exit(EXIT_FAILURE);
}

/**
 * Recursively make directories given a fullpath.
 */
int
mkdirtree(const char *path, size_t len)
{
	char tmp[len];
	snprintf(tmp, len, "%s", path);

	if('/' == tmp[len - 1])
		tmp[len - 1] = '\0';

	for (size_t i=0; i < len; ++i) {
		if('/' == tmp[i]) {
			tmp[i] = 0;
			mkdir(tmp, S_IRWXU);
			tmp[i] = '/';
		}
	}

	mkdir(tmp, S_IRWXU);

	return 0;
}

/**
 * Open up the "in" and "out" files for the channel.
 */
int
channel_ensure_files_open(channel *cp)
{
	char tmp[cp->path.len + 5];
	struct stat st;

	// Check to see if the file is linked at all.
	fstat(cp->fdin, &st);
	if (!st.st_nlink) {
		LOGINFO("Creating in-file: \"%s\".\n", tmp);
		snprintf(tmp, cp->path.len, "%s/in", cp->path.mem);
		if (0 > (cp->fdin = mkfifo(tmp, S_IRWXU)))
			if (EEXIST != errno)
				goto except;
	} 

	// Check to see if the file is linked at all.
	fstat(cp->fdin, &st);
	if (!st.st_nlink) {
		LOGINFO("Creating out-file: \"%s\".\n", tmp);
		snprintf(tmp, cp->path.len, "%s/out", cp->path.mem);
		if (0 > (cp->fdout = open(tmp, O_APPEND | O_CREAT)))
			goto except;
	}
	
	return 0;

except:
	LOGERROR("Unable open file for channel \"%s\".\n", tmp);
	return -1;
}

/**
 * Create a new channel.
 */
channel *
channel_new(const char *prefix, const char *name)
{
	stx path;
	channel *tmp;

	if (!stxalloc(&path, strlen(prefix) + strlen(name)))
		goto except;

	stxvapp_str(&path, prefix, "/", name, NULL);

	if (0 > mkdirtree(path.mem, path.len))
		goto cleanup_path;

	if (!(tmp = malloc(sizeof(*tmp))))
		goto cleanup_path;

	tmp->path = path;
	tmp->fdin = -1;
	tmp->fdout = -1;

	if (0 < channel_ensure_files_open(tmp))
		goto cleanup_tmp;

	return tmp;

cleanup_tmp:
	free(tmp);
cleanup_path:
	stxdel(&path);
	LOGERROR("Allocation for channel failed.\n");
except:
	LOGERROR("Unable to create new channel (%s).\n", name);
	return NULL;
}

void
channel_del(channel *cp)
{
	close(cp->fdout);
	close(cp->fdin);
	free(cp);
}

void
channel_log(channel *cp, const char *msg)
{
	//TODO(todd): stat the file here, then either remake it if it was
	// closed or use it if still open.
}


void
fmt_login_msg(stx *buf,
		const char *host,
		const char *nick,
		const char *fullname)
{
	buf->len = snprintf(buf->mem, buf->size,
			"NICK %s\r\nUSER %s localhost %s :%s\r\n",
			nick, nick, host, fullname);
}


int
main(int argc, char **argv) 
{
	int flags = 0;
	struct list chan_head;
	stx buf;

	// Path to in/out files
	const char *prefix = default_prefix;

	// Irc server connection info.
	int sockfd = 0;
	const char *host = NULL;
	const char *port = default_port;
	const char *nickname = default_nickname;
	const char *fullname = default_fullname;

	YORHA_FOR_EACH (char **, ap, argv + 1, argc) {
		if (*ap[0] == '-') {
			switch (*ap[1]) {
			case 'h': 
				usage(); 
				break;
			case 'v': 
				//version();
				break;
			case 'p':
				++*ap;
				if (!*ap)
					usage();
				port = *ap;
				break;
			case 'D': 
				flags |= FLAG_DAEMON; 
				break;
			case 'o':
				++*ap;
				if (!*ap)
					usage();
				prefix = *ap;
				break;
			default: 
				usage();
			}
		} else {
			host = *ap;
		}
	}

	if (!host)
		LOGFATAL("No remote host provided.\n");

	if (FLAG_DAEMON == (flags & FLAG_DAEMON))
		yorha_daemonize();

	// Open the irc-server connection.
	if (0 > yorha_tcpopen(&sockfd, host, port, connect))
		LOGFATAL("Failed to connect to host \"%s\" on port \"%s\".\n",
				host, port);

	LOGINFO("Successfully initialized.\n");

	// Initialize the message buffer.
	stxalloc(&buf, MSG_MAX + 1);

	// Initialize the list and add the first channel.
	list_init(&chan_head);
	list_append(&chan_head, &channel_new(prefix, host)->node);

	fmt_login_msg(&buf, host, nickname, fullname);
	yorha_send_msg(sockfd, buf.mem, buf.len);

	while (sockfd) {
		int maxfd;
		fd_set rd;
		struct timeval tv;
		int rv;

		FD_ZERO(&rd);
		FD_SET(sockfd, &rd);
		tv.tv_sec = ping_timeout;
		tv.tv_usec = 0;
		maxfd = sockfd;

		// Add all file descriptors back to the ready list.
		LIST_FOR_EACH (lp, &chan_head) {
			channel *tmp = LIST_GET(lp, channel, node);
			if (tmp->fdin > maxfd)
				maxfd = tmp->fdin;
			FD_SET(tmp->fdin, &rd);
		}

		rv = select(maxfd + 1, &rd, 0, 0, &tv);

		if (rv < 0) {
		} else if (rv == 0) {
		}

		// Remote host has new messages.
		if (FD_ISSET(sockfd, &rd)) {
			if (0 > yorha_readline(buf.mem, &buf.len,
						buf.size - 1,
						sockfd)) {
				//TODO error;
			} else {
				buf.mem[buf.len] = '\0';
				LOGINFO("%s", buf.mem);
				//TODO(todd): Handle IRC server messages.
			}
		}

		LIST_FOR_EACH (lp, &chan_head) {
			channel *tmp = LIST_GET(lp, channel, node);
			if (FD_ISSET(tmp->fdin, &rd)) {
				if (0 > yorha_readline(buf.mem, &buf.len,
							buf.size - 1,
							tmp->fdin)) {
					//TODO error;
				} else {
					//TODO(todd): do something here.
				}
			}
		}
	}
	
	stxdel(&buf);

	return 0;
}
