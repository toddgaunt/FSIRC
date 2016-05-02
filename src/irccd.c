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

#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h> 
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <malloc.h>
//#include <libconfig.h>//Use this to make config file
/* - - - - - - - */
#include "irccd.h"

static char *myfifo = "/tmp/irccd.fifo";
static char *mypath = "/tmp/irccd.socket";
static char nick[NICK_LEN] = "iwakura_lain";
static char host_serv[CHAN_LEN] = "162.213.39.42";
static unsigned int port = 6667;

int send_msg(int sockfd, char *out)
{/* Send message with debug output to socket */
	int n = send(sockfd, out, strlen(out), 0);
	if (n <= 0) {
		fprintf(stderr, "%s: error: message sending error\n", PRGM_NAME);
	} else {
		fprintf(stderr, "%s: out: %s", PRGM_NAME, out);
	}
	return n;
}

int read_msg(int sockfd, char *recvline)
{/* Read next message from socket, replies to any PING with a PONG */
	char *pos, out[MAX_BUF];
	int n = read(sockfd, recvline, MAX_BUF);
	if (n > 0) {
		fprintf(stdout, "%s: in: %s", PRGM_NAME, recvline);
	}
	memset(&out, 0, sizeof(out));
	if (n > 0 && strstr(recvline, "PING") != NULL) {
		pos = strstr(recvline, " ")+1;
		sprintf(out, "PONG %s\r\n", pos);
		send_msg(sockfd, out);
	}
	return n;
}

int host_conn(char *host, unsigned int port, int *host_sockfd)
{/* Construct socket at host_sockfd, then connect */
	struct sockaddr_in servaddr; 
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	
	// modifies the host_sockfd for the rest of main() to use
	*host_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (*host_sockfd < 0) {
		fprintf(stdout, "%s: socket creation error\n", PRGM_NAME);
		return 1;
	}
	if (inet_pton(AF_INET, host, &servaddr.sin_addr) <= 0) {
		fprintf(stderr, "%s: error: invalid network address error\n", PRGM_NAME);
		return 1;
	}
	// points sockaddr pointer to servaddr because connect takes sockaddr structs
	if (connect(*host_sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
		return 1;
	}

	return 0;
}

int host_disc(int *host_sockfd)
{/* Disconnect socket and then remove it */
	send_msg(*host_sockfd, "QUIT\r\n");
	return shutdown(*host_sockfd, SHUT_RDWR);
}

int local_bind(char *path, int *local_sockfd)
{/* Contruct a unix socket for inter-process communication, then connect */
	struct sockaddr_un servaddr; 
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sun_family = AF_UNIX;
	strcpy(servaddr.sun_path, path);

	// modifies local socket for unix communication 
	*local_sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (*local_sockfd < 0) {
		fprintf(stderr, "%s: error: socket creation error\n", PRGM_NAME);
		return 1;
	}
	if (bind(*local_sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
		return 1;
	}
	return 0;
}

int spawn_reader(int sockfd)
{/* Fork process to constantly read from irc socket */
	int timer = 0;
	int pid = fork(); 
	char recvline[MAX_BUF];
	if (pid == 0) {
		while(1) {
			memset(&recvline, 0, sizeof(recvline));
			if (read_msg(sockfd, recvline) <= 0) {
				timer += 1;
				sleep(1);
				fprintf(stderr, "%s: reader error (%d)\n", PRGM_NAME, timer);
			} else { 
				timer = 0;
			}
			if (timer >= 10) {
				exit(1); // kill the reader if socket connection fails after 10 tries
			}
		}
	}
	return pid;
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
		fprintf(stderr, "%s: error: out of memory\n", PRGM_NAME);
		return -1;
	}
	tmp->name = strdup(chan_name);
	tmp->next = NULL;

	fprintf(stdout, "%s: channel added: %s\n",PRGM_NAME, tmp->name);
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

			fprintf(stdout, "%s: channel removed: %s\n", PRGM_NAME, garbage->name);

			free(garbage->name);
			free(garbage);
			return 0;
		}
	}
	fprintf(stdout, "%s: channel %s not found in list\n", PRGM_NAME, chan_name);
	return 1;
}

// Currently unused
int printchan(char *buf, Channel *head)
{/* Pretty Prints all channels client is connected to 
  * Later should write channels to a log file */
	Channel *node = head;
	if (node != NULL) {
		sprintf(buf, "%s->", node->name);
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
	char out[MAX_BUF];
	sprintf(out, "PING %s\r\n", msg);
	return send_msg(sockfd, out);
}

int sendlogin(int sockfd, char *nick, char *realname) 
{/* Sends out login information to the host */
	char out[MAX_BUF];
	sprintf(out, "NICK %s\r\nUSER %s 8 * :nick\r\n", nick, nick);
	send_msg(sockfd, out);
	return 0;
}

int chan_name_check(char *chan_name) 
{/* Performs checks to make sure the string is a channel name */
	if (chan_name[0] != '#' || sizeof(chan_name) > CHAN_LEN) {
		fprintf(stdout, "%s: %s is not a valid channel\n", PRGM_NAME, chan_name);
		return 1;
	}
	return 0;
}

int main(int argc, char *argv[]) 
{
	// Channel list pointers
	Channel *chan_head = (Channel*)malloc(sizeof(Channel)); // head of the list
	Channel *current_channel = NULL; // used to store last visted node
    char chan_name[CHAN_LEN];

	// sockets
	int host_sockfd = 0; //socket used for irccd to connect to host_serv
	int local_sockfd = 0; // socket used for client to talk to irccd (also forked processes)
	local_bind(mypath, &local_sockfd);

	// Defines the action to be performed
	char actmode = 0;
	int pid = 0;
	
	// Message buffers for sending and recieving 
	char buf[MAX_BUF], out[MAX_BUF], pos[MAX_BUF];

	mkfifo(myfifo, 0666);
	int fd, i;
	while(1) {
		fd = open(myfifo, O_RDONLY); 
		memset(&buf, 0, sizeof(buf));
		read(fd, buf, sizeof(buf));
		actmode = buf[0];

		fprintf(stdout, "%s: mode: %c\n", PRGM_NAME, actmode);

		for (i = 0; i < sizeof(buf)-1; i++) {
			pos[i] = buf[i+1]; // Stores the message seperately from actcode
		}
		close(fd);

		switch (actmode) {
			case JOIN_MOD:
				if (chan_name_check(pos)) {
					break;
				}
				strcpy(chan_name, pos);
				sprintf(out, "JOIN %s\r\n", chan_name);

				if (send_msg(host_sockfd, out) > 0) {
					add_chan(chan_head, pos);
				}
				break;
			case PART_MOD:
				strcpy(chan_name, pos);
				sprintf(out, "PART %s\r\n", chan_name);

				if (send_msg(host_sockfd, out) > 0) {
					rm_chan(chan_head, chan_name);
				}
				break;
			case LIST_MOD:
				sprintf(out, "LIST %s\r\n", pos);
				send_msg(host_sockfd, out);
				break;
			case WRITE_MOD:
				sprintf(out, "PRIVMSG %s :%s\r\n", chan_name, pos);
				send_msg(host_sockfd, out);
				break;
			case NICK_MOD:
				if (strcmp(nick, pos) == 0) {
					fprintf(stdout, "%s: nickname %s already in use "
									"by this client\n", PRGM_NAME, nick);
					break;
				} 
				strcpy(nick, pos);
				sprintf(out, "NICK %s\r\n", nick);
				send_msg(host_sockfd, out);
				break;
			case CONN_MOD:
				// Test if we're already connected somewhere
				if (strcmp(host_serv, pos) == 0 && send_msg(host_sockfd, "PING\r\n") != -1) {
					fprintf(stdout, "%s: server %s already connected to.\n", PRGM_NAME, host_serv);
				} else {
					strcpy(host_serv, pos);
					if (host_conn(host_serv, port, &host_sockfd) != 0) {
						fprintf(stderr, "%s: error: connection failed\n", PRGM_NAME);
						exit(1);
					}
					sprintf(out, "NICK %s\r\n", nick);
					send_msg(host_sockfd, out);
					sprintf(out, "USER %s 8 * :nick\r\n", nick);
					send_msg(host_sockfd, out);
					// Seperate process reads socket
					pid = spawn_reader(host_sockfd);
				}
				break;
			case PING_MOD:
				ping_host(host_sockfd, pos);
				break;
			case DISC_MOD:
				host_disc(&host_sockfd);
				break;
			case QUIT_MOD:
				kill(pid, SIGTERM);
				exit(0);
			default:
				printf("NO COMMAND\n");
				sleep(1);
		}
	}
	printf("Failed to fork\n");
	kill(pid, SIGTERM);
	exit(1);
}
