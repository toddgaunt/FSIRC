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
/* - - - - - - - */
#include "irccd.h"

int main(int argc, char *argv[]) 
{
    // Some program info
    char *myfifo = "/tmp/irccd.fifo";
    char actmode = 0;
    int pid = 0;

    // Connection info 
    char host_serv[CHAN_LEN] = "162.213.39.42";
    char chan_name[CHAN_LEN];
    unsigned int port = 6667;
    char nick[CHAN_LEN] = "iwakura_lain";
    int sockfd = 0; //socket
    
    // Message buffers for sending and recieving 
    char buf[MAX_BUF], out[MAX_BUF], pos[MAX_BUF];
    
    // For channel linked list
    Channel *chan_head=NULL;
    //Channel *chan_cur // Will be used to store current node later TODO
    chan_head = (Channel*)malloc(sizeof(Channel));

    mkfifo(myfifo, 0666);
    int fd, i;
    while(1) {
        fd = open(myfifo, O_RDONLY); 
        memset(&buf, 0, sizeof(buf));
        read(fd, buf, sizeof(buf));
        actmode = buf[0];
        if (DEBUG) printf("MODE: %c\n", actmode);
        for (i = 0; i < sizeof(buf)-1; i++) {
            pos[i] = buf[i+1]; // Stores the message for later formatting
        }

        switch (actmode) {
            case JOIN_MOD:
                // joins a channel
                i = 0;
                while (pos[i] != '\0') {
                    chan_name[i] = pos[i];
                    i += 1; 
                }
                chan_name[i]='\0';
                sprintf(out, "JOIN %s\r\n", chan_name);
                if (send_msg(sockfd, out) > 0) {
                    add_chan(chan_head, chan_name);
                } else {
                    printf("not connected.\n");
                }
                break;
            case PART_MOD:
                // leaves a channel
                i = 0;
                while (pos[i] != '\0') {
                    chan_name[i] = pos[i];
                    i += 1; 
                }
                chan_name[i]='\0';
                sprintf(out, "PART %s\r\n", chan_name);
                if (send_msg(sockfd, out) > 0) {
                    rm_chan(chan_head, chan_name);
                } else {
                    printf("not connected.\n");
                }
                break;
            case LIST_MOD:
                list_chan(chan_head);
                break;
            case WRITE_MOD:
                // Add a check to see if channel is an actual channel
                sprintf(out, "PRIVMSG %s :%s\r\n", chan_name, pos);
                send_msg(sockfd, out);
                break;
            case NICK_MOD:
                if (strcmp(nick, pos) == 0) {
                    if (DEBUG) printf("Nickname %s is already in use by this client.", nick);
                } else {
                    i = 0;
                    while (pos[i] != '\0') {
                        nick[i] = pos[i];
                        i += 1; 
                    }
                    nick[i] = '\0';
                    sprintf(out, "NICK %s\r\n", nick);
                    send_msg(sockfd, out);
                }
                break;
            case CONN_MOD:
                // Test if we're already connected somewhere
                if (strcmp(host_serv, pos) == 0 && send_msg(sockfd, "PING") != -1) {
                    if (DEBUG) printf("Server %s already connected to.", nick);
                } else {
                    i = 0;
                    while (pos[i] != '\0') {
                        host_serv[i] = pos[i];
                        i += 1;
                    }
                    host_serv[i] = '\0';

                    if (!host_conn(host_serv, port, &sockfd)) {
                        printf("Connection failed.\n");
                        exit(1);
                    }
                    sprintf(out, "NICK %s\r\n", nick);
                    send_msg(sockfd, out);
                    sprintf(out, "USER %s 8 * :nick\r\n", nick);
                    send_msg(sockfd, out);
                    // Forks read process constantly read from host
                    pid = spawn_reader(sockfd);
                }
                break;
            case DISC_MOD:
                break;
            case QUIT_MOD:
                kill_children(pid, 0);
            default:
                printf("NO COMMAND...");
                sleep(1);
        }
        close(fd);
    }
    printf("Failed to fork.\n");
    //kill_children(pid, 1);
}

int host_conn(char *host, unsigned int port, int *sockfd)
{/* constructs socket at sockfd and then connects to server */
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

    if (inet_pton(AF_INET, host, &servaddr.sin_addr) <= 0) {
        printf("Invalid network address.\n");
        return 0;
    }

    /* points sockaddr pointer to servaddr because connect takes sockaddr structs */
    if (connect(*sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        return 0;
    }

    return 1;
}

int host_disc(char *host, unsigned int port, int *sockfd)
{/* disconnects the socket and then deletes it */
    int n = 0;
    return n;
}

int send_msg(int sockfd, char *out)
{/*sends a message to the irc channel*/
    int n = send(sockfd, out, strlen(out), 0);
    if (n > 0 && DEBUG) printf("OUT: %s", out);

    return n;
}

int read_msg(int sockfd, char *recvline)
{/*reads next message from irc, and replies to any pings with send_msg*/
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

int spawn_reader(int sockfd)
{
    int pid = fork(); 
    char recvline[MAX_BUF];
    if (pid == 0) {
        while(1) {
            memset(&recvline, 0, sizeof(recvline)); // clears previous messsage
            if (read_msg(sockfd, recvline) <= 0) {
                if (DEBUG) printf("READ FAILED.\n");
                exit(1); // exits if read failed aka disconnect
            }
        }
    }
    return pid;
}

int add_chan(Channel *head, char *chan_name)
{/* Adds a channel to a linked list */
    // Filters out impossible channel names
    if (chan_name[0] == '#') { 
        Channel *tmp;
        tmp = head;
        if (tmp != NULL) {
            while (tmp->next != NULL) {
                tmp = tmp->next; //Traverses to next item in list
                // Checks if the channel is already in the list, returns if it is 
                if (strcmp(tmp->name, chan_name) == 0) return 1;
            }
        } 
        tmp->next = malloc(sizeof(Channel)); //Creates new item

        tmp = tmp->next; 

        if (tmp == 0) {
            printf("Out of memory.\n");
            return -1;
        }

        /* init Channel values */
        tmp->name = strdup(chan_name);
        tmp->next = NULL;
        if (DEBUG) printf("LINKED: %s\n",tmp->name);
        
        return 0;
    } else {
        if (DEBUG) printf("%s is not a valid channel.\n", chan_name);
        return 1;
    }
}

int rm_chan(Channel *head, char *chan_name)
{/* Removes last entry from a linked list */
    // requires a valid channel name beginning with #
    if (chan_name[0] == '#') {
        Channel *tmp;
        Channel *garbage;
        tmp = head;
        if (tmp != NULL) {
            while (tmp->next != NULL) {
                if (strcmp(tmp->next->name, chan_name) == 0) {
                    // Frees link and then redirects it to the next node.
                    garbage = tmp->next; 
                    tmp->next = tmp->next->next;
                    if (DEBUG) printf("Channel %s removed.\n", garbage->name);
                    free(garbage->name);
                    free(garbage);
                    return 0; //returns successfully if item is removed.
                } else {
                    tmp = tmp->next;
                }
            }
        }
    } else {
        if (DEBUG) printf("%s is not a valid channel.\n", chan_name);
    }
    return 1;
}

void list_chan(Channel *head)
{/* Pretty Prints all channels client is connected to 
  * Later should write channels to a log file */
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

void kill_children(int pid, int ecode)
{/*cleans up child processes*/
    kill(pid, SIGTERM);
    exit(ecode);
}
