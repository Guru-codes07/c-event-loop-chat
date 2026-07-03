#ifndef CONNECTIONS_H
#define CONNECTIONS_H
#include<poll.h>
#include "common.h"


typedef struct     // client struct  
{
    int socket_fd;
    char name[NAME_SIZE];
}client_t;

/* Global client table — defined in connections.c, accessible everywhere */
extern client_t *clients[MAX_CLIENTS];
extern struct pollfd fds[MAX_CLIENTS + 1];
extern int nfds;

// functions which are used to manage clients
// add a new client to the clients array
int add_client(client_t *client);

// remove a client from the clients array
void remove_client(client_t *client);

/* To broadcast the message to everyone except
the one who sended it (sender) */
void broadcast_message(const char *message,client_t *sender);

// function that shows online users
void send_online_list(client_t *client);

/* function that sends private message to a single user */
int send_private_message(client_t *sender,const char *target_name,const char *text);




#endif