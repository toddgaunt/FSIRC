/*
 * Name: irccd
 * Author: Todd Gaunt
 * Last updated: 2016-03-19
 * License: MIT
 * Description:
 * irccd is a simple irc-bot daemon
 * it will auto-save messages for you
 * and can be called from the command line
 * to send messages to the specified channel.
 * TODO conform to XDG spec, ~/.config/irccd/socket, ~/.config/irccd/log etc..
 */

#include <arpa/inet.h> 
#include <errno.h>
#include <malloc.h>
#include <netdb.h>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <unistd.h>
//#include <libconfig.h>//Use this to make config file
/* - - - - - - - */
#include "irccd.h"

static struct Channel *channels = NULL;        /* Head of the connected channels list */
static char socketpath[_POSIX_PATH_MAX] = "/tmp/irccd.socket"; /* Path to socket on filesystem */
static char logpath[_POSIX_PATH_MAX] = "/tmp/irccd.log"; /* Path to socket on filesystem */
static char host_serv[IP_LEN];                 /* String containing server address */
static unsigned short port = 6667;               /* Port to connect with */
static char nick[NICK_LEN] = "iwakura_lain";            /* User nickname */
static char realname[NICK_LEN] = "Todd G.";             /* User nickname */

static void usage()
{
	fprintf(stderr, "irccd - irc client daemon - " VERSION "\n"
		"usage: irccd [-h] [-d]\n");
	exit(EXIT_FAILURE);
}

int log_msg(char *buf)
{/* Writes buf to a log file corresponding to channel message was in */
	FILE *fd = fopen(logpath, "a");
	if (fprintf(fd, buf) < 0) {
		return -1;
	}
	fclose(fd);
	return 0;
}

int send_msg(int sockfd, char *buf)
{/* Send message to socket, max size is PIPE_BUF*/
	// If message was trucated along the way, will correct the issuen
	int n = send(sockfd, buf, PIPE_BUF, 0);
	if (n <= 0) {
		buf = "irccd: unable to send message";
		perror(buf);
	} else {
		printf("irccd: out: %s", buf);
	}
	log_msg(buf);
	return n;
}

int read_line(int sockfd, char *recvline, int len)
{/* Read chars from socket until '\n' */
	int i = 0;
	char c;
	do {
		if (read(sockfd, &c, sizeof(char)) != sizeof(char)) {
			perror("irccd: Unable to read socket");
			return -1;
		}
		recvline[i++] = c;
	} while (c != '\n' && i < len);

	return 0;
}

int socket_connect(char *host, int *sockfd, int port)
{/* Construct socket at sockfd, then connect */
	struct sockaddr_in servaddr; 
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	
	// Define socket to be TCP
	*sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (*sockfd < 0) {
		perror("irccd: cannot create socket");
		return 1;
	}
	if (inet_pton(AF_INET, host, &servaddr.sin_addr) <= 0) {
		printf("irccd: cannot transform ip\n");
		return 1;
	}
	// Points sockaddr pointer to servaddr because connect takes sockaddr structs
	if (connect(*sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
		perror("irccd: unable to connect socket");
		return 1;
	}

	return 0;
}

int socket_bind(const char *path, int *sockfd)
{/* Contruct a unix socket for inter-process communication, then connect */
	struct sockaddr_un servaddr; 
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sun_family = AF_UNIX;
	strcpy(servaddr.sun_path, path);

	// Define socket to be a Unix socket
	*sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (*sockfd < 0) {
		perror("irccd: cannot create socket");
		return 1;
	}
	// Before binding, unlink to detach previous owner of socket 
	// eg. earlier invocations of this program
	unlink(path);

	if (bind(*sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
		perror("irccd: unable to bind socket");
		return 1;
	}
	return 0;
}

int fork_reader(int sockfd)
{/* Fork process to constantly read from irc socket */
	int pid = fork(); 
	if (pid == 0) {
		char recvline[PIPE_BUF];
		int timer = 0;
		while(1) {
			memset(recvline, 0, sizeof(recvline));
			if (read_line(sockfd, recvline, PIPE_BUF) != 0) {
				timer += 1;
				if (timer >= PING_TIMEOUT) {
					close(sockfd);
					exit(1); // kill the reader if read attempt fails after PING_TIMEOUT (10 seconds)
				}
				sleep(1);
			} else { 
				printf("irccd: in: %s", recvline);
				log_msg(recvline);
				if (strstr(recvline, "PING") != NULL) {
					recvline[1] = 'O'; // Swaps ping 'I' to 'O' for "PONG"
					send_msg(sockfd, recvline);
				}
				timer = 0;
			}
		}
	}
	return pid;
}

int fork_sender(int sockfd)
{/* Fork a process that handles message queueing and sends them */
	return -1;
}

int add_chan(char *chan_name)
{/* Add a channel to end of a list */
	Channel *tmp = channels;
	while (tmp->next != NULL) {
		tmp = tmp->next;
		if (strcmp(tmp->name, chan_name) == 0) {
			return 1;
		}
	}
	tmp->next = (Channel *)calloc(1, sizeof(Channel)); 
	tmp = tmp->next; 
	if (tmp == 0) {
		perror("irccd: Cannot allocate memory");
		return -1;
	}
	tmp->name = strdup(chan_name); // mallocs then strcpy
	tmp->next = NULL;

	printf("irccd: channel added: %s\n", tmp->name);
	return 0;
}

int rm_chan(char *chan_name)
{/* Remove an entry with name = chan_name from list */
	Channel *garbage;
	Channel *tmp = channels;
	while (tmp->next != NULL) {
		if (strcmp(tmp->next->name, chan_name) != 0) {
			tmp = tmp->next;
		} else {
			garbage = tmp->next; 
			tmp->next = tmp->next->next;

			printf("irccd: channel removed: %s\n", garbage->name);

			free(garbage->name);
			free(garbage);
			return 0;
		}
	}
	printf("irccd: channel %s not found in list\n", chan_name);
	return 1;
}

void list_chan(char *buf, int len)
{/* Creates a string of all channels into *buf */
	memset(buf, 0, len);
	Channel *tmp = channels;
	while (tmp->next != NULL) {
		tmp = tmp->next;
		// Add another name only if buf can hold another channel name
		if (strlen(buf) < len - CHAN_LEN - 1) {
			sprintf(buf, "%s%s->", buf, tmp->name);
		} else break;
	}
	sprintf(buf, "%s\n", buf);
}

int ping_host(int sockfd)
{/* Sends a ping message to a socket */
	return send_msg(sockfd, "PING :irccd\r\n");
}

int chan_check(char *chan_name) 
{/* Performs checks to make sure the string is a channel name */
	if (chan_name[0] != '#' || strlen(chan_name) > CHAN_LEN) {
		fprintf(stderr, "irccd: %s is not a valid channel name\n", chan_name);
		return 1;
	}
	return 0;
}

void quit_cmd(int pid, int fd)
{
	if (pid != 0) {
		kill(pid, SIGTERM);
		pid = 0;
	}
	exit(0);
}

void login(int sockfd)
{
	char buf[32 + (NICK_LEN*3)]; // size for holding snprintf message
	sprintf(buf, "USER %s 8 * :%s\r\nNICK %s\r\n", nick, realname, nick);
	send_msg(sockfd, buf);
}

int main(int argc, char *argv[]) 
{
	// Ignore sigpipes, they're handled internally
	signal(SIGPIPE, SIG_IGN);

	// TCP socket for irc server
	int tcpfd = 0;

	// Name of channel last used to connect to, outgoing messages get sent here.
    char chan_name[CHAN_LEN];

	// Message buffers for sending and recieving 
	char message[PIPE_BUF];
	char buf[PIPE_BUF];
	char out[PIPE_BUF+32]; // Memory buffer for when we want to preserve buf

	// Unix domain socket for clients to communicate with irccd
	int unixfd = 0;
	socket_bind(socketpath, &unixfd);

	// Initialize channels
	channels = (Channel*)malloc(sizeof(Channel));

	// File descriptor for unix socket
	int fd, i;
	int pid = 0;

	char actmode = 0; /* Which mode to operate on */
	listen(unixfd, 5);
	while(1) {
		fd = accept(unixfd, 0, 0);
		memset(buf, 0, sizeof(buf)); // reset buf to zeros before receiving
		recv(fd, buf, sizeof(buf), 0); 
		actmode = buf[0];
		fprintf(stdout, "irccd: mode: %c\n", actmode);

		for (i=0; i<sizeof(buf)-1; i++) {
			buf[i] = buf[i+1]; // Stores the message seperately from actcode
		}
		buf[sizeof(buf)-1]='\0';

		switch (actmode) {
		case JOIN_MOD:
			if (chan_check(buf) != 0) {
				break;
			}
			// Extract the name from buf
			strcpy(chan_name, buf);
			snprintf(out, sizeof(out), "JOIN %s\r\n", chan_name);
			if (send_msg(tcpfd, out) > 0) {
				add_chan(chan_name);
			}
			break;
		case PART_MOD:
			// Must not modify chan_name
			if (chan_check(buf) != 0) {
				break;
			}
			snprintf(out, sizeof(out), "PART %s\r\n", buf);

			if (send_msg(tcpfd, out) > 0) {
				rm_chan(buf);
			}
			break;
		case LIST_MOD:
			snprintf(out, sizeof(out), "LIST %s\r\n", buf);
			send_msg(tcpfd, out);
			break;
		case LIST_CHAN_MOD:
			// Only communicates with local unix socket, not irc
			list_chan(message, sizeof(message));
			break;
		case WRITE_MOD:
			snprintf(out, sizeof(out), "PRIVMSG %s :%s\r\n", chan_name, buf);
			send_msg(tcpfd, out);
			break;
		case NICK_MOD:
			if (strcmp(nick, buf) == 0) {
				fprintf(stdout, "irccd: nickname %s already in use "
					"by this client\n", nick);
				break;
			} 
			strcpy(nick, buf);
			snprintf(out, sizeof(out), "NICK %s\r\n", nick);
			send_msg(tcpfd, out);
			break;
		case CONN_MOD:
			// Test if we're already connected somewhere
			if (ping_host(tcpfd) != -1) {
				snprintf(message, sizeof(message), "irccd: already connected to %s\r\n", host_serv);
				fprintf(stderr, buf);
			} else {
				strcpy(host_serv, buf);
				socket_connect(host_serv, &tcpfd, port);
				// Forked process reads socket
				pid = fork_reader(tcpfd);
				login(tcpfd);
				snprintf(message, sizeof(message), "irccd: connected to %s\r\n", host_serv);
			}
			break;
		case PING_MOD:
			ping_host(tcpfd);
			break;
		case DISC_MOD:
			if (tcpfd != 0) {
				send_msg(tcpfd, "QUIT :Bye bye!\r\n");
				close(tcpfd);
				if (pid != 0) {
					kill(pid, SIGTERM); // Kills child reader
					pid = 0; // Set pid to zero so this won't kill main if invoked again
				}
			} else {
				fprintf(stderr, "Cannot disonnect socket: No connected socket\n");
			}
			tcpfd = 0; // Zero signifies non-existent socket
			break;
		case QUIT_MOD:
			quit_cmd(pid, fd);
			break;
		default:
			printf("NO COMMAND\n");
			sleep(1);
			break;
		}
		// Sends result of commands back to client
		send(fd, message, sizeof(message), 0);
		close(fd);
	}
	quit_cmd(pid, fd);
}
