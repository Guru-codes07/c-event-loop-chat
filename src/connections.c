#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
 
#include "connections.h"
#include "protocol.h"
 
/* Global client table */
client_t *clients[MAX_CLIENTS] = {0};
client_t *fd_to_client[MAX_CLIENTS + 1];
 
/* Poll array for monitoring sockets */
struct pollfd fds[MAX_CLIENTS + 1] = {0};
int nfds = 0;

// add_client func():
int add_client(client_t *client)
{
    if (nfds >= MAX_CLIENTS + 1) 
    {
        /* Poll array is full; should not happen if clients[] and nfds
         * stay in sync, but guard against it anyway. */
        printf("[INFO] Server full, rejecting client\n");
        return 0;
    }

    for (int i = 0; i < MAX_CLIENTS; i++)
     {
        if (clients[i] == NULL) 
        {
            clients[i] = client;

            /* Add to poll array */
            fds[nfds].fd = client->socket_fd;
            fds[nfds].events = POLLIN;
            fd_to_client[nfds] = client;  /* Map index to client */
            client->poll_index = nfds;    /* Client remembers its own slot */
            nfds++;
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

    /* Find and remove from the logical clients[] table */
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i] == client)
        {
            clients[i] = NULL;
            break;
        }
    }

    /* Compact the poll array using this client's own remembered slot,
     * instead of assuming clients[] index and fds[] index line up
     * (they don't, once clients disconnect/reconnect out of order). */
    int idx = client->poll_index;
    int last = nfds - 1;

    if (idx != last)
    {
        /* Move the last live entry into the freed slot */
        fds[idx] = fds[last];
        fd_to_client[idx] = fd_to_client[last];
        fd_to_client[idx]->poll_index = idx;  /* moved client learns its new home */
    }

    fds[last].fd = -1;
    fd_to_client[last] = NULL;
    nfds--;

    printf("[INFO] Client removed: %s\n", client->name);
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





