#ifndef IRCCD_H_INCLUDED
#define IRCCD_H_INCLUDED

#include <limits.h>

#ifndef PIPE_BUF /* If no PIPE_BUF defined */
#define PIPE_BUF _POSIX_PIPE_BUF
#endif

#define PATH_DEVNULL "/dev/null"

#define VERSION "1.0"
#define DEBUG 1 // toggles debug
#define SERVER_PORT 6667

/* Maximum length of some strings */
#define CHAN_LEN 128
#define IP_LEN 32
#define NICK_LEN 16
#define PING_TIMEOUT 10
#define IRC_BUF_MAX 512

/* Command definitions */
#define CONN_MOD 'c' // connect to a host
#define DISC_MOD 'd' // disconnect from a host
#define JOIN_MOD 'j' // join channel 
#define LOG_MOD 'l' // show chat log
#define NICK_MOD 'n' // nickname mode
#define PART_MOD 'p' // part from channel
#define WRITE_MOD 'w' // write to channel

#define LIST_CHAN_MOD 'C' // list channels joined
#define LIST_MOD 'L' // list remote channels
#define RAW_MOD 'R' // raw message to irc with no formatting
#define PING_MOD 'P' // ping a channel
#define QUIT_MOD 'Q' // quit mode

/* linked list for all Channels */
typedef struct Channel {
	char *name;
	struct Channel *next;
} Channel;

#endif
