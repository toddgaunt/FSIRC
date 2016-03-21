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
#include <malloc.h>

#include "irccd.h"


int 
main(int argc, char *argv[]) 
{
    /* Defining fifo and initial mode */
    char *myfifo = "/tmp/irccd.fifo";
    char actmode = 0;

    /* Connection info */
    char host_serv[MAX_BUF] = "162.213.39.42";
    char chan_name[CHAN_LEN];
    unsigned int port = 6667;
    char nick[] = "iwakura_lain";
    int sockfd; //socket
    
    /* Message buffers for sending and recieving */
    char recvline[MAX_BUF+1], buf[MAX_BUF], out[MAX_BUF+1];
    
    /* For channel linked list */
    Channel *chan_head=NULL;
    //Channel *chan_cur // Will be used to store current node later TODO
    chan_head = (Channel*)malloc(sizeof(Channel));

    if (!host_conn(host_serv, port, &sockfd)) {
        printf("connection failed.");
        exit(1);
    }

    /* Initial connection and nickname */
    sprintf(out, "NICK %s\r\n", nick);
    send_msg(sockfd, out);
    sprintf(out, "USER %s 8 * :nick\r\n", nick);
    send_msg(sockfd, out);

    /* Forks to constantly read from irc server */
    int pid = fork(); 
    if (pid == 0) {
        while(1) {
            if (DEBUG) printf("READING...\n");
            memset(&recvline, 0, sizeof(recvline)); //clears previous messsage
            if (read_msg(sockfd, recvline) <= 0) {
                printf("READ FAILED.\n");
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
            actmode = buf[0];
            if (DEBUG) printf("MODE: %c\n", actmode);
            for (i = 0; i < sizeof(buf)-1; i++) {
                pos[i] = buf[i+1]; // Stores the message for later formatting
            }

            switch (actmode) {
                case QUIT_MOD:
                    kill_children(pid, 0);
                case JOIN_MOD:
                    /* Save the value of the currently joined channel */
                    for (i = 0; i < sizeof(pos); i++) {
                        chan_name[i] = pos[i];
                    }
                    sprintf(out, "JOIN %s\r\n", chan_name);
                    if (DEBUG) printf("JOIN: %s",out);
                    send_msg(sockfd, out);
                    add_chan(chan_head, chan_name);
                    break;
                case PART_MOD:
                    /* leaves a channel */
                    for (i = 0; i < sizeof(pos); i++) {
                        chan_name[i] = pos[i];
                    }
                    sprintf(out, "PART %s\r\n", chan_name);
                    if (DEBUG) printf("PART: %s",out);
                    send_msg(sockfd, out);
                    rm_chan(chan_head, chan_name);
                    break;
                case LIST_MOD:
                    list_chan(chan_head);
                    break;
                case WRIT_MOD:
                    /* Add a check to see if channel is an actual channel */
                    sprintf(out, "PRIVMSG %s :%s\r\n", chan_name, pos);
                    if (DEBUG) printf("WRITE: %s", out);
                    send_msg(sockfd, out);
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
        printf("Socket creation failed.");
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

int add_chan(Channel *head, char *chan_name)
/* Adds a channel to a linked list */
{
    /* Filters out impossible channel names */
    if (chan_name[0] == '#') { 
        Channel *tmp;
        tmp = head;
        if (tmp != NULL) {
            while (tmp->next != NULL) {
                tmp = tmp->next; //Traverses to next item in list
                /* Checks if the channel is already in the list, returns if it is */
                if (strcmp(tmp->name, chan_name) == 0) return 1;
            }
        } 
        tmp->next = malloc(sizeof(Channel)); //Creates new item

        tmp = tmp->next; 

        if (tmp == 0) {
            printf("Out of memory.\n");
            return -1;
        }

        /* inits list values */
        tmp->name = strdup(chan_name);
        tmp->next = NULL;
        if (DEBUG) printf("LINKED: %s\n",tmp->name);
        
        return 0;
    } else {
        if (DEBUG) printf("%s is not a valid channel.\n", chan_name);
        return 1;
    }
}

int rm_chan()
/* Removes last entry from a linked list */
{
    //TODO
}

int list_chan(Channel *head)
/* Pretty Prints all channels client is connected to */
{
    Channel *tmp;
    tmp = head;
    if (tmp != NULL) {
        printf("%s->", tmp->name);
        while (tmp->next != NULL) {
            tmp = tmp->next;
            printf("%s->", tmp->name);
        }
    }
    printf("\n");
}

int set_nick()
/* sets the client's irc nickname */
{
    //TODO
}

int 
send_msg(int sockfd, char *out)
/*sends a message to the irc channel*/
{
    int n = send(sockfd, out, strlen(out), 0);
    if (n > 0 && DEBUG) printf("OUT: %s", out);

    return n;
}

int
read_msg(int sockfd, char *recvline)
/*reads next message from irc, and replies to any pings with send_msg*/
{
    int n = read(sockfd, recvline, MAX_BUF);
    if (n > 0 && DEBUG) printf("IN: %s", recvline);

    // If message is PING, reply PONG
    char *pos, out[MAX_BUF+1];
    if (n > 0) {
        recvline[n] = 0;
        if (strstr(recvline, "PING") != NULL) {
            pos = strstr(recvline, " ")+1;
            sprintf(out, "PONG %s\r\n", pos);
            send_msg(sockfd, out);
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
