#ifndef IRCCD_H_INCLUDED
#define IRCCD_H_INCLUDED

#include <errno.h>
#include <stdlib.h>
#include <limits.h>

#ifndef PIPE_BUF /* If no PIPE_BUF defined */
#define PIPE_BUF _POSIX_PIPE_BUF
#endif

#define PATH_DEVNULL "/dev/null"

#define PING_TIMEOUT 10


struct irc_srv_conn {
	char *nick;
	char *realname;
	char *host;
	unsigned int port;

	int tcpfd;
};

struct channel {
	char *name;
	struct Channel *next;
};

int log_msg(char *buf);

int send_msg(int *sockfd, char *buf, int len);

#endif
