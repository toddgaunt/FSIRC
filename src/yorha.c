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

struct irclogin {
	int sockfd;
	const char *username;
	const char *realname;
	const char *host;
	const char *port;
};

typedef struct channel {
	int fdin;
	int fdout;
	char *name;
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
	fprintf(fp, date);
}

static void
usage()
{
	fprintf(stderr, "usage: "PRGM_NAME" [-h] [-p] [-v] [-D]\n");
	exit(EXIT_FAILURE);
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
 * Daemonize a process. If nochdir is 1, the process won't switch to '/'.
 * If noclose is 1, standard streams won't be redirected to /dev/null.
 */
static int
daemonize(int nochdir, int noclose) 
{
	// Fork once to go into the background.
	if (0 > daemonize_fork())
		return -1;

	if (0 > setsid())
		return -1;

	if (!nochdir) {
		chdir("/");
	}

	// Fork once more to ensure no TTY can be required.
	if (0 > daemonize_fork())
		return -1;

	if (!noclose) {
		int fd;
		if (!(fd = open(PATH_NULL, O_RDWR)))
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

/**
 * Recursively make directories given a fullpath.
 */
int
mkdirtree(const char *path)
{
	char tmp[PATH_MAX];
	size_t len;

	snprintf(tmp, sizeof(tmp), "%s", path);
	len = strlen(tmp);
	if('/' == tmp[len - 1])
		tmp[len - 1] = 0;
	for(char *p=tmp; *p; ++p)
		if('/' == *p) {
			*p = 0;
			mkdir(tmp, S_IRWXU);
			*p = '/';
		}
	mkdir(tmp, S_IRWXU);
	return 0;
}

/**
 * Create a new channel.
 */
channel *
channel_new(const char *name)
{
	char path[PATH_MAX];
	channel *tmp;
	if (!(tmp = malloc(sizeof(*tmp))) || (0 > mkdirtree(name)))
		goto except;

	if (!(tmp->name = malloc(strlen(name))))
		goto cleanup_tmp;

	strcpy(tmp->name, name);

	snprintf(path, 256, "%s/%s", name, "in");
	remove(path);
	if (0 > (tmp->fdin = mkfifo(path, S_IRWXU)))
		goto cleanup_tmp_name;
	
	snprintf(path, 256, "%s/%s", name, "out");
	if (0 > (tmp->fdout = open(path, O_APPEND | O_CREAT)))
		goto cleanup_tmp_fdin;

	return tmp;

cleanup_tmp_fdin:
	close(tmp->fdin);
cleanup_tmp_name:
	free(tmp->name);
cleanup_tmp:
	free(tmp);
except:
	LOGERROR("Unable to allocate memory for new channel.\n");
	return NULL;
}

void
irc_login(int sockfd, const char *username, const char *realname)
{
}

void
channel_log(channel *cp, const char *msg)
{
	//TODO(todd): stat the file here, then either remake it if it was
	// closed or use it if still open.
}

int
main(int argc, char **argv) 
{
	int flags = 0;
	const char *prefix = default_prefix;
	channel *chan;
	stx buf;
	struct irclogin ircn = {
		.sockfd = 0,
		.host = NULL,
		.port = default_port,
		.username = default_username,
		.realname = default_realname
	};

	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
			case 'h': 
				usage(); 
				break;
			case 'v': 
				//version();
				break;
			case 'p':
				++i;
				ircn.port = argv[i];
				break;
			case 'D': 
				flags |= FLAG_DAEMON; 
				break;
			case 'o':
				++i;
				prefix = argv[i];
				break;
			default: 
				usage();
			}
		} else {
			ircn.host = argv[i];
		}
	}

	if (!ircn.host)
		LOGFATAL("No remote host provided.\n");
	
	// Setup the daemon.
	if (0 > chdir(prefix))
		LOGFATAL("Unable to chdir to %s\n.", prefix);
	if (FLAG_DAEMON == (flags & FLAG_DAEMON))
		daemonize(1, 0);

	// Open the irc-server connection.
	if (0 > yorha_tcpopen(&ircn.sockfd, ircn.host, ircn.port, connect))
		LOGFATAL("Failed to connect to host \"%s\" on port \"%s\".\n",
				ircn.host, ircn.port);

	LOGINFO("Successfully initialized.\n");

	// Initialize the message buffer.
	stxalloc(&buf, MSG_MAX + 1);

	chan = channel_new(ircn.host);
	list_init(&chan->node);

	irc_login(ircn.sockfd, ircn.username, ircn.realname);

	while (ircn.sockfd) {
		struct list *lp;
		int maxfd;
		fd_set rd;
		struct timeval tv;
		int rv;

		FD_ZERO(&rd);
		FD_SET(ircn.sockfd, &rd);
		tv.tv_sec = ping_timeout;
		tv.tv_usec = 0;
		maxfd = ircn.sockfd;

		// Add all file descriptors back to the ready list.
		lp = &chan->node;
		do {
			channel *tmp = list_get(lp, channel, node);
			if (tmp->fdin > maxfd)
				maxfd = tmp->fdin;
			FD_SET(tmp->fdin, &rd);
			lp = lp->next;
		} while (lp != &chan->node);

		rv = select(maxfd + 1, &rd, 0, 0, &tv);

		if (rv < 0) {
		} else if (rv == 0) {
		}

		// Remote host has new messages.
		if (FD_ISSET(ircn.sockfd, &rd)) {
			if (0 > yorha_readline(buf.mem, &buf.len,
						buf.size - 1,
						ircn.sockfd)) {
				//TODO error;
			} else {
				buf.mem[buf.len] = '\0';
				LOGINFO("%s", buf.mem);
				//TODO(todd): Handle IRC server messages.
			}
		}

		// Find a channel that has new messages.
		lp = &chan->node;
		do {
			channel *tmp = list_get(lp, channel, node);
			if (FD_ISSET(tmp->fdin, &rd)) {
				if (0 > yorha_readline(buf.mem, &buf.len,
							buf.size - 1,
							tmp->fdin)) {
				} else {
					//TODO(todd): do something here.
				}
			}
			lp = lp->next;
		} while (lp != &chan->node);
	}
	
	stxdel(&buf);

	return 0;
}
