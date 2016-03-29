#ifndef IRCCD_H_INCLUDED
#define IRCCD_H_INCLUDED

/* Maximum length of some strings */
#define MAX_BUF 4096
#define MAX_LINE 1024
#define CHAN_LEN 128
/* Command definitions, might change to pure ints */
#define QUIT_MOD 'q' // quit mode
#define WRIT_MOD 'w' // write to channel
#define JOIN_MOD 'j' // join channel 
#define PART_MOD 'p' // part from channel
#define LOG_MOD 'l' // log chat display
#define LIST_MOD 'L' // list channels
#define NICK_MOD 'n' // nickname mode

#define DEBUG 1 // toggles debug

/* linked list for all Channels */
typedef struct Channel Channel;
struct Channel {
    char *name;
    struct Channel *next;
};

/* The backbone of most other functions here */
int send_msg(int sockfd, char *out);
int read_msg(int sockfd, char *recvline);

/* The commands sent to this irc daemon */
int host_conn(char *server, unsigned int port, int *sockfd);
//TODO
int add_chan(Channel *head, char *name);
int rm_chan();
int list_chan(Channel *head);

/* cleans up all forked processes */
void kill_children(int pid, int ecode);

#endif
