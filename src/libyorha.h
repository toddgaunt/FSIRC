#include <netdb.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

#define NICK_MAX 32
#define REALNAME_MAX 32
#define MSG_MAX 512

/**
 * All RFC mandated irc user commands. The shorthand for these commands should
 * be sent as UPPERCASE.
 */
enum {
	IRC_ADMIN,
	IRC_AWAY,
	IRC_CHAT,
	IRC_CNOTICE,
	IRC_CONNECT,
	IRC_CPRIVMSG,
	IRC_DIE,
	IRC_ENCAP,
	IRC_ERROR,
	IRC_HELP,
	IRC_IGNORE,
	IRC_INFO,
	IRC_INVITE,
	IRC_ISON,
	IRC_JOIN,
	IRC_KICK,
	IRC_KILL,
	IRC_KNOCK,
	IRC_LINKS,
	IRC_LIST,
	IRC_LUSERS,
	IRC_ME,
	IRC_MODE,
	IRC_MOTD,
	IRC_MSG,
	IRC_NAMES,
	IRC_NAMESX,
	IRC_NICK,
	IRC_NOTICE,
	IRC_OPER,
	IRC_PART,
	IRC_PING,
	IRC_PONG,
	IRC_PRIVMSG,
	IRC_QUIT,
	IRC_REHASH,
	IRC_RESTART,
	IRC_RULES,
	IRC_SERVER,
	IRC_SERVICE,
	IRC_SERVLIST,
	IRC_SETNAME,
	IRC_SILENCE,
	IRC_SQUERY,
	IRC_SQUIT,
	IRC_STATS,
	IRC_SUMMON,
	IRC_TIME,
	IRC_TOPIC,
	IRC_TRACE,
	IRC_UHNAMES,
	IRC_USERHOST,
	IRC_USERIP,
	IRC_USERS,
	IRC_VERSION,
	IRC_WALLOPS,
	IRC_WATCH,
	IRC_WHO,
	IRC_WHOIS,
	IRC_WHOWAS,
};

/**
 * Establish a new tcp connection as a server or client.
 *
 * The last argument of this function should be either bind() or connect(). If
 * the function provided is bind(), then 'sockfd' will be bound to a new tcp
 * socket. If instead connect() is provided, then 'sockfd' will be assigned
 * tcp socket connected to the 'host':'port'.
 *
 * Return: 0 upon success, -1 if there was an error using the sockt.
 */
int
yorha_tcpopen(int *sockfd, const char *host, const char *port, 
		int (*open)(int, const struct sockaddr *, socklen_t));

/**
 * Read characters from a file descriptor into a buffer.
 *
 * Read characters from 'fd' into 'buf' until either 'maxrecv' characters have
 * been read or a newline character is read. The newline character is included
 * in the read.
 * 
 * Return: -1 if unable to read from 'fd'. 0 if 'fd' was read until EOF.
 * 'maxrecv' if 'maxrecv' characters were read but fd did not yet reach EOF.
 */
int
yorha_readline(char *buf, size_t *len, size_t maxrecv, int fd);
