#ifndef IRCCD_H_INCLUDED
#define IRCCD_H_INCLUDED

/* Maximum length of some strings */
#define MAX_BUF 4096
#define MAX_LINE 1024
#define CHAN_LEN 128
#define NICK_LEN 16
/* Command definitions, might change to pure ints */
#define CONN_MOD 'c' // connect to a host
#define DISC_MOD 'd' // disconnect from a host
#define JOIN_MOD 'j' // join channel 
#define LOG_MOD 'l' // log chat display
#define NICK_MOD 'n' // nickname mode
#define PART_MOD 'p' // part from channel
#define WRITE_MOD 'w' // write to channel

#define LIST_MOD 'L' // list channels
#define RAW_MOD 'R' // raw message to irc with no formatting
#define PING_MOD 'P' // ping a channel
#define QUIT_MOD 'Q' // quit mode

#define DEBUG 1 // toggles debug
#define PRGM_NAME "irccd"

/* linked list for all Channels */
typedef struct Channel{
	char *name;
	struct Channel *next;
} Channel;

typedef struct Conn_Info Conn_Info;
struct Conn_Info {
	/* Connection info */
	char host[CHAN_LEN];
	char channel[CHAN_LEN];
	unsigned int port;
	char nick[CHAN_LEN];
	char realname[CHAN_LEN];
	int sockfd; //socket
};

#endif
