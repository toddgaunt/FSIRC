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
#include <libarg.h>

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
	{logtime(stderr);\
	fprintf(stderr, " "PRGM_NAME": info: "__VA_ARGS__);}

#define LOGERROR(...)\
	{logtime(stderr);\
	fprintf(stderr, " "PRGM_NAME": error: "__VA_ARGS__);}

#define LOGFATAL(...)\
	{logtime(stderr);\
	fprintf(stderr, " "PRGM_NAME": fatal: "__VA_ARGS__);\
	exit(EXIT_FAILURE);}

struct channels {
	size_t size;
	size_t len;
	int *fds;
	stx *paths;
	spx *names;
};

static void
version()
{
	fprintf(stderr, PRGM_NAME" version "VERSION"\n");
	exit(-1);
}

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

	chdir("/");
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
mkdirpath(const spx path)
{
	char tmp[path.len + 1];
	memcpy(tmp, path.mem, path.len);
	tmp[path.len] = '\0';

	if('/' == tmp[path.len - 1])
		tmp[path.len - 1] = '\0';

	for (size_t i=0; i < path.len; ++i) {
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
 * TODO
 */
static int
channel_open_fifo(const spx path)
{
	int fd = -1;
	char tmp[path.len + sizeof("/in")];

	memcpy(tmp, path.mem, path.len);
	strcpy(tmp + path.len, "/in");

	if (0 > (fd = mkfifo(tmp, S_IRWXU))) {
		if (EEXIST == errno) {
			remove(tmp);
			if (0 > (fd = mkfifo(tmp, S_IRWXU))) {
				goto except;
			}
		} else {
			goto except;
		}
	}

	LOGINFO("Input fifo created at \"%s\"\n successfully.", tmp);
	return fd;

except:
	LOGERROR("Input fifo creation at \"%s\" failed.\n", tmp);
	return -1;
}

/**
 * Allocate and initialize memory for a struct channels.
 */
int
channels_init(struct channels *ch, size_t init_size)
{
	if (!(ch->fds = calloc(init_size, sizeof(*ch->fds))))
		goto except;

	if (!(ch->paths = calloc(init_size, sizeof(*ch->paths))))
		goto cleanup_ch_fds;

	if (!(ch->names = calloc(init_size, sizeof(*ch->names))))
		goto cleanup_ch_paths;

	ch->size = init_size;
	ch->len = 0;
	return 0;

cleanup_ch_paths:
	free(ch->paths);
cleanup_ch_fds:
	free(ch->fds);
except:
	return -1;
}

/**
 * Add a channel to a struct channels, and resize the lists if necessary.
 */
int
channels_add(struct channels *ch, const spx path, const spx name)
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

		if (!realloc(ch->paths, sizeof(*ch->paths) * next_size))
			return -1;

		if (!realloc(ch->names, sizeof(*ch->names) * next_size))
			return -1;

		ch->size = next_size;
	}

	size_t i;
	stx *sp = ch->paths + ch->len;

	if (0 > stxensuresize(sp, path.len + name.len + 1)) {
		LOGERROR("Allocation of path buffer for channel \"%.*s\" failed.\n",
				(int)name.len, name.mem);
		return -1;
	}

	stxcpy_spx(sp, path);
	stxapp_mem(sp, "/", 1);
	stxapp_spx(sp, name);

	for (i=sp->len - 1; i>0; --i)
		if ('/' == sp->mem[i])
			break;

	// The name is a slice of the path.
	ch->names[ch->len] = stxslice(stxref(sp), i, sp->len);

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
	ch->paths[index] = ch->paths[ch->len - 1];
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
	free(ch->paths);
	free(ch->names);
}

static void
channels_log()
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
		write(sockfd, "PART", 4);
		//TODO;
	case 'm':
		//TODO;
	case 'r':
		//TODO;
	default: 
		LOGERROR("Invalid command entered\n.");
	}
}

void
assign_spx(struct spx *sp, const char *arg) {
	sp->mem = arg;
	sp->len = strlen(arg);
}

int
main(int argc, char **argv) 
{
	// Message buffer
	stx buf;

	// Channel connection data.
	struct channels ch;

	// Path to runtime directory files
	spx prefix = {
		.mem = default_prefix,
		.len = strlen(default_prefix),
	};

	// Irc server connection info.
	int sockfd = 0;

	spx host = {0};

	spx port = {
		.mem = default_port,
		.len = strlen(default_port)
	};

	spx nickname = {
		.mem = default_nickname,
		.len = strlen(default_nickname)
	};

	spx fullname = {
		.mem = default_fullname,
		.len = strlen(default_fullname)
	};

	struct arg_opt opts[] = {
		{
			.flag = 'h',
			.name = "help",
			.param = opts,
			.callback = arg_help,
			.help = "Show this help message and exit"
		},
		{
			.flag = 'v',
			.name = "version",
			.callback = version,
			.help = "Show the program version and exit"
		},
		{
			.flag = 'p',
			.name = "port",
			.arg = ARG_REQUIRED,
			.param = &port,
			.callback = assign_spx,
			.help = "Specify the [p]ort of the remote irc server"
		},
		{
			.flag = 'D',
			.name = "daemonize",
			.callback = daemonize,
			.help = "Allow the program to [d]aemonize"
		},
		{
			.flag = 'd',
			.name = "directory",
			.arg = ARG_REQUIRED,
			.param = &prefix,
			.callback = assign_spx,
			.help = "Specify the runtime [d]irectory to use"
		},
		{0}
	};

	argv = arg_parse(argv + 1, opts);

	if (!argv[0])
		LOGFATAL("No host argument provided.\n");

	host.mem = argv[0];
	host.len = strlen(argv[0]);

	// Open the irc-server connection.
	if (0 > tcpopen(&sockfd, host.mem, port.mem, connect))
		LOGFATAL("Failed to connect to host \"%s\" on port \"%s\".\n",
				host.mem, port.mem);

	LOGINFO("Successfully initialized.\n");

	// Initialize the message buffer and login to the remote host.
	stxalloc(&buf, MSG_MAX);
	fmt_login(&buf, host.mem, nickname.mem, fullname.mem);
	send_msg(sockfd, &buf);

	// Initialize the channel list and add the remote host as the first
	// channel.
	if (0 > channels_init(&ch, 2))
		LOGFATAL("Allocation of channels list failed.\n");

	if (0 > channels_add(&ch, prefix, host))
		LOGFATAL("Adding root channel \"%.*s\" failed.\n",
				(int)host.len, host.mem);

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

		for (size_t i=0; i<ch.len; ++i) {
			if (ch.fds[i] > maxfd) {
				maxfd = ch.fds[i];
			}
			FD_SET(ch.fds[i], &rd);
		}

		rv = select(maxfd + 1, &rd, 0, 0, &tv);

		if (rv < 0) {
			LOGFATAL("Error running select().\n");
		} else if (rv == 0) {
			//TODO(todd): handle timeout.
		}

		// Remote host has new messages.
		if (FD_ISSET(sockfd, &rd)) {
			if (0 > readline(&buf, sockfd)) {
				LOGERROR("Unable to read from %s: ", host.mem);
				perror("");
				return EXIT_FAILURE;
			} else {
				channels_log();
				fwrite(buf.mem, 1, buf.len, stdout);
				//proc_irc_cmd(sockfd, host, &buf);
			}
		}

		for (size_t i=0; i<ch.len; ++i) {
			if (FD_ISSET(ch.fds[i], &rd)) {
				if (0 > readline(&buf, ch.fds[i])) {
					LOGERROR("Unable to read from (");
					fwrite(ch.paths[i].mem, 1, ch.paths[i].len, stderr);
					fprintf(stderr, "): ");
					perror("");
					return EXIT_FAILURE;
				} else {
					//TODO(todd) Process client message.
				}
			}
		}
	}

	channels_del(&ch);
	stxdel(&buf);

	return 0;
}
