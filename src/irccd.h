#ifndef IRCCD_H_INCLUDED
#define IRCCD_H_INCLUDED

#include <limits.h>

#define PRGNAME "irccd"
#define DEBUG 1 // toggles debug

/* Maximum length of some strings */
#define CHAN_LEN 128
#define IP_LEN 32
#define NICK_LEN 16
#define PING_TIMEOUT 10
#define IRCCD_PORT 6667
#define VERSION "1.0"

/* Command definitions, might change to pure ints */
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

typedef struct Ircmessage {
	char channel[CHAN_LEN];
	char sender[NICK_LEN];
	char body[PIPE_BUF];
	char time[NICK_LEN];
} Message;

#endif
