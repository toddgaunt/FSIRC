/*
 * ircd is a simple irc-bot daemon
 * it will auto-save messages for you
 * and can be called from the command line
 * to send messages to the specified channel.
 */
//TODO Make it so that the program is more responsive to the control script.

#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h> 
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
//#include <sys/stat.h> // used for creating FIFO pipe

#define MAXLINE 4096 // For reading irc messages
#define MAX_BUF 4096 // For reading fifo pipe
#define QUIT_MOD 'q' // quit mode
#define READ_MOD 'r' // read mode
#define WRIT_MOD 'w' // write mode
#define DEBUG_MOD 'd' // debug mode
#define CONN_MOD 'c' // connection mode

/* function declarations */
int host_conn(char *server, unsigned int port, int *sockfd);
int chan_conn(char *channel);
int send_msg(int sockfd, char *out, int debug);
int read_msg(int sockfd, char *recvline, int debug);

int 
main(int argc, char *argv[]) 
{
    /* Defining pipe and mode */
    char *ircd_fifo = "/tmp/ircd.fifo";
    char buf[MAX_BUF];

    /*TODO read these vars from a config file or something
    * Use sprintf to format them */
    char server[] = "162.213.39.42";
    //char channel[] = "#Y35chan";
    unsigned int port = 6667;
   // char nick[] = "todd";
    int debug = 1;
    int reconn = 0;
   
    // Message buffers for sending and receiving
    char recvline[MAXLINE+1], out[MAXLINE+1];
    int sockfd;

    // Creates socket and connects to server
    if (!host_conn(server, port, &sockfd)) {
        printf("connection failed.");
        exit(1);
    }

    /*TODO substitute hardcoded strings for variables*/
    send_msg(sockfd, "NICK iwakura_lain\r\n", debug);
    send_msg(sockfd, "USER iwakura_lain 8 * :Iwakura\r\n", debug);
    send_msg(sockfd, "JOIN #Y35chan\r\n", debug);

    char pos[MAX_BUF];
    int fd, i;
    int actmode = READ_MOD; // mode the program is in.
    while(1) {
        fd = open(ircd_fifo, O_RDWR);
        if (fd >= 0) {
            memset(&buf, 0, sizeof(buf)); //Clears buffer
            read(fd, buf, MAX_BUF);
            if (debug) printf("PIPE: %s\r\n", buf);
            actmode = buf[0]; // code is first bit of pipe message, might make it first 4 later
            for (i = 0; i<sizeof(buf)-1; i++) {
                pos[i] = buf[i+1]; //copies over msg
            }
        } else {
            actmode = READ_MOD; // if the pipe is closed, go back to reading.
        }
        close(fd);
        remove(ircd_fifo);
        if (debug) printf("MODE: %c\r\n", actmode);

        switch (actmode) {
            case QUIT_MOD:
                exit(0);
            case DEBUG_MOD:
                if (debug != 0) debug = 0;
                else debug = 1;
                break;
            case 1:
                break;
            case READ_MOD:
                if (debug) printf("READING...\n");
                if (read_msg(sockfd, recvline, debug) && reconn) {
                    if (!host_conn(server, port, &sockfd)) {
                        printf("connection failed.");
                        exit(1);
                    }
                }
                break;
            case WRIT_MOD:
                sprintf(out, "PRIVMSG #Y35chan :%s\r\n", pos);
                send_msg(sockfd, out, debug);
                break;
            case CONN_MOD:
                if (reconn == 0) reconn = 1;
                else reconn = 0;
                break;
            case 8:
                break;
            default:
                actmode = READ_MOD;
        }
    }
    exit(1); // if while loop is broken, exit with error.
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
/*reads next message from irc, and replies to any pings with send_msg*/
{
    recvline[0] = 0;
    int n = read(sockfd, recvline, MAXLINE);

    if (n > 0 && debug) {
        printf("IN: %s", recvline);
    }

    // If message is PING, reply PONG
    char *pos, out[MAXLINE+1];
    if (n > 0) {
        recvline[n] = 0;
        // Sees if the server sent a ping message
        if (strstr(recvline, "PING") != NULL) {
            //out[0] = 0;
            pos = strstr(recvline, " ")+1;
            sprintf(out, "PONG %s\r\n", pos);
            send_msg(sockfd, out, debug);
        }
    }
    return n;
}
