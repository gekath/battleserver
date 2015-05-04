// text jk
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_LENGTH 512

#ifndef PORT
    #define PORT 50510
#endif

struct client {
    int fd;
    char name[MAX_LENGTH];
    struct in_addr ipaddr;
    int matched;
    int loggedin;
    int hp; // hit points
    int pm; // power moves
    int thisturn;
    int playagain;
    int speaking;
    char  message[MAX_LENGTH];
    struct client * last;
    struct client *next;
};

const char *intro = "Here are your options!\n (a) Attack\n (p) Power Strike!\n (s) Send a kind or threatening message.\nYou are attacking\n";
static struct client *addclient(struct client *top, int fd, struct in_addr addr);
static struct client *removeclient(struct client *top, int fd);
static void broadcast(struct client *top, struct client * exclude, char *s, int size);
//int get_login(struct client *p, struct client * top, char * move);
unsigned time(time_t * t);

int handleclient(struct client *p, struct client *top);
int bindandlisten(void);
int find_network_newline(char * buf, int inbuf);
void broadcastlogin(struct client *p, struct client *head);
int matchclient(struct client * p, struct client * head);
int combat(struct client * p, struct client * q, struct client * top);
int turn(struct client *p1, struct client *p2, char * move);
int moveclient(struct client * p, struct client * top);
void removehelper(struct client ** top, struct client * prev, struct client * current);
//int getspeech(struct client * p, struct client * q, char * move);
int getwords(struct client * p, char * move, int loginorspeech);

int main(void) {
    int clientfd, maxfd, nready;
    struct client *p;
    struct client *head = NULL;
    socklen_t len;
    struct sockaddr_in q;
    struct timeval tv;
    fd_set allset;
    fd_set rset;

    int i;

    int listenfd = bindandlisten();
    // initialize allset and add listenfd to the
    // set of file descriptors passed into select
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    // maxfd identifies how far into the set to search
    maxfd = listenfd;

    while (1) {
        // make a copy of the set before we pass it into select
        rset = allset;
        /* timeout in seconds (You may not need to use a timeout for
        * your assignment)*/
        tv.tv_sec = 10;
        tv.tv_usec = 0;  /* and microseconds */

        nready = select(maxfd + 1, &rset, NULL, NULL, &tv);
        if (nready == 0) {
            printf("No response from clients in %ld seconds\n", tv.tv_sec);
            continue;
        }

        if (nready == -1) {
            perror("select");
            continue;
        }

        if (FD_ISSET(listenfd, &rset)){
            printf("a new client is connecting\n");
            len = sizeof(q);
            if ((clientfd = accept(listenfd, (struct sockaddr *)&q, &len)) < 0) {
                perror("accept");
                exit(1);
            }
            FD_SET(clientfd, &allset);
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }
            printf("connection from %s\n", inet_ntoa(q.sin_addr));

            head = addclient(head, clientfd, q.sin_addr);
            char login[] = "Enter your username: ";
            send(clientfd, login, strlen(login), 0);
        }
        
        for(i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i, &rset)) {
                for (p = head; p != NULL; p = p->next) {
                    if (p->fd == i) {
                        int result = handleclient(p, head);
                        if (result == -1) {
                            int tmp_fd = p->fd;
                            head = removeclient(head, p->fd);
                            FD_CLR(tmp_fd, &allset);
                            close(tmp_fd);
                        }
                        break;
                    }
                }
            }
        }
    }
    return 0;
}

int handleclient(struct client *p, struct client *top) {
    char outbuf[512];
    int len;
    char move[1];

    // Reads from fd of client
    if ((len = read(p->fd, move, 1)) < 0) {
        perror("read");
        exit(1);
    }
    
    // While the client is still making requests
    if (len > 0) {
    
        // Logging in
        if (p->loggedin == 0) {
            //get_words(p, top, move);    
            
            int checklogged;
            checklogged = getwords(p, move, 1);
            
            // User is finished entering username. Ready to enter battle arena.
            if (checklogged == 1) {
                p->loggedin = 1;
                broadcastlogin(p, top);
            } 


            // Once the client is logged in
            if(p->loggedin == 1) {
        
                // Match client
                matchclient(p, top);

                // If client has a match, send message indicating they are attacking
                if (p->last) {
                    if ((write(p->fd, intro, strlen(intro))) <0) {
                        perror("write");
                        exit(1);
                    }
                }
            }
        } else {

            // Match client
            matchclient(p, top);

            if (p->matched != 0) {
               
                // Player p's turn 
                if (p->thisturn == 1) {
                   
                    // Client's opponent has thisturn attribute of zero
                    p->last->thisturn = 0;
                        
                    // Client is speaking
                    if (p->speaking == 1) {
                        // getspeech(p, p->last, move);
                        
                        int checkfinished;
                        checkfinished = getwords(p, move, 2);
                        
                        // User is finished speaking.
                        if (checkfinished == 1) {
                            char pmsg[MAX_LENGTH];
                            sprintf(pmsg, "%s: %s\n", p->name, p->message);
                            if (write(p->last->fd, pmsg, strlen(pmsg)) < 0) {
                                perror("write");
                                exit(1);
                            }
                            bzero(p->message, MAX_LENGTH);
                            p->speaking = 0;
                            
                            if (write(p->fd, intro, strlen(intro)) < 0) {
                                perror("write");
                                exit(1);
                            } 
                        }    
                    } else {

                    turn(p, p->last, move);
                   
                    // Player p's turn
                    if (p->thisturn == 0 ) {
                        
                        // send message indicating client is attacking
                        if (write(p->last->fd, intro, strlen(intro)) < 0) {
                            perror("write");
                            exit(1);
                        }
                        

                        // If opponent has lost (i.e. hp <= 0)  
                        } if (p->last->hp <=0) {
                
                            // Write win/lose messages to corresponding players
                            char lost[] = "You lost. DUn DUn Duuuun.\nAwaiting new opponent\n";
                            char win[] = "You win.\nAwaiting new opponent\n";
                            if (write(p->fd, win, strlen(win)) < 0) {
                                perror("write");
                                exit(1);
                            }
                        
                            if (write(p->last->fd, lost, strlen(lost)) < 0) {
                                perror("write");
                                exit(1);
                            }

                            // End of match. Set attributes back to initial values
                            p->matched = 0;
                            p->hp = -1;
                            p->pm = -1;
                            p->last->matched = 0;
                            p->last->hp = -1;
                            p->last->pm = -1;
                        }                
                    }
                }
            }
        } return 0;
        
    // Socket has closed
    } else if (len == 0) {
        printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
        sprintf(outbuf, "Say goodbye to %s\r\n", p->name);
        broadcast(top, p->last, outbuf, strlen(outbuf));

        // Inform partner that new opponent will be set up. They win the game.
        char win[] = "You win.\nAwaiting new opponent\n";
        if (write(p->last->fd, win, strlen(win)) < 0) {
            perror("write");
            exit(1);
        }

        // End of match. Set attributes back to initial values
        p->last->matched = 0;
        p->last->hp = -1;
        p->last->pm = -1;

        // Match with new client
        matchclient(p->last, top);
       
        return -1;

    // Error. Shouldn't happen
    } else {
        perror("read");
        return -1;
    }
}

int matchclient(struct client *p, struct client * top) {

    if ((p->matched == 1) | (p->loggedin == 0)) {
        return 0;
    }
    // Check conditions
    if (top != NULL) {
        struct client * current = top;
        for (current = top; current; current = current->next) {
            if (current->matched == 0) {
                if (current->loggedin == 1) {
                    if (p->fd != current->fd) {
                        if (p->last == NULL) {
                            break;
                        } else {
                            if (p->last->fd != current->fd) {
                                break;
                            }
                        }
                    }
                }
            }
        } 
        // set conditions
        if (current == NULL) {
            return 0;
        } else {
            p->last = current;
            current->last = p;
            p->matched = 1;
            current->matched = 1;
        
            printf("%s and %s have been matched\n", p->name, current->name);
            // combat
            combat(p, current, top);
        }
    }

    return 0;
}

int combat(struct client *p, struct client *q, struct client * top) {
    
    // Randomize hitpoints
    time_t t;

    srand((unsigned) time(&t));
    p->hp = rand() % (30 + 1 - 20) + 20;
    q->hp = rand() % (30 + 1 - 20) + 20;

    printf("%d %d\n", p->hp, q->hp);

    p->pm = rand() % (3 + 1 - 1) + 1;
    q->pm = rand() % (3 + 1 - 1) + 1; 

    // Send welcome messages to players
    char begin1[MAX_LENGTH];
    char begin2[MAX_LENGTH];
    sprintf(begin1, "Let the battle begin! You are battling %s.\n", q->name);
    sprintf(begin2, "Let the battle begin! You are battling %s.\n", p->name);
    write(p->fd, begin1, strlen(begin1));
    write(q->fd, begin2, strlen(begin2));

    // Send win/loss messages
    char lost[] = "You lost. DUn DUn Duuuun.\nAwaiting new opponent\n";
    char win[] = "You win.\nAwaiting new opponent\n";
    // Check who lost
    if ((p->hp <= 0) |( q->hp <= 0)) {
        if (p->hp <= 0) {
            write(p->fd, lost, strlen(lost));
            write(q->fd, win, strlen(win));
        }
        else if (q->hp <= 0) {
            write(p->fd, win, strlen(win));
            write(q->fd, lost, strlen(lost));
        }
        p->matched = 0;
        p->hp = -1;
        p->pm = -1;
        q->matched = 0;
        q->hp = -1;
        q->pm = -1;
        
//        moveclient(p, top);
  //      moveclient(q, top);
    }
    
    return 0;
}

int turn(struct client *p1, struct client *p2, char * move) {

    // Randomize damage
    time_t t;
    int damage, probability;
    srand((unsigned) time(&t));
    
    // "a" input: Attack move
    if (strncmp(move, "a", 1) == 0) {
        damage = rand() % (6 + 1 - 2) + 2;
        p2->hp = p2->hp - damage;
        
        // Next player's turn
        p1->thisturn = 0;
        p2->thisturn = 1;

        // Write to players
        char damagemsg2[MAX_LENGTH];
        sprintf(damagemsg2, "You now have %d HP.\n==========================\n", p2->hp);
        if (write(p2->fd, damagemsg2, strlen(damagemsg2)) < 0) {
            perror("write");
            exit(1);
        }        

    // "p" input: Power move
    } else if (strncmp(move, "p", 1) == 0) {
        
        // player does not have any more power moves. Must play turn again
        if (p1->pm < 1) {
            char replay[] = "You're out of power moves. Try again.\n";
            if (write(p1->fd, replay, strlen(replay)) < 0) {
                perror("write");
                exit(1);
            }
            if (write(p1->fd, intro, strlen(intro)) <0) {
                perror("write");
                exit(1);
            } 


        // player plays power move
        } else {
            damage = rand() % (6 + 1 - 2) + 2;
            probability = rand() % (1 + 1 - 0) + 0;
            
            // Power move was effective
            if (probability == 1) {
                // write that damage has been done
                p2->hp = p2->hp - (damage * 3);
                p1->pm = p1->pm -1;
    
                // Send message to players
                char pm1msg[] = "Your powermove was effective.\n==========================\n";
                if (write(p1->fd, pm1msg, strlen(pm1msg)) < 0) {
                    perror("write");
                    exit(1);
                }
                
                char damage1[MAX_LENGTH];
                sprintf(damage1, "You now have %d HP.\n==========================\n", p2->hp);
                if (write(p2->fd, damage1, strlen(damage1)) < 0) {
                    perror("write");
                    exit(1);
                }

            // Power move missed
            } else if (probability == 0) {
                
                char missed[] = "You missed.\n==========================\n";
                if (write(p1->fd, missed, strlen(missed)) < 0) {
                    perror("write");
                    exit(1);
                }

                char damage2[MAX_LENGTH];
                sprintf(damage2, "You now have %d HP.\n==========================\n", p2->hp);
                if (write(p2->fd, damage2, strlen(damage2)) < 0) {
                    perror("write");
                    exit(1);
                }

            }
            p1->thisturn = 0;
            p2->thisturn = 1;
        }
    
    // Player is speaking to opponent
    } else if (strncmp(move, "s", 1) == 0) {
    
        p1->speaking = 1;

        char writemsg[] = "Write your message to your opponent: ";
        if (write(p1->fd, writemsg, strlen(writemsg)) < 0) {
            perror("write");
            exit(1);
        }
    
    // Invalid input. Ask user to input again
    } else {
        char invalid[] = "Invalid move. Try again\n";
        if (write(p1->fd, invalid, strlen(invalid)) < 0) {
            perror("write");
            exit(1);
        }
        
        if (write(p1->fd, intro, strlen(intro)) <0) {
            perror("write");
            exit(1);
        }

    }

    return 0;
}
/*
int moveclient(struct client * p, struct client *top) {

    p->matched = 0;
    p->hp = -1;
    p->pm = -1;

    struct client * prev = NULL;
    struct client * current;
    for (current = top; current; prev = current, current = current->next) {
        if (current->fd == p->fd) {
            removehelper(&top, prev, current);
        break;
        }
    }

    if (top == NULL) {
        top = current;
    } else {
        struct client * newcurrent = top;
        while (newcurrent->next != NULL) {
            newcurrent = newcurrent->next;
        }
        newcurrent->next = p;
    }

    return top;

}
*/
void removehelper(struct client ** top, struct client * prev, struct client * current) {
    if (prev == NULL) {
        *top = current->next;
    } else {
        prev->next = current->next;
    }
}


 /* bind and listen, abort on error
  * returns FD of listening socket
  */
int bindandlisten(void) {
    struct sockaddr_in r;
    int listenfd;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    int yes = 1;
    if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) {
        perror("setsockopt");
    }
    memset(&r, '\0', sizeof(r));
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(PORT);

    if (bind(listenfd, (struct sockaddr *)&r, sizeof r)) {
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, 5)) {
        perror("listen");
        exit(1);
    }
    return listenfd;
}

static struct client *addclient(struct client *top, int fd, struct in_addr addr) {

    struct client *p = malloc(sizeof(struct client));
    if (p == NULL) {
        perror("malloc");
        exit(1);
    }

    printf("Adding client %s\n", inet_ntoa(addr));

    p->fd = fd;
    bzero(p->name, MAX_LENGTH);
    p->ipaddr = addr;
    p->matched = 0;
    p->loggedin = 0;
    p->hp = -1;
    p->pm = -1;
    bzero(p->message, MAX_LENGTH);
    p->thisturn = 1;
    p->speaking = 0;
    p->last = NULL;
    p->next = NULL;

    if (top == NULL) {
        top = p;
    } else {
        struct client * current = top;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = p;
    } return top;
}

static struct client *removeclient(struct client *top, int fd) {
    struct client **p;

    for (p = &top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        struct client *t = (*p)->next;
        printf("Removing client %d %s\n", fd, inet_ntoa((*p)->ipaddr));
        free(*p);
        *p = t;
    } else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                 fd);
    }
    return top;
}


static void broadcast(struct client *top, struct client *exclude, char *s, int size) {
    struct client *p;
    
    for (p = top; p; p = p->next) {
        if (exclude != NULL && p->loggedin == 1) {
            if (p->fd != exclude->fd) {
                write(p->fd, s, size);
            }
        }
    }
}

int find_network_newline(char *buf, int inbuf) {
    int i = 0;
    for (i=0; i < inbuf; i++) {
        if (buf[i] == '\n') {
            printf("in here");
            return i;
        }
    } printf("out here");
    return -1; // return the location of '\r' if found
}

int getwords(struct client *p, char * move, int loginorspeech) {

    int found = 0;
    char (*buffer)[MAX_LENGTH];

    if (loginorspeech == 1) {
        buffer = &p->name;
    } else if (loginorspeech == 2) {
        buffer = &p->message;
    }

    if (strlen(*buffer) < MAX_LENGTH - 1) {
        strncat(*buffer, move, 1);
    } else if (strlen(*buffer) == MAX_LENGTH) {
        (*buffer)[MAX_LENGTH] = '\0';
    } 
    if ((*buffer)[strlen(*buffer) - 1] == '\n') {
        (*buffer)[strlen(*buffer) - 1] = '\0';
        found = 1;
    }
    return found;
}
    
void broadcastlogin(struct client *p, struct client *head) {

    // Welcome to the Pokemon battle arena
    char str_welcome[MAX_LENGTH];
    sprintf(str_welcome, "Welcome to the Pokemon battle arena, %s\nAwaiting an opponent.\n", p->name);

    if (send(p->fd, str_welcome, strlen(str_welcome), 0) < 0) {
        perror("send");
        exit(1);
    }
    // Notify other battlers of new player
    char greeting[MAX_LENGTH];
    sprintf(greeting, "Please welcome new Pokemon battler, %s\n", p->name);
    broadcast(head, p, greeting, strlen(greeting)); 
}
