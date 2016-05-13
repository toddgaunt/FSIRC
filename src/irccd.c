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
#include <fcntl.h>
#include <malloc.h>
#include <netdb.h>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <unistd.h>
//#include <libconfig.h>//Use this to make config file
/* - - - - - - - */
#include "irccd.h"

static void usage()
{
	fprintf(stdout, "irccd - irc client daemon - " VERSION "\n"
	"usage: irccd [-h] [-d]\n");
}

int send_msg(int sockfd, char *out)
{/* Send message with debug output to socket */
	int n = send(sockfd, out, strlen(out), MSG_NOSIGNAL);
	if (n <= 0) {
		perror(PRGNAME": unable to send message");
	} else {
		fprintf(stdout, PRGNAME": out: %s", out);
	}
	return n;
}

int read_msg(int sockfd, char *recvline)
{/* Read next message from socket, replies to any PING with a PONG */
	char *pos, out[PIPE_BUF];
	int n = recv(sockfd, recvline, PIPE_BUF, 0);
	if (n <= 0) {
		perror(PRGNAME": unable to recieve message");
	} else {
		fprintf(stdout, PRGNAME": in: %s", recvline);
	}
	memset(&out, 0, PIPE_BUF);
	if (n > 0 && strstr(recvline, "PING") != NULL) {
		pos = strstr(recvline, " ")+1;
		sprintf(out, "PONG %s\r\n", pos);
		send_msg(sockfd, out);
	}
	return n;
}

int sock_conn(char *host, int *host_sockfd)
{/* Construct socket at host_sockfd, then connect */
	struct sockaddr_in servaddr; 
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(IRCCD_PORT);
	
	// Modifies the host_sockfd for the rest of main() to use
	*host_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (*host_sockfd < 0) {
		perror(PRGNAME": cannot create socket");
		return 1;
	}
	if (inet_pton(AF_INET, host, &servaddr.sin_addr) <= 0) {
		perror(PRGNAME": cannot transform ip");
		return 1;
	}
	// Points sockaddr pointer to servaddr because connect takes sockaddr structs
	if (connect(*host_sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
		perror(PRGNAME": unable to connect socket");
		return 1;
	}

	return 0;
}

int host_disc(int *sockfd)
{/* Disconnects from irc gracefully, then close the socket. */
	send_msg(*sockfd, "QUIT\r\n");
	return close(*sockfd);
}

int bind_sock(char *path, int *sockfd)
{/* Contruct a unix socket for inter-process communication, then connect */
	struct sockaddr_un servaddr; 
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sun_family = AF_UNIX;
	strcpy(servaddr.sun_path, path);

	// modifies local socket for unix communication 
	*sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (*sockfd < 0) {
		perror(PRGNAME": cannot create socket");
		return 1;
	}
	unlink(path);
	if (bind(*sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
		perror(PRGNAME": unable to bind socket");
		return 1;
	}
	return 0;
}

int spawn_reader(int sockfd)
{/* Fork process to constantly read from irc socket */
	int timer = 0;
	int pid = fork(); 
	char recvline[PIPE_BUF];
	if (pid == 0) {
		while(1) {
			memset(&recvline, 0, PIPE_BUF);
			if (read_msg(sockfd, recvline) <= 0) {
				timer += 1;
				if (timer >= PING_TIMEOUT) {
					exit(1); // kill the reader if read attempt fails after PING_TIMEOUT (300 seconds)
				}
				sleep(1);
			} else { 
				timer = 0;
			}
		}
	}
	return pid;
}

int parse_msg(char *buf)
{/* Parses buf into an IrcMessage struct */
	return 1;
}

int log_msg(char *buf)
{/* Writes buf to a log file corresponding to channel message was in */
	return 1;
}

int add_chan(Channel *head, char *chan_name)
{/* Add a channel to end of a list */
	Channel *tmp = head;
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

	fprintf(stdout, PRGNAME": channel added: %s\n", tmp->name);
	return 0;
}

int rm_chan(Channel *head, char *chan_name)
{/* Remove an entry with name = chan_name from list */
	Channel *garbage;
	Channel *tmp = head;
	while (tmp->next != NULL) {
		if (strcmp(tmp->next->name, chan_name) != 0) {
			tmp = tmp->next;
		} else {
			garbage = tmp->next; 
			tmp->next = tmp->next->next;

			fprintf(stdout, PRGNAME": channel removed: %s\n", garbage->name);

			free(garbage->name);
			free(garbage);
			return 0;
		}
	}
	fprintf(stdout, PRGNAME": channel %s not found in list\n", chan_name);
	return 1;
}

int list_chan(Channel *head, char *buf)
{/* Pretty Prints all channels client is connected to */
	Channel *node = head;
	if (node != NULL) {
		//sprintf(buf, "%s->", node->name);
		while (node->next != NULL) {
			node = node->next;
			sprintf(buf, "%s%s->", buf, node->name);
		}
		sprintf(buf, "%s\n", buf);
		return 1;
	}
	return 0;
}

int ping_host(int sockfd, char *msg)
{/* Sends a ping message to a socket */
	char out[PIPE_BUF];
	sprintf(out, "PING %s\r\n", msg);
	return send_msg(sockfd, out);
}

int send_login(int sockfd, char *nick) 
{/* Sends out login information to the host */
	char out[PIPE_BUF];
	sprintf(out, "NICK %s\r\n", nick);
	sprintf(out, "%sUSER %s 8 * :nick\r\n", out, nick);
	send_msg(sockfd, out);
	return 0;
}

int chan_check(char *chan_name) 
{/* Performs checks to make sure the string is a channel name */
	if (chan_name[0] != '#' || sizeof(chan_name) > CHAN_LEN) {
		fprintf(stdout, "PRGRM_NAME: %s is not a valid channel name\n", chan_name);
		return 1;
	}
	return 0;
}

int main(int argc, char *argv[]) 
{
	usage();
	char *sockpath = "/tmp/irccd.socket";
	char host_serv[IP_LEN];
	char actmode = 0;

	// User info
	char *nick = "iwakura_lain";

	int host_sockfd = 0; // fd used for internet socket

	// Channel pointers
	Channel *channels = (Channel*)malloc(sizeof(Channel)); // List of channels
    char chan_name[CHAN_LEN];

	int local_sockfd = 0; // socket used for client to talk to irccd (also forked processes)
	bind_sock(sockpath, &local_sockfd);

	// Message buffers for sending and recieving 
	char buf[PIPE_BUF], out[PIPE_BUF];

	// File descriptor and for loop counter
	int fd, i;
	int pid = 0;

	listen(local_sockfd, 5);
	while(1) {
		fd = accept(local_sockfd, NULL, NULL);
		memset(&buf, 0, PIPE_BUF);
		memset(&out, 0, PIPE_BUF);
		recv(fd, buf, PIPE_BUF, 0); 
		actmode = buf[0];
		fprintf(stdout, PRGNAME": mode: %c\n", actmode);

		for (i = 0; i < sizeof(buf)-1; i++) {
			buf[i] = buf[i+1]; // Stores the message seperately from actcode
		}
		buf[PIPE_BUF-1]='\0';

		switch (actmode) {
		case JOIN_MOD:
			if (chan_check(buf)) {
				break;
			}
			strcpy(chan_name, buf);
			sprintf(out, "JOIN %s\r\n", chan_name);
			if (send_msg(host_sockfd, out) > 0) {
				add_chan(channels, buf);
			}
			break;
		case PART_MOD:
			strcpy(chan_name, buf);
			sprintf(out, "PART %s\r\n", chan_name);

			if (send_msg(host_sockfd, out) > 0) {
				rm_chan(channels, chan_name);
			}
			break;
		case LIST_MOD:
			sprintf(out, "LIST %s\r\n", buf);
			send_msg(host_sockfd, buf);
			break;
		case LIST_CHAN_MOD:
			list_chan(channels, buf);
			send(fd, buf, PIPE_BUF, 0);
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
			if (send_msg(host_sockfd, "PING\r\n") != -1) {
				sprintf(out, PRGNAME": already connected to %s\n", host_serv);
				fprintf(stderr, out);
			} else {
				strcpy(host_serv, buf);
				sock_conn(host_serv, &host_sockfd);
				// Forked process reads socket
				pid = spawn_reader(host_sockfd);
				send_login(host_sockfd, nick);
				sprintf(out, PRGNAME": connected to %s\n", host_serv);
			}
			send(fd, out, PIPE_BUF, 0);
			break;
		case PING_MOD:
			ping_host(host_sockfd, buf);
			break;
		case DISC_MOD:
			if (host_sockfd != 0) {
				host_disc(&host_sockfd);
				if (pid != 0) {
					kill(pid, SIGTERM); // Kills child reader
					pid = 0; // Set pid to zero so this won't kill main if invoked again
				}
			} else {
				fprintf(stderr, "Cannot disonnect socket: No connected socket\n");
			}
			host_sockfd = 0; // Zero signifies non-existent socket
			break;
		case QUIT_MOD:
			if (pid != 0) {
				kill(pid, SIGTERM);
				pid = 0;
			}
			exit(0);
			break;
		default:
			printf("NO COMMAND\n");
			sleep(1);
			break;
		}
		close(fd);
	}
	perror("ULTIMATE DEATH");
	if (pid != 0) {
		kill(pid, SIGTERM);
	}
	exit(1);
}
