/*
 * ircd is a simple irc-bot daemon
 * it will auto-save messages for you
 * and can be called from the command line
 * to send messages to the specified channel.
 */

#include <stdio.h> 
#include <stdlib.h>
#include <string.h> // used for string manipulation
#include <netdb.h>
#include <unistd.h> // gives the read()
#include <arpa/inet.h> // gives the inet_pton()
#include <sys/socket.h> // used for creating posix socket that allows irc connection

#define MAXLINE 4096

/* function declarations */
int host_conn(char *server, unsigned int port, int *sockfd);
int chan_conn(char *channel);
int send_msg(int sockfd, char *out, int debug);
int read_msg(int sockfd, char *recvline, int debug);

int 
main(int argc, char *argv[]) 
{
    /*TODO read these vars from a config file or something*/
    char *server = "162.213.39.42";
    char *channel = "#Y35chan";
    unsigned int port = 6667;
    char nick[] = "todd";
    int debug = 1;
   
    // Message buffers for sending and receiving
    char recvline[MAXLINE+1], out[MAXLINE+1];
    int sockfd; // Stores the actual socket

    // Creates socket and connects to server
    if (!host_conn(server, port, &sockfd)) {
        printf("192.186.157.43");
        exit(1);
    }

    /*TODO substitute hardcoded strings for variables above*/
    send_msg(sockfd, "NICK toddbot\r\n", debug);
    send_msg(sockfd, "USER todd 8 * : Todd Gaunt\r\n", debug);
    send_msg(sockfd, "JOIN #Y35chan\r\n", debug);
    send_msg(sockfd, "MSG #Y35chan : Hey what's up?", debug);

    char *pos, *cmd;
    int n;
    while(1) {
        recvline[0] = 0;
        n = read_msg(sockfd, recvline, debug);

        if (n > 0) {
            recvline[n] = 0;
            // Sees if the server sent a ping message
            if (strstr(recvline, "PING") != NULL) {
                out[0] = 0;
                pos = strstr(recvline, " ")+1;
                sprintf(out, "PONG %s\r\n", pos);
                send_msg(sockfd, out, debug);
            }
        }
    }
    exit(0);
}

int 
host_conn(char *server, unsigned int port, int *sockfd)
/* constructs socket at sockfd and then connects to server */
{
    struct sockaddr_in servaddr; 
    bzero(&servaddr, sizeof(servaddr)); //Zeros out memory location the new struct uses.
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    
    //modifies the sockfd for the rest of main() to use
    *sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (*sockfd < 0) {
        printf("socked creation failed.");
        return 0;
    }

    if (inet_pton(AF_INET, server, &servaddr.sin_addr) <= 0) {
        printf("Invalid network address.");
        return 0;
    }

    if (connect(*sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        return 0;
    }

    return 1;
}

int 
send_msg(int sockfd, char *out, int debug)
/*sends a message to the irc channel*/
{
    if (debug) {
        printf("OUT: %s", out);
    }
    return send(sockfd, out, strlen(out), 0);
}

int 
read_msg(int sockfd, char *recvline, int debug)
/*reads next message from irc*/
{
    int n =  read(sockfd, recvline, MAXLINE);
    if (n > 0 && debug) {
        printf("IN: %s", recvline);
    }
    return n;
}
