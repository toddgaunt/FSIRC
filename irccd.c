/*
 * ircd is a simple irc-bot daemon
 * it will auto-save messages for you
 * and can be called from the command line
 * to send messages to the specified channel.
 */
//TODO Make it so that the program is more responsive to the control script.

#include <stdio.h> 
#include <stdlib.h>
#include <string.h> // used for string manipulation
#include <netdb.h>
#include <unistd.h> // gives the read()
#include <arpa/inet.h> // gives the inet_pton()
#include <fcntl.h> // For declaring O_READONLY
//#include <sys/stat.h> // used for creating FIFO pipe
#include <sys/types.h> // used for assigning rw permissions to fifo file
#include <sys/socket.h> // used for creating posix socket that allows irc connection

#define MAXLINE 4096 // For reading irc messages
#define MAX_BUF 1024 // For reading named pipe

/* function declarations */
int host_conn(char *server, unsigned int port, int *sockfd);
int chan_conn(char *channel);
int send_msg(int sockfd, char *out, int debug);
int read_msg(int sockfd, char *recvline, int debug);

int 
main(int argc, char *argv[]) 
{
    /* the piping code */
    char *ircd_fifo = "/tmp/ircd.fifo";
    char buf[MAX_BUF];

    /*TODO read these vars from a config file or something*/
    char server[] = "162.213.39.42";
    //char channel[] = "#Y35chan";
    unsigned int port = 6667;
   // char nick[] = "todd";
    int debug = 1;
   
    // Message buffers for sending and receiving
    char recvline[MAXLINE+1], out[MAXLINE+1];
    int sockfd; // Stores the actual socket

    // Creates socket and connects to server
    if (!host_conn(server, port, &sockfd)) {
        printf("connection failed.");
        exit(1);
    }

    /*TODO substitute hardcoded strings for variables*/
    send_msg(sockfd, "NICK iwakura_lain\r\n", debug);
    send_msg(sockfd, "USER iwakura_lain 8 * :Iwakura\r\n", debug);
    //send_msg(sockfd, "JOIN #Y35chan\r\n", debug);
    send_msg(sockfd, "JOIN #Y35chan\r\n", debug);

    char *pos;
    int n, fd;
    while(1) {
        /* opens the pipe to read its contents 
         * TODO move this pipe out of while loop so python script
         * can tell it which server to connect to. */
        /* created the named pipe */
        fd = open(ircd_fifo, O_RDWR);
        // if open succeeds, wait for pipe
        if (fd >= 0) {
            memset(&buf, 0, sizeof(buf)); //Clears buffer
            /*TODO add a switch case statement for different message codes sent from the client*/
            read(fd, buf, MAX_BUF);
            if (debug) printf("PIPE: %s\r\n", buf);
            send_msg(sockfd, "PRIVMSG #Y35chan :Hey what's up?\r\n", debug);
        }
        close(fd);
        remove(ircd_fifo); //removes the fifo file, because the python scripts creates a new one.

        // Reading messages from irc
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
        } else {
            exit(0);
        }
    }
    exit(0);
}

int 
host_conn(char *server, unsigned int port, int *sockfd)
/* constructs socket at sockfd and then connects to server */
{
    struct sockaddr_in servaddr; 
    memset(&servaddr, 0, sizeof(servaddr)); //Zeros out memory location the new struct uses.
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

    // points sockaddr pointer to servaddr because connect takes sockaddr structs
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
