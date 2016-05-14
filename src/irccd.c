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
static char *socketpath = "/tmp/irccd.socket"; /* Path to socket on filesystem */
static char host_serv[IP_LEN];                 /* String containing server address */
static unsigned int port = 6667;               /* Port to connect with */
static char *nick = "iwakura_lain";            /* User nickname */
static char *realname = "Todd G.";             /* User nickname */

static void usage()
{
	fprintf(stderr, "irccd - irc client daemon - " VERSION "\n"
		"usage: irccd [-h] [-d]\n");
	exit(EXIT_FAILURE);
}

int log_msg(char *buf)
{/* Writes buf to a log file corresponding to channel message was in */
	return 1;
}

int send_msg(int sockfd, char *out)
{/* Send message to socket */
	int n = send(sockfd, out, PIPE_BUF, 0);
	if (n <= 0) {
		perror(PRGNAME": unable to send message");
	} else {
		printf(PRGNAME": out: %s", out);
	}
	return n;
}

int read_line(int sockfd, char *recvline, int max_buf)
{/* Read chars from socket until '\n' */
	int i = 0;
	char c;
	do {
		if (read(sockfd, &c, sizeof(char)) != sizeof(char)) {
			perror(PRGNAME": Unable to read socket");
			return -1;
		}
		recvline[i++] = c;
	} while (c != '\n' && i < max_buf);

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
		perror(PRGNAME": cannot create socket");
		return 1;
	}
	if (inet_pton(AF_INET, host, &servaddr.sin_addr) <= 0) {
		perror(PRGNAME": cannot transform ip");
		return 1;
	}
	// Points sockaddr pointer to servaddr because connect takes sockaddr structs
	if (connect(*sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
		perror(PRGNAME": unable to connect socket");
		return 1;
	}

	return 0;
}

int socket_bind(char *path, int *sockfd)
{/* Contruct a unix socket for inter-process communication, then connect */
	struct sockaddr_un servaddr; 
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sun_family = AF_UNIX;
	strcpy(servaddr.sun_path, path);

	// Define socket to be a Unix socket
	*sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (*sockfd < 0) {
		perror(PRGNAME": cannot create socket");
		return 1;
	}
	// Before binding, unlink to detach previous owner of socket 
	// eg. earlier invocations of this program
	unlink(path);

	if (bind(*sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
		perror(PRGNAME": unable to bind socket");
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
		char *pos, out[PIPE_BUF]; // For ping messaging
		while(1) {
			memset(&recvline, 0, PIPE_BUF);
			if (read_line(sockfd, recvline, PIPE_BUF) != 0) {
				timer += 1;
				if (timer >= PING_TIMEOUT) {
					close(sockfd);
					exit(1); // kill the reader if read attempt fails after PING_TIMEOUT (10 seconds)
				}
				sleep(1);
			} else { 
				printf(PRGNAME": in: %s", recvline);
				log_msg(recvline);
				if (strstr(recvline, "PING") != NULL) {
					pos = strstr(recvline, " ")+1;
					sprintf(out, "PONG %s\r\n", pos);
					send_msg(sockfd, out);
				}
				timer = 0;
			}
		}
	}
	return pid;
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
	tmp->next = (Channel *)malloc(sizeof(Channel)); 
	tmp = tmp->next; 
	if (tmp == 0) {
		perror(PRGNAME": Cannot allocate memory");
		return -1;
	}
	tmp->name = strdup(chan_name);
	tmp->next = NULL;

	printf(PRGNAME": channel added: %s\n", tmp->name);
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

			printf(PRGNAME": channel removed: %s\n", garbage->name);

			free(garbage->name);
			free(garbage);
			return 0;
		}
	}
	printf(PRGNAME": channel %s not found in list\n", chan_name);
	return 1;
}

void list_chan(char *buf)
{/* Creates a string of all channels into *buf */
	Channel *tmp = channels;
	while (tmp->next != NULL) {
		tmp = tmp->next;
		sprintf(buf, "%s%s->", buf, tmp->name);
	}
	sprintf(buf, "%s\n", buf);
}

int ping_host(int sockfd, char *msg)
{/* Sends a ping message to a socket */
	char out[PIPE_BUF];
	sprintf(out, "PING %s\r\n", msg);
	return send_msg(sockfd, out);
}

int chan_check(char *chan_name) 
{/* Performs checks to make sure the string is a channel name */
	if (chan_name[0] != '#' || sizeof(chan_name) > CHAN_LEN) {
		fprintf(stderr, "PRGRM_NAME: %s is not a valid channel name\n", chan_name);
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
	send(fd, "irccd: Quitting", PIPE_BUF, 0);
	exit(0);
}

void login(int sockfd)
{
	char out[PIPE_BUF];
	sprintf(out, "USER %s 8 * :%s\r\nNICK %s\r\n", nick, realname, nick);
	send_msg(sockfd, out);
}

int main(int argc, char *argv[]) 
{
	/* 
	struct Ircmessage *msg = (Ircmessage*)malloc(sizeof(Ircmessage));
	char *mybuf = ": singularik!verssdasddsadadsdd@-bitch____baby PRIVMSG #Y35chan :my body is here";
	parse_msg(mybuf, msg);
	*/

	// Ignore sigpipes, they're handled internally
	signal(SIGPIPE, SIG_IGN);

	// TCP socket for irc server
	int host_sockfd = 0;

	// Name of channel last used to connect to, outgoing messages get sent here.
    char chan_name[CHAN_LEN];

	// Unix domain socket for clients to communicate with irccd
	int local_sockfd = 0;
	socket_bind(socketpath, &local_sockfd);

	// Message buffers for sending and recieving 
	char buf[PIPE_BUF], out[PIPE_BUF], message[PIPE_BUF];

	// Initialize channels
	channels = (Channel*)malloc(sizeof(Channel));

	// File descriptor for unix socket
	int fd, i;
	int pid = 0;

	char actmode = 0; /* Which mode to operate on */
	listen(local_sockfd, 5);
	while(1) {
		fd = accept(local_sockfd, NULL, NULL);
		memset(&buf, 0, PIPE_BUF);
		memset(&out, 0, PIPE_BUF);
		memset(&message, 0, PIPE_BUF);
		recv(fd, buf, PIPE_BUF, 0); 
		actmode = buf[0];
		fprintf(stdout, PRGNAME": mode: %c\n", actmode);

		for (i=0; i<PIPE_BUF-1; i++) {
			buf[i] = buf[i+1]; // Stores the message seperately from actcode
		}
		buf[PIPE_BUF-1]='\0';

		switch (actmode) {
		case JOIN_MOD:
			if (chan_check(buf) != 0) {
				break;
			}
			strcpy(chan_name, buf);
			sprintf(out, "JOIN %s\r\n", chan_name);
			if (send_msg(host_sockfd, out) > 0) {
				add_chan(buf);
			}
			break;
		case PART_MOD:
			strcpy(chan_name, buf);
			sprintf(out, "PART %s\r\n", chan_name);

			if (send_msg(host_sockfd, out) > 0) {
				rm_chan(chan_name);
			}
			break;
		case LIST_MOD:
			sprintf(out, "LIST %s\r\n", buf);
			send_msg(host_sockfd, buf);
			break;
		case LIST_CHAN_MOD:
			list_chan(message);
			break;
		case WRITE_MOD:
			sprintf(out, "PRIVMSG %s :%s\r\n", chan_name, buf);
			send_msg(host_sockfd, out);
			break;
		case NICK_MOD:
			if (strcmp(nick, buf) == 0) {
				fprintf(stdout, PRGNAME": nickname %s already in use "
					"by this client\n", nick);
				break;
			} 
			strcpy(nick, buf);
			sprintf(out, "NICK %s\r\n", nick);
			send_msg(host_sockfd, out);
			break;
		case CONN_MOD:
			// Test if we're already connected somewhere
			if (ping_host(host_sockfd, "") != -1) {
				sprintf(message, PRGNAME": already connected to %s\r\n", host_serv);
				fprintf(stderr, out);
			} else {
				strcpy(host_serv, buf);
				socket_connect(host_serv, &host_sockfd, port);
				// Forked process reads socket
				pid = fork_reader(host_sockfd);
				login(host_sockfd);
				sprintf(message, PRGNAME": connected to %s\r\n", host_serv);
			}
			break;
		case PING_MOD:
			ping_host(host_sockfd, buf);
			break;
		case DISC_MOD:
			if (host_sockfd != 0) {
				send_msg(host_sockfd, "QUIT :Bye bye!\r\n");
				close(host_sockfd);
				if (pid != 0) {
					kill(pid, SIGTERM); // Kills child reader
					pid = 0; // Set pid to zero so this won't kill main if invoked again
				}
			} else {
				fprintf(stderr, "Cannot disonnect socket: No connected socket\r\n");
			}
			host_sockfd = 0; // Zero signifies non-existent socket
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
		send(fd, message, sizeof(out), 0);
		close(fd);
	}
	quit_cmd(pid, fd);
}
