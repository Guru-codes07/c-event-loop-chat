#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
 
#include "connections.h"
#include "protocol.h"
 
/* Global client table */
client_t *clients[MAX_CLIENTS] = {0};
 
/* Poll array for monitoring sockets */
struct pollfd fds[MAX_CLIENTS + 1] = {0};
int nfds = 0;

// add_client func():

int add_client(client_t *client)
{
    if (client == NULL)
        return 0;
 
    for (int i = 0; i < MAX_CLIENTS; i++) 
    {
        if (clients[i] == NULL) 
        {
            clients[i] = client;
 
            /* Add to poll array */
            int slot = i + 1;
            fds[slot].fd = client->socket_fd;
            fds[slot].events = POLLIN;
 
            if (slot + 1 > nfds) 
            {
                nfds = slot + 1;
            }
 
            printf("[INFO] Client added: %s (fd=%d)\n", client->name, client->socket_fd);
            return 1;
        }
    }
 
    printf("[INFO] Server full, rejecting client\n");
    return 0;
}
 
// remove_client func()

void remove_client(client_t *client)
{
    if (client == NULL)
        return;
 
    /* Find and remove from clients array */
    for (int i = 0; i < MAX_CLIENTS; i++) 
    {
        if (clients[i] == client) 
        {
            clients[i] = NULL;
            fds[i + 1].fd = -1;
            printf("[INFO] Client removed: %s\n", client->name);
            break;
        }
    }
}
 
// client by name 

client_t *find_client_by_name(const char *name)
{
    if (name == NULL)
        return NULL;
 
    for (int i = 0; i < MAX_CLIENTS; i++) 
    {
        if (clients[i] != NULL && strcmp(clients[i]->name, name) == 0) 
        {
            return clients[i];
        }
    }
 
    return NULL;
}
 
// broadcasting the message

void broadcast_message(Message *msg, client_t *exclude)
{
    if (msg == NULL)
        return;
 
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != NULL && clients[i] != exclude) 
        {
            if (send_msg(clients[i]->socket_fd, msg) < 0) 
            {
        printf("[ERROR] Failed to send message to %s\n", clients[i]->name);
            }
        }
    }
}





