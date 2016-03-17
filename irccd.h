/* Include guards */
#ifndef IRCCD_H_INCLUDED
#define IRCCD_H_INCLUDED

/* Maximum length of some strings */
#define MAX_BUF 4096
#define MAX_LINE 1024
/* Actcode definitions, might change to pure ints */
#define QUIT_MOD 'q' // quit mode
#define WRIT_MOD 'w' // write mode
#define DEBUG_MOD 'd' // debug mode
#define JOIN_MOD 'j' // join mode
#define PART_MOD 'p' // part mode
#define NICK_MOD 'n' // nickname mode

/* connects to server, returns 0 if failure, 1 if success */
int host_conn(char *server, unsigned int port, int *sockfd);

int send_msg(int sockfd, char *out, int debug);
int read_msg(int sockfd, char *recvline, int debug);

/* cleans up all forked processes */
void kill_children(int pid, int ecode);

#endif
