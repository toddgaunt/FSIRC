/*
 * Name: irccd
 * Author: Todd Gaunt
 * Last updated: 2016-03-16
 * License: MIT
 * Description:
 * irccd is a simple irc-bot daemon
 * it will auto-save messages for you
 * and can be called from the command line
 * to send messages to the specified channel.
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
#include <signal.h>

#include "irccd.h"

int 
main(int argc, char *argv[]) 
{
    /* Defining fifo and initial mode */
    char *myfifo = "/tmp/irccd.fifo";
    char actmode = 0;

    /* Connection info */
    char ircserv[MAX_BUF] = "162.213.39.42";
    char ircchan[MAX_BUF];
    unsigned int port = 6667;
    char nick[] = "iwakura_lain";
    int debug = 1;
   
    /* Message buffers for sending and recieving */
    char recvline[MAX_BUF+1], buf[MAX_BUF], out[MAX_BUF+1];
    /* socket */
    int sockfd;

    /* Connects to server and returns failure code */
    if (!host_conn(ircserv, port, &sockfd)) {
        printf("connection failed.");
        exit(1);
    }

    sprintf(out, "NICK %s\r\n", nick);
    send_msg(sockfd, out, debug);
    sprintf(out, "USER %s 8 * :nick\r\n", nick);
    send_msg(sockfd, out, debug);

    /* Forks to constantly read from irc server */
    int pid = fork(); 
    if (pid == 0) {
        while(1) {
            memset(&recvline, 0, sizeof(recvline));
            if (debug) printf("READING...\n");
            if (read_msg(sockfd, recvline, debug) <= 0) {
                printf("CONNECTION FAILED.\n");
                exit(1); // Exits if read fails aka disconnect
            }
        }
    } else {
        mkfifo(myfifo, 0666);
        char pos[MAX_BUF]; // Holds message without actcode
        int fd, i;
        while(1) {
            fd = open(myfifo, O_RDONLY); 
            memset(&buf, 0, sizeof(buf));
            read(fd, buf, MAX_BUF);
            if (debug) printf("PIPE: %s\n", buf);
            actmode = buf[0];
            printf("ACTMODE: %c\n", actmode);
            for (i = 0; i < sizeof(buf)-1; i++) {
                pos[i] = buf[i+1]; // Stores the message for later formatting
            }

            if (debug) printf("MODE: %c\n", actmode);
            switch (actmode) {
                case QUIT_MOD:
                    kill_children(pid, 0);
                case DEBUG_MOD:
                    if (debug != 0) debug = 0;
                    else debug = 1;
                    break;
                case JOIN_MOD:
                    /* Save the value of the currently joined channel */
                    if (debug) printf("JOINING...\n");
                    for (i = 0; i < sizeof(pos); i++) {
                        ircchan[i] = pos[i];
                    }
                    sprintf(out, "JOIN %s\r\n", ircchan);
                    send_msg(sockfd, out, debug);
                    break;
                case PART_MOD:
                    /* leaves a channel */
                    if (debug) printf("PARTING...\n");
                    for (i = 0; i < sizeof(pos); i++) {
                        ircchan[i] = pos[i];
                    }
                    sprintf(out, "PART %s\r\n", ircchan);
                    send_msg(sockfd, out, debug);
                    break;
                case WRIT_MOD:
                    /* Add a check to see if ircchan is an actual channel */
                    if (debug) printf("WRITING...\n");
                    sprintf(out, "PRIVMSG %s :%s\r\n", ircchan, pos);
                    send_msg(sockfd, out, debug);
                    break;
                case NICK_MOD:
                    break;
                default:
                    printf("NO COMMAND...");
                    sleep(1);
            }
            close(fd);
        }
    }
    printf("Fifo creation failed.\n");
    kill_children(pid, 1);
}

int 
host_conn(char *server, unsigned int port, int *sockfd)
/* constructs socket at sockfd and then connects to server */
{
    struct sockaddr_in servaddr; 
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    
    /* modifies the sockfd for the rest of main() to use */
    *sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (*sockfd < 0) {
        printf("socked creation failed.");
        return 0;
    }

    if (inet_pton(AF_INET, server, &servaddr.sin_addr) <= 0) {
        printf("Invalid network address.");
        return 0;
    }

    /* points sockaddr pointer to servaddr because connect takes sockaddr structs */
    if (connect(*sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        return 0;
    }

    return 1;
}

int 
send_msg(int sockfd, char *out, int debug)
/*sends a message to the irc channel*/
{
    int n = send(sockfd, out, strlen(out), 0);
    if (n > 0 && debug) printf("OUT: %s", out);

    return n;
}

int
read_msg(int sockfd, char *recvline, int debug)
/*reads next message from irc, and replies to any pings with send_msg*/
{
    recvline[0] = 0;
    int n = read(sockfd, recvline, MAX_BUF);
    if (n > 0 && debug) printf("IN: %s", recvline);

    // If message is PING, reply PONG
    char *pos, out[MAX_BUF+1];
    if (n > 0) {
        recvline[n] = 0;
        if (strstr(recvline, "PING") != NULL) {
            pos = strstr(recvline, " ")+1;
            sprintf(out, "PONG %s\r\n", pos);
            send_msg(sockfd, out, debug);
        }
    }
    return n;
}

void
kill_children(int pid, int ecode)
/*cleans up child processes*/
{
    kill(pid, SIGTERM);
    exit(ecode);
}
