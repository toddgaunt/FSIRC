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
#include "libarg.h"

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

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

#define NICK_MAX 32
#define REALNAME_MAX 32
#define MSG_MAX 512

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

typedef struct channel {
	int fd;
	stx path;
	stx name;
	struct list node;
} channel;

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
 * Daemonize a process. If nochdir is 1, the process won't switch to '/'.
 * If noclose is 1, standard streams won't be redirected to /dev/null.
 */
static int
daemonize()
{
	// Fork once to go into the background.
	if (0 > daemonize_fork())
		return -1;

	if (0 > setsid())
		return -1;

	// Fork once more to ensure no TTY can be required.
	if (0 > daemonize_fork())
		return -1;

	int fd;
	if (!(fd = open("/dev/null", O_RDWR)))
		return -1;
	dup2(fd, STDIN_FILENO);
	dup2(fd, STDOUT_FILENO);
	dup2(fd, STDERR_FILENO);
	// Only close the fd if it isn't one of the std streams.
	if (fd > 2)
		close(fd);

	chdir("/");

	return 0;
}

int
tcpopen(int *sockfd, const char *host, const char *port, 
		int (*open)(int, const struct sockaddr *, socklen_t))
{
	struct addrinfo hints;
	struct addrinfo *res;
	struct addrinfo *ptr;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	// Get the ip address of 'host'.
	if (getaddrinfo(host, port, &hints, &res)) {
		return -1;
	}
	
	// Attempt to connect to any available address.
	for (ptr=res; ptr; ptr=ptr->ai_next) {
		if (0 > (*sockfd = socket(ptr->ai_family, ptr->ai_socktype,
				ptr->ai_protocol)))
			continue;

		if (0 > open(*sockfd, ptr->ai_addr, ptr->ai_addrlen))
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
 */
int
mkdirtree(const stx *path)
{
	char tmp[path->len + 1];
	memcpy(tmp, path->mem, path->len);
	tmp[path->len] = '\0';

	if('/' == tmp[path->len - 1])
		tmp[path->len - 1] = '\0';

	for (size_t i=0; i < path->len; ++i) {
		if('/' == tmp[i]) {
			tmp[i] = 0;
			mkdir(tmp, S_IRWXU);
			tmp[i] = '/';
		}
	}

	mkdir(tmp, S_IRWXU);

	return 0;
}

int
channel_open_fifo(channel *cp)
{
	char tmp[cp->path.len + 4];
	memcpy(tmp, cp->path.mem, cp->path.len);
	strcpy(tmp + cp->path.len, "/in");

	if (0 > (cp->fd = mkfifo(tmp, S_IRWXU)))
		if (EEXIST == errno) {
			remove(tmp);
			if (0 > (cp->fd = mkfifo(tmp, S_IRWXU)))
					return -1;
		} else {
			return -1;
		}

	LOGINFO("Fifo file opened (%s)\n.", tmp);
	return 0;
}

/**
 * Create a new channel.
 */
channel *
channel_new(const stx *path)
{
	size_t i;
	channel *tmp;

	if (0 > mkdirtree(path))
		goto except;

	if (!(tmp = malloc(sizeof(*tmp)))) {
		LOGERROR("Allocation for channel failed.\n");
		goto except;
	}

	if (0 < stxalloc(&tmp->path, path->len)) {
		LOGERROR("Allocation for channel path failed.\n");
		goto cleanup_tmp;
	}

	stxcpy_stx(&tmp->path, path);
	for (i=tmp->path.len - 1; i>0; --i) {
		if ('/' == tmp->path.mem[i])
			break;
	}

	stxslice(&tmp->name, &tmp->path, i, tmp->path.len);

	if (0 < channel_open_fifo(tmp))
		goto cleanup_tmp_path;

	return tmp;

cleanup_tmp_path:
	stxdel(&tmp->path);
cleanup_tmp:
	free(tmp);
except:
	LOGERROR("Unable to create new channel (%s).\n", path->mem);
	return NULL;
}

void
channel_del(channel *cp)
{
	close(cp->fd);
	free(cp);
}

void
channel_log(const char *path, const char *msg, size_t len)
{
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

	return 0;
}

void
fmt_login(stx *buf, const char *host, const char *nick, const char *fullname)
{
	buf->len = snprintf(buf->mem, buf->size,
			"NICK %s\r\nUSER %s localhost %s :%s\r\n",
			nick, nick, host, fullname);
}

void
proc_channel_cmd(int sockfd, const stx *name, stx *buf)
{
	if (buf->mem[0] != '/') {
		write(sockfd, "PRIVMSG", 7);
		write(sockfd, name->mem, name->len);
		write(sockfd, " :", 2);
		write(sockfd, buf->mem, buf->len);
		write(sockfd, "\r\n", 2);
		return;
	}

	if (2 > buf->len)
		LOGERROR("Invalid command entered\n.");

	switch (buf->mem[1]) {
	case 'j': 
		write(sockfd, "JOIN", 4);
		//TODO;
	case 'p': 
		//TODO;
	case 'm':
		//TODO;
	case 'r':
		//TODO;
	default: 
		LOGERROR("Invalid command entered\n.");
	}
}

int
main(int argc, char **argv) 
{
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

	struct args args = {0};

	struct arg_opt opts[] = {
		{
			.flag = 'p',
			.name = "port",
			.callback2 = arg_assign_ptr_once,
			.callback_arg = &port,
			.callback_default_str = default_port,
			.help = "Specify the [p]ort of the remote irc server"
		},
		{
			.flag = 'd',
			.name = "daemon",
			.callback0 = daemonize,
			.help = "Allow the program to [d]aemonize"
		},
		{
			.flag = 'r',
			.name = "run",
			.callback2 = arg_assign_ptr_once,
			.callback_arg = &prefix,
			.callback_default_str = default_prefix,
			.help = "Specify the [r]untime directory to use"
		},
		{
			.flag = 'h',
			.name = "help",
			.callback1 = arg_help,
			.callback_arg = &args,
			.help = "Show a help message and exit"
		},
		{
			.flag = 'v',
			.name = "version",
			.help = "Show the program version and exit"
		},
	};

	struct arg_req req = {
		.name = "host",
		.callback = arg_assign_ptr_once,
		.callback_arg = &host,
	};

	arg_add_opts(&args, opts, sizeof(opts)/sizeof(*opts));
	arg_add_req(&args, &req);

	if (0 > arg_parse(&args, argv + 1)) {
		arg_usage(&args);
		exit(-1);
	}

	// Open the irc-server connection.
	if (0 > tcpopen(&sockfd, host, port, connect))
		LOGFATAL("Failed to connect to host \"%s\" on port \"%s\".\n",
				host, port);

	LOGINFO("Successfully initialized.\n");

	// Initialize the message buffer.
	stxalloc(&buf, MSG_MAX);

	// Initialize the list and add the first channel.
	stxvapp_str(&buf, prefix, "/", host, 0);
	list_init(&chan_head);
	list_append(&chan_head, &channel_new(&buf)->node);

	fmt_login(&buf, host, nickname, fullname);
	send_msg(sockfd, &buf);

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
			if (tmp->fd > maxfd)
				maxfd = tmp->fd;
			FD_SET(tmp->fd, &rd);
		}

		rv = select(maxfd + 1, &rd, 0, 0, &tv);

		if (rv < 0) {
			LOGFATAL("Error running select().\n");
		} else if (rv == 0) {
		}

		// Remote host has new messages.
		if (FD_ISSET(sockfd, &rd)) {
			if (0 > readline(&buf, sockfd)) {
				LOGERROR("Unable to read from %s: ", host);
				perror("");
				exit(-1);
			} else {
				fwrite(buf.mem, 1, buf.len, stdout);
				//proc_irc_cmd(sockfd, host, &buf);
			}
		}

		LIST_FOR_EACH (lp, &chan_head) {
			channel *tmp = LIST_GET(lp, channel, node);
			if (FD_ISSET(tmp->fd, &rd)) {
				if (0 > readline(&buf, tmp->fd)) {
					LOGERROR("Unable to read from (");
					fwrite(tmp->path.mem, 1, tmp->path.len, stderr);
					fprintf(stderr, "): ");
					perror("");
					exit(-1);
				} else {
					printf("hi\n");
					proc_channel_cmd(sockfd, &tmp->name, &buf);
				}
			}
		}
	}
	
	stxdel(&buf);

	return 0;
}
