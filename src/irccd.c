/*
 * Name: irccd
 * Author: Todd Gaunt
 * Last updated: 2016-05-22
 * License: MIT
 * Description:
 * irccd is a simple irc-bot daemon
 * it will auto-save messages for you
 * and can be called from the command line
 * to send messages to the specified channel.
 *
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

// Increase this for debugging output
static int debug = 0;

static struct Channel *channels = NULL; // Head of the connected channels list
static char socketpath[_POSIX_PATH_MAX] = "/tmp/irccd.socket"; // Path to socket on filesystem
static char logpath[_POSIX_PATH_MAX] = "/tmp/irccd.log"; // Path to socket on filesystem
static int verbose = 1; // Verbosity count
static int pid; // Process id

// Might change during operation
static char nick[NICK_LEN+1] = "iwakura_lain"; // User nickname (+1 for '\0')
static char realname[NICK_LEN+1] = "Todd G."; // Real user name (+1 for '\0')
static char host[IRC_BUF_MAX] = "nothing"; // String containing server address
static unsigned short port = SERVER_PORT; // Port to connect with 

static void usage()
{
	fprintf(stderr, "irccd - irc client daemon - %s\n"
		"usage: irccd [-h] [-v] [-d]\n", VERSION);
	exit(EXIT_FAILURE);
}

static void quit()
{/* Quit the program and clean up child process with pid */
	if (pid != 0) {
		kill(pid, SIGTERM);
		pid = 0;
	}
	exit(EXIT_SUCCESS);
}

int log_msg(char *buf)
{/* Writes buf to a log file, expects buf to not include any newline characters */
	FILE *fd = fopen(logpath, "a");
	if (fprintf(fd, "%s\n", buf) < 0) {
		return -1;
	}
	fclose(fd);
	return 0;
}

int send_msg(int *sockfd, char *buf, int len)
{/* Send message to socket, max size is IRC_BUF_MAX, last two chars must be \r and \n 
  * or they will be replaced */
	if (len > IRC_BUF_MAX) return 1;
	if (buf[len-1] != '\n' || buf[len-2] !='\r') {
		buf[len -2] = '\r';
		buf[len-1] = '\n';
	}
	int n = send(*sockfd, buf, len, 0);
	if (n > 0) {
		fprintf(stdout, "irccd: out: %s", buf);
	} else {
		perror("irccd");
	}
	// Remove all newline chars "\r\n"
	buf[len-1] = '\0';
	buf[len-2] = '\0';
	log_msg(buf);
	return n;
}

int read_line(int *sockfd, char *recvline, int len)
{/* Read chars from socket until a newline or 510 chars have been read */
	int i = 0;
	char c;
	do {
		if (recv(*sockfd, &c, sizeof(char), 0) != sizeof(char)) {
			perror("irccd: Unable to read socket");
			return -1;
		}
		recvline[i++] = c;
	} while (c != '\n' && i < len - 2); // -2 because we need to save room for "\r\n"
	recvline[--i] = '\0'; // truncates message ending in '\n' with null byte
	if (recvline[i-1] == '\r') {
		recvline[--i] = '\0'; // If there is a carriage return, also make it null
	}
	fprintf(stdout, "irccd: in: %s\n", recvline);
	log_msg(recvline);
	return i; // Length of the string not including '\0' eg. "read\0" has a length of 4
}

int fork_reader(int *sockfd, int *unixfd)
{/* Fork process to constantly read from irc socket */
	pid = fork(); 
	if (pid != 0) {
		return pid;
	}
	int recvlen;
	char recvline[IRC_BUF_MAX];
	while(1) {
		recvlen = read_line(sockfd, recvline, IRC_BUF_MAX);
		if(recvlen == -1) {
			fprintf(stderr, "irccd(reader): Quit, connection dropped\n");
			exit(EXIT_FAILURE);
		}
		// PONG the server
		if (recvline[1] == 'I') {
			recvline[1] = 'O'; // Swaps ping 'I' to 'O' in "PING"
			// read_line() leaves two bytes after the end of a string for \r\n
			recvlen += 2;
			recvline[recvlen-2] = '\r';
			recvline[recvlen-1] = '\n';
			send_msg(sockfd, recvline, recvlen);
		}
		// If parent died (aka init is parent), exit
		if (getppid() == 1) exit(EXIT_FAILURE);
	}
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
		fprintf(stderr, "irccd: cannot transform ip\n");
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

int add_chan(char *name)
{/* Add a channel to end of list */
	Channel *tmp = channels;
	while (tmp->next) {
		tmp = tmp->next;
		// If the named channel is already in the list, stop
		if (strcmp(tmp->name, name) == 0) {
			return 1;
		}
	}
	tmp->next = (Channel *)malloc(sizeof(Channel)); 
	tmp = tmp->next; 
	if (!tmp) {
		printf("irccd: Cannot allocate memory");
		return -1;
	}
	tmp->name = strdup(name); // mallocs then strcpy
	tmp->next = NULL;

	return 0;
}

int part_chan(char *name)
{/* Remove an entry with name from list */
	Channel *garbage;
	Channel *tmp = channels;
	while (tmp->next) {
		if (strcmp(tmp->next->name, name) == 0) {
			garbage = tmp->next; 
			tmp->next = tmp->next->next;

			free(garbage->name);
			free(garbage);
			return 0;
		}
		tmp = tmp->next;
	}
	return 1;
}

int list_chan(char *message, int max_size)
{/* Modifies a string of all channels into message */
	memset(message, 0, max_size);
	Channel *tmp = channels;
	int len = 0;
	while (tmp && len < max_size - CHAN_LEN-1) {
		len = snprintf(message, max_size, "%s%s->", message, tmp->name);
		tmp = tmp->next;
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
	if (len < IRC_BUF_MAX - 6) {
		len += 8;
	}
	char out[len];
	len = snprintf(out, len, "PING :%s", buf);
	return send_msg(sockfd, out, len);
}

static void login(int *sockfd)
{/* Send login and user message to irc server. */
	int len = (NICK_LEN * 3) + 20;
	char buf[len];
	len = snprintf(buf, len, "USER %s 8 * :%s\r\nNICK %s\r\n", nick, realname, nick);
	send_msg(sockfd, buf, len);
}

static int host_connect(int *tcpfd, int *unixfd, char *buf)
{/* Open tcp socket connection, and fork reader process */
	strncpy(host, buf, IRC_BUF_MAX);
	// Change the root channel's name to the new host
	free(channels->name);
	channels->name = strdup(host);
	if (socket_connect(host, tcpfd, port) == 0) {
		fork_reader(tcpfd, unixfd);
		login(tcpfd);
		return 0;
	}
	return 1;
}

static void could_not_send(char *message, int len) {
	strncpy(message, "message could not be sent\n", len);
	fprintf(stderr, "irccd: %s", message);
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
	// Ignore sigpipes, they're handled internally
	signal(SIGPIPE, SIG_IGN);

	// Head channel
	channels = (Channel*)malloc(sizeof(Channel));
	channels->name = strdup(host);

	// Looping ints
	int i, j;

	// tcp socket for irc
	int tcpfd = 0;

	// Communication socket for clients
	int unixfd;
	socket_bind(socketpath, &unixfd);

	// Recving buffer from client
	char recvline[IRC_BUF_MAX];
	short recvlen;

	// Message buffers. out for irc,  message for client
	char message[IRC_BUF_MAX];
	short msglen;

	char out[IRC_BUF_MAX];

	if (argc > 1) {
		for(i = 1; (i < argc) && (argv[i][0] == '-'); i++) {
			switch (argv[i][1]) {
			case 'h': usage(); break; // help message
			case 'v': for (j = 1; argv[i][j] && argv[i][j] == 'v'; j++) {
						  verbose++; 
					  }
					  break;
			case 'q': for (j = 1; argv[i][j] && argv[i][j] == 'q'; j++) {
						  if (verbose > 0) verbose--; 
					  }
					  break;
			case 'p': port = strtol(argv[++i], NULL, 10); break; // daemon port
			case 'n': strncpy(nick, argv[++i], NICK_LEN); break; // daemon port
			case 'd': daemonize(); break; // daemonize the process
			case 'c': host_connect(&tcpfd, &unixfd, argv[++i]); break; // irc server host
			default: usage();
			}
		}
	}

	char actmode = 0; // Which mode to operate on
	int fd; // File descriptor for local socket

	listen(unixfd, 5); // Start listening for connections
	// Main loop
	while(1) {
		fd = accept(unixfd, 0, 0);
		recvlen = read_line(&fd, recvline, IRC_BUF_MAX-2); // -2 because room is needed for \r\n
		if (recvlen == -1) {
			fprintf(stderr, "irccd: error recieving message from client\n");
			continue;
		}
		if (debug) printf("recvline: %s, recvlen: %d, i:%d, %c\n", recvline, recvlen, i, recvline[recvlen]);
		// Actmode determines which action the loop should do
		actmode = recvline[0];
		for (i=0; i<recvlen; i++) {
			recvline[i] = recvline[i+1]; // Stores the message seperately from actcode
		}
		// Shorten the string
		recvline[--recvlen] = '\0';
		// temp for debugging
		if (debug) printf("recvline: %s, recvlen: %d, i:%d, %c\n", recvline, recvlen, i, recvline[recvlen]);

		switch (actmode) {
		case JOIN_MOD:
			if (chan_namecheck(recvline) != 0) {
				msglen = snprintf(message, IRC_BUF_MAX, "Invalid channel name\n");
				fprintf(stderr, "irccd %s", message);
				break;
			}
			recvlen = snprintf(out, IRC_BUF_MAX, "JOIN %s\r\n", recvline);
			if (send_msg(&tcpfd, out, recvlen) > 0) {
				if (add_chan(recvline) == 1){
					msglen = snprintf(message, IRC_BUF_MAX, "Channel already joined\n");
					fprintf(stderr, "irccd: %s", message);
					break;
				}
				msglen = snprintf(message, IRC_BUF_MAX, "JOIN message sent\n");
				fprintf(stderr, "irccd: %s", message);
			} else {
				could_not_send(message, IRC_BUF_MAX);
			}
			break;
		case PART_MOD:
			if (chan_namecheck(recvline) != 0) {
				msglen = snprintf(message, IRC_BUF_MAX, "Invalid channel name\n");
				fprintf(stderr, "irccd %s", message);
				break;
			}
			recvlen = snprintf(out, IRC_BUF_MAX, "PART %s\r\n", recvline);
			if (send_msg(&tcpfd, out, recvlen) > 0) {
				if (part_chan(recvline) == 1){
					msglen = snprintf(message, IRC_BUF_MAX, "Channel not joined, could not \
					part from %s\n", recvline);
					fprintf(stderr, "irccd: %s", message);
					break;
				}
				msglen = snprintf(message, IRC_BUF_MAX, "PART message sent\n");
				fprintf(stderr, "irccd: %s", message);
			} else {
				could_not_send(message, IRC_BUF_MAX);
			}
			break;
		case LIST_CHAN_MOD:
			list_chan(message, IRC_BUF_MAX);
			break;
		case WRITE_MOD:
			recvlen = snprintf(out, IRC_BUF_MAX, "PRIVMSG %s\r\n", recvline);
			if (send_msg(&tcpfd, out, recvlen)) {
				strcpy(message, "Message sent successfully\n");
				fprintf(stderr, "irccd: %s", message);
			} else {
				could_not_send(message, IRC_BUF_MAX);
			}
			break;
		case NICK_MOD:
			if (strncmp(nick, recvline, NICK_LEN) == 0) {
				strcpy(message, "irccd: You are already using this nickname");
				fprintf(stderr, "irccd: %s", message);
				break;
			} 
			strncpy(nick, recvline, NICK_LEN);
			recvlen = snprintf(out, IRC_BUF_MAX, "NICK %s\r\n", nick);
			if (send_msg(&tcpfd, out, recvlen)) {
				msglen = snprintf(message, IRC_BUF_MAX, "NICK message sent\n");
				fprintf(stderr, "irccd: %s", message);
			} else {
				could_not_send(message, IRC_BUF_MAX);
			}
			break;
		case CONN_MOD:
			
			if (ping(&tcpfd, "", 0) > 0) {
				msglen = snprintf(message, IRC_BUF_MAX, "Already connected to a %s\n", host);
				fprintf(stderr, "irccd: %s", message);
				break;
			}
			if (host_connect(&tcpfd, &unixfd, recvline) != 0) {
				msglen = snprintf(message, IRC_BUF_MAX, "Could not connect to %s\n", host);
				fprintf(stderr, "irccd: %s", message);
			} else {
				msglen = snprintf(message, IRC_BUF_MAX, "Connected to %s\n", host);
				fprintf(stderr, "irccd: %s", message);
			}
			break;
		case PING_MOD:
			ping(&tcpfd, recvline, recvlen);
			break;
		case DISC_MOD:
			if (ping(&tcpfd, "", 0) > 0) {
				send_msg(&tcpfd, "QUIT :Bye bye!\r\n", 16);
				//TODO segfaults after this point
				close(tcpfd);
				if (pid != 0) {
					kill(pid, SIGTERM); // Kills child reader
					pid = 0; // Set pid to zero so this won't kill main if invoked again
				}
			} else {
				msglen = snprintf(message, IRC_BUF_MAX, "Cannot disconnect, not connected to host\n");
				fprintf(stderr, "irccd: %s", message);
			}
			break;
		case QUIT_MOD:
			quit(pid);
			break;
		default:
			msglen = snprintf(message, IRC_BUF_MAX, "Invalid command\n");
			fprintf(stderr, "irccd: %s", message);
			sleep(1);
			break;
		}
		// Finally send and message
		send(fd, message, msglen, 0);
		close(fd);
	}
	quit(pid);
}
