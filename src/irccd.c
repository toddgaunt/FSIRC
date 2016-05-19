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

static struct Channel *channels = NULL; // Head of the connected channels list
static char socketpath[_POSIX_PATH_MAX] = "/tmp/irccd.socket"; // Path to socket on filesystem
static char logpath[_POSIX_PATH_MAX] = "/tmp/irccd.log"; // Path to socket on filesystem
static int pid; // Process id

// Might change during operation
static char nick[NICK_LEN] = "iwakura_lain"; // User nickname
static char realname[NICK_LEN] = "Todd G."; // Real user name
static char host[PIPE_BUF] = "nothing"; // String containing server address
static unsigned short port = SERVER_PORT; // Port to connect with 

static void usage()
{
	fprintf(stderr, "irccd - irc client daemon - %s\n"
		"usage: irccd [-h] [-v] [-d]\n", VERSION);
	exit(EXIT_FAILURE);
}

static void quit(int pid)
{/* Quit the program and clean up child process with pid */
	if (pid != 0) {
		kill(pid, SIGTERM);
		pid = 0;
	}
	exit(EXIT_SUCCESS);
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

int send_msg(int *sockfd, char *buf)
{/* Send message to socket, max size is PIPE_BUF*/
	int n = send(*sockfd, buf, strnlen(buf, PIPE_BUF), 0);
	if (n > 0) {
		printf("irccd: out: %s", buf);
	} else {
		perror("irccd");
	}
	log_msg(buf);
	return n;
}

int read_line(int *sockfd, char *recvline, int len)
{/* Read chars from socket until a newline */
	int i = 0;
	char c;
	do {
		if (read(*sockfd, &c, sizeof(char)) != sizeof(char)) {
			perror("irccd: Unable to read socket");
			return -1;
		}
		recvline[i++] = c;
	} while (c != '\n' && i < len);
	recvline[i-1] = '\0'; // truncates message (usually ending in \n) with null byte
	printf("irccd: in: %s\n", recvline);
	log_msg(recvline);
	return 0;
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

int fork_reader(int *sockfd)
{/* Fork process to constantly read from irc socket */
	pid = fork(); 
	if (pid != 0) {
		return pid;
	}
	char recvline[PIPE_BUF];
	while(1) {
		if (read_line(sockfd, recvline, PIPE_BUF) != 0) {
			fprintf(stderr, "irccd(reader): Quit, connection dropped\n");
			exit(EXIT_FAILURE);
		} else { 
			if (recvline[1] == 'I') {
				recvline[1] = 'O'; // Swaps ping 'I' to 'O' in "PING"
				send_msg(sockfd, recvline);
			}
		}
		// If parent died (aka init is parent), exit
		if (getppid() == 1) exit(EXIT_FAILURE);
	}
}

int add_chan(char *chan_name)
{/* Add a channel to end of list */
	Channel *tmp = channels;
	while (tmp->next) {
		tmp = tmp->next;
		// If the named channel is already in the list, stop
		if (strcmp(tmp->name, chan_name) == 0) {
			return 1;
		}
	}
	tmp->next = (Channel *)calloc(1, sizeof(Channel)); 
	tmp = tmp->next; 
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
	printf("irccd: Could not remove channel %s\n", tmp->name);
	return -1;
}

int list_chan(char *message, int max_size)
{/* Modifies a string of all channels into message */
	memset(message, 0, max_size);
	Channel *tmp = channels;
	int len = 0;
	while (tmp && len < max_size - CHAN_LEN) {
		printf("%s\n", tmp->name);
		sprintf(message, "%s%s->", message, tmp->name);
		tmp = tmp->next;
		len = len + CHAN_LEN;
	}
	sprintf(message, "%s\n", message);
	return 0;
}

int chan_namecheck(char *name) 
{/* Performs checks to make sure the string is a channel name */
	char *p;
	int i; 
	if (name[0] != '#') {
		return 1;
	}
	for (p = name, i = 0; *p != '\0' && i < NICK_LEN; p++, i++);
	if (i >= NICK_LEN) return 2;
	return 0;
}

static int ping(int *sockfd, char *buf, int len) 
{/* Sends ping message to irc server */
	char out[len];
	snprintf(out, len-8, "PING: %s\r\n", buf);
	return send_msg(sockfd, out);
}

static void login(int *sockfd)
{/* Send login and user message to irc server. */
	int len = (NICK_LEN * 3) + 32;
	char buf[len];
	snprintf(buf, len, "USER %s 8 * :%s\r\nNICK %s\r\n", nick, realname, nick);
	send_msg(sockfd, buf);
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
	int i, j; // Looping ints
	int verbose = 0;

	// tcp socket for irc
	int tcpfd = 0;

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

	// Head channel
	channels = (Channel*)calloc(1, sizeof(Channel));

	// Communication socket for clients
	int unixfd;
	socket_bind(socketpath, &unixfd);

	// Message buffers
	char out[PIPE_BUF], buf[PIPE_BUF];

	// Message buffer for client communication
	char message[PIPE_BUF] = "";

	char actmode = 0; // Which mode to operate on
	int fd; // File descriptor for local socket and process id

	listen(unixfd, 5); // Start listening for connections
	// Main loop
	while(1) {
		fd = accept(unixfd, 0, 0);
		read_line(&fd, buf, PIPE_BUF); 
		actmode = buf[0]; // Store the beginning char which determines the switch
		fprintf(stdout, "irccd: mode: %c\n", actmode);

		for (i=0; i<sizeof(buf)-1; i++) {
			buf[i] = buf[i+1]; // Stores the message seperately from actcode
		}
		buf[i-1] = '\0'; // Messages are sent with a '\n' at the end, replace that newline with \0.

		switch (actmode) {
		case JOIN_MOD:
			if (chan_namecheck(buf) != 0) {
				snprintf(message, PIPE_BUF, "%s is not a valid channel name\n", buf);
				fprintf(stderr, "irccd %s", message);
				break;
			}
			snprintf(out, PIPE_BUF, "JOIN %s\r\n", buf);
			if (send_msg(&tcpfd, out) > 0) {
				add_chan(buf);
				snprintf(message, PIPE_BUF, "%s was joined successfully\n", buf);
				fprintf(stderr, "irccd: %s", message);
			} else {
				snprintf(message, PIPE_BUF, "%s was not added\n", buf);
				fprintf(stderr, "irccd: %s", message);
			}
			break;
		case PART_MOD:
			if (chan_namecheck(buf) != 0) {
				snprintf(message, PIPE_BUF, "%s is not a valid channel name\n", buf);
				fprintf(stderr, "irccd: %s", message);
				break;
			}
			snprintf(out, PIPE_BUF, "PART %s\r\n", buf);
			if (send_msg(&tcpfd, out) > 0) {
				rm_chan(buf);
				snprintf(message, PIPE_BUF, "%s was removed successfully\n", buf);
				fprintf(stderr, "irccd: %s", message);
			} else {
				snprintf(message, PIPE_BUF, "%s was not removed\n", buf);
				fprintf(stderr, "irccd: %s", message);
			}
			break;
		case LIST_CHAN_MOD:
			list_chan(message, PIPE_BUF);
			break;
		case WRITE_MOD:
			snprintf(out, PIPE_BUF, "PRIVMSG %s\r\n", buf);
			if (send_msg(&tcpfd, out)) {
				strcpy(message, "Message sent successfully\n");
				fprintf(stderr, "irccd: %s", message);
			} else {
				strcpy(message, "Message could not be sent\n");
				fprintf(stderr, "irccd: %s", message);
			}
			break;
		case NICK_MOD:
			if (strncmp(nick, buf, NICK_LEN) == 0) {
				fprintf(stdout, "irccd: Nickname %s already in use "
					"by this client\n", nick);
				break;
			} 
			snprintf(out, PIPE_BUF, "NICK %s\r\n", buf);
			if (send_msg(&tcpfd, out)) {
				strncpy(nick, buf, NICK_LEN);
				strcpy(message, "Nickname changed successfully\n");
				fprintf(stderr, "irccd: %s", message);
			} else {
				strcpy(message, "Nickname could not be changed\n");
				fprintf(stderr, "irccd: %s", message);
			}
			break;
		case CONN_MOD:
			if (ping(&tcpfd, "", PIPE_BUF) > 0) {
				snprintf(message, PIPE_BUF, "irccd: Already connected to %s\n", host);
				fprintf(stderr, message);
			} else {
				strncpy(host, buf, PIPE_BUF);
				if (socket_connect(host, &tcpfd, port) == 0) {
					fork_reader(&tcpfd);
					login(&tcpfd);
					snprintf(message, PIPE_BUF, "Connected to %s\n", host);
					fprintf(stderr, "irccd: %s", message);
				} else {
					snprintf(message, PIPE_BUF, "Could not connect to %s\n", host);
					fprintf(stderr, "irccd: %s", message);
				}
			}
			break;
		case PING_MOD:
			ping(&tcpfd, buf, PIPE_BUF);
			snprintf(message, PIPE_BUF, "Pinged host: %s\n", host);
			fprintf(stderr, "irccd: %s", message);
			break;
		case DISC_MOD:
			if (ping(&tcpfd, "", PIPE_BUF) >= 0) {
				send_msg(&tcpfd, "QUIT: irccd\r\n");
				close(tcpfd);
				strcpy(message, "Successfully disconnected socket\n");
				fprintf(stderr, "irccd: %s", message);
			} else {
				strcpy(message, "Cannot disconnect socket\n");
				fprintf(stderr, "irccd: %s", message);
			}
			break;
		case QUIT_MOD:
			quit(pid);
			break;
		default:
			strcpy(message, "Invalid command\n");
			fprintf(stderr, "irccd: %s", message);
			sleep(1);
			break;
		}
		// Finally send and free message

		send(fd, message, strnlen(message, PIPE_BUF), 0);
		close(fd);
	}
	quit(pid);
}
