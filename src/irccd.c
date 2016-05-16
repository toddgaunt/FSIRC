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
 * TODO MAKE STR BUFFERS SAFER, RIght now there are probably many buffer overflow 
 * attacks and str unsafey in general.
 */

#include <arpa/inet.h> 
#include <errno.h>
#include <malloc.h>
#include <netdb.h>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <unistd.h>
//#include <libconfig.h>//Use this to make config file
/* - - - - - - - */
#include "irccd.h"

static struct Channel *channels = NULL;          /* Head of the connected channels list */
static char host[IP_LEN] = "nothing";                        /* String containing server address */
static char socketpath[_POSIX_PATH_MAX] = "/tmp/irccd.socket";         /* Path to socket on filesystem */
static char logpath[_POSIX_PATH_MAX] = "/tmp/irccd.log";            /* Path to socket on filesystem */
static unsigned short port = SERVER_PORT;               /* Port to connect with */

// Might change during operation
static char nick[NICK_LEN] = "iwakura_lain";                      /* User nickname */
static char realname[NICK_LEN] = "Todd G.";                  /* User nickname */

static void usage()
{
	fprintf(stderr, "irccd - irc client daemon - %s\n"
		"usage: irccd [-h] [-p]\n", VERSION);
	exit(EXIT_FAILURE);
}

int log_msg(char *buf)
{/* Writes buf to a log file */
	FILE *fd = fopen(logpath, "a");
	if (fprintf(fd, "%s\n", buf) < 0) {
		return -1;
	}
	fclose(fd);
	return 0;
}

int send_msg(int sockfd, char *buf)
{/* Send message to socket, max size is PIPE_BUF*/
	int n = send(sockfd, buf, PIPE_BUF, 0);
	if (n > 0) {
		printf("irccd: out: %s", buf);
	} else {
		perror("irccd");
	}
	log_msg(buf);
	return n;
}

int read_line(int sockfd, char *recvline, int len)
{/* Read chars from socket until a newline */
	int i = 0;
	char c;
	do {
		if (read(sockfd, &c, sizeof(char)) != sizeof(char)) {
			perror("irccd: Unable to read socket");
			return -1;
		}
		recvline[i++] = c;
	} while (c != '\n' && i < len);
	recvline[i-1] = '\0'; // gets rid of the '\n'
	return 0;
}

int resolve_host(char *ip, char *hostname)
{/* Resolve hostnames to ip addresses */
	return -1;
}

int socket_connect(char *host, int *sockfd, int port)
{/* Construct socket at sockfd, then connect */
	struct sockaddr_in servaddr; 
	memset(&servaddr, 0, sizeof(struct sockaddr_in));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);

	// Define socket to be TCP
	*sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (*sockfd < 0) {
		perror("irccd: cannot create socket");
		return 1;
	}
	// Convert ip address to usable bytes
	if (inet_pton(AF_INET, host, &servaddr.sin_addr) <= 0) {
		perror("irccd: cannot transform ip");
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
	memset(&servaddr, 0, sizeof(struct sockaddr_un));
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
	if (pid != 0) {
		return pid;
	}
	char recvline[PIPE_BUF];
	while(1) {
		if (read_line(sockfd, recvline, PIPE_BUF) != 0) {
			fprintf(stderr, "irccd(reader): Quit, connection dropped\n");
			exit(EXIT_FAILURE);
		} else { 
			printf("irccd: in: %s\n", recvline);
			log_msg(recvline);
			if (recvline[1] == 'I') {
				recvline[1] = 'O'; // Swaps ping 'I' to 'O' in "PING"
			}
		}
	}
}

int add_chan(char *chan_name)
{/* Add a channel to end of list */
	Channel *tmp = channels;
	while (tmp->next) {
		tmp = tmp->next;
		if (strcmp(tmp->name, chan_name) == 0) {
			return 1;
		}
	}
	tmp = tmp->next; 
	tmp = (Channel *)malloc(sizeof(Channel)); 
	if (!tmp) {
		printf("irccd: Cannot allocate memory");
		return -1;
	}
	tmp->name = strdup(chan_name); // mallocs then strcpy
	tmp->next = NULL;

	return 0;
}

int rm_chan(char *chan_name)
{/* Remove an entry with chan_name from list */
	Channel *garbage;
	Channel *tmp = channels;
	while (tmp->next) {
		if (strcmp(tmp->next->name, chan_name) == 0) {
			garbage = tmp->next; 
			tmp->next = tmp->next->next;

			printf("irccd: channel removed: %s\n", garbage->name);

			free(garbage->name);
			free(garbage);
			return 0;
		}
		tmp = tmp->next;
	}
}

void list_chan(char *buf, int len)
{/* Puts a string of all channels into *buf */
	memset(buf, 0, len);
	Channel *tmp = channels;
	for (tmp = channels; tmp; tmp = tmp->next) {
		// Add another name only if buf can hold another channel name
		if (strlen(buf) < len - CHAN_LEN - 1) {
			sprintf(buf, "%s%s->", buf, tmp->name);
		} else break;
	}
	sprintf(buf, "%s\n", buf);
}

int chan_namecheck(const char *name) 
{/* Performs checks to make sure the string is a channel name */
	if (name[0] != '#') {
		fprintf(stderr, "irccd: %s is not a valid channel name\n", name);
		return 1;
	}
	return 0;
}

static int ping(int sockfd, char *buf) 
{
	// Uses static msg buffer
	char out[PIPE_BUF];
	snprintf(out, PIPE_BUF, "PING: %s\r\n", buf);
	return send_msg(sockfd, out);
}

static void login(int sockfd)
{
	// Uses static msg buffer
	int size = (NICK_LEN * 3) + 32;
	char buf[size];
	snprintf(buf, size, "USER %s 8 * :%s\r\nNICK %s\r\n", nick, realname, nick);
	send_msg(sockfd, buf);
}

void quit_cmd(int pid)
{
	if (pid != 0) {
		kill(pid, SIGTERM);
		pid = 0;
	}
	exit(EXIT_SUCCESS);
}

int handle_client_input() {
	return -1;
}

int daemonize() 
{/* Fork the process, detach from terminal, and redirect std streams */
	switch (fork()) { 
	case -1:
		return -1;
	case 0: 
		break;
	default: 
		exit(EXIT_SUCCESS);
	}

	if (setsid() < 0) return -1;
	if (chdir("/") < 0) return -1;

	int devnull = open(PATH_DEVNULL, O_RDWR);
	if (devnull < 0) return -1;
	dup2(devnull, STDIN_FILENO);
	dup2(devnull, STDOUT_FILENO);
	dup2(devnull, STDERR_FILENO);
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
	return 0;
}

int main(int argc, char *argv[]) 
{
	int i, j;
	int verbose = 0;

	if (argc > 1) {
		for(i = 1; (i < argc) && (argv[i][0] == '-'); i++) {
			switch (argv[i][1]) {
			case 'h': usage(); break;
			case 'v': for (j = 1; argv[i][j] && argv[i][j] == 'v'; j++) verbose++; break;
			case 'p': port = strtol(argv[++i], NULL, 10); break;
			case 'd': daemonize(); break;
			default: usage();
			}
		}
	}
	// Ignore sigpipes, they're handled internally
	signal(SIGPIPE, SIG_IGN);

	// Name of channel last used to connect to, outgoing messages get sent here.
    char chan_name[CHAN_LEN] = "#Y35chan";

	// Message buffers for sending and recieving 
	char message[PIPE_BUF];
	char buf[PIPE_BUF];
	char out[PIPE_BUF]; // Buffer for when we want to preserve buf
	char *ping_msg = "irccd";

	// sockets
	int tcpfd;  // tcp for irc
	int unixfd; // unix for clients
	socket_bind(socketpath, &unixfd);

	// Head channel
	channels = (Channel*)malloc(sizeof(Channel));

	// File descriptor for unix socket
	int fd;
	int pid = 0;

	char actmode = 0; /* Which mode to operate on */
	listen(unixfd, 5); // Start listening for connections
	while(1) {
		fd = accept(unixfd, 0, 0);
		read_line(fd, buf, PIPE_BUF); 
		actmode = buf[0];
		fprintf(stdout, "irccd: mode: %c\n", actmode);

		for (i=0; i<sizeof(buf)-1; i++) {
			buf[i] = buf[i+1]; // Stores the message seperately from actcode
		}
		buf[i-1] = '\0';
		switch (actmode) {
		case JOIN_MOD:
			if (chan_namecheck(buf) != 0) {
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
			if (chan_namecheck(buf) != 0) {
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
				fprintf(stdout, "irccd: Nickname %s already in use "
					"by this client\n", nick);
				break;
			} 
			strcpy(nick, buf);
			snprintf(out, sizeof(out), "NICK %s\r\n", nick);
			send_msg(tcpfd, out);
			break;
		case CONN_MOD:
			// Test if we're already connected somewhere
			if (ping(tcpfd, ping_msg) > 0) {
				snprintf(message, sizeof(message), "irccd: Already connected to %s\n", host);
				fprintf(stderr, message);
			} else {
				strcpy(host, buf);
				printf("connectpls\n");
				if (socket_connect(host, &tcpfd, port) != 0) {
					break;
				}
				pid = fork_reader(tcpfd);
				login(tcpfd);
				snprintf(message, sizeof(message), "irccd: Connected to %s\n", host);
			}
			break;
		case PING_MOD:
			ping(tcpfd, ping_msg);
			break;
		case DISC_MOD:
			if (!tcpfd) {
				send_msg(tcpfd, "QUIT: irccd\r\n");
				if (!pid) {
					kill(pid, SIGTERM); // Kills child reader
					pid = 0; // Set pid to zero so this won't kill main if invoked again
				}
				close(tcpfd);
			} else {
				fprintf(stderr, "irccd: Cannot disonnect socket: No connected socket\n");
			}
			tcpfd = 0; // Zero signifies non-existent socket
			break;
		case QUIT_MOD:
			quit_cmd(pid);
			break;
		default:
			printf("irccd: Invalid command\n");
			sleep(1);
			break;
		}
		// Sends result of commands back to client
		send(fd, message, sizeof(message), 0);
		close(fd);
	}
	quit_cmd(pid);
}
