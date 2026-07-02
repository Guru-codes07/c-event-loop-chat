#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include<sys/socket.h>
#include<unistd.h>

#include "connections.h"
#include "common.h"

// list of all connected clients
client_t *clients[MAX_CLIENTS] = {0};

// declaring poll() structure
struct pollfd fds[MAX_CLIENTS + 1] = {0};
int nfds = 0;

// add a new client to the clients array
int add_client(client_t *client)
{
    for(int i=0; i<MAX_CLIENTS; i++)
    {
        if(clients[i]==NULL)
        {
            clients[i] =  client;
            int slot = i+1;
            fds[slot].fd = (*client).socket_fd;
            fds[slot].events = POLLIN;
            if(slot+1>nfds)
            {
                nfds = slot + 1;
            }
          return 1;
        }
    }
       return 0;
}

// remove a client from the clients array
void remove_client(client_t *client)
{
    for(int i=0;i<MAX_CLIENTS;i++)
    {
        if(clients[i]==client)
        {
            clients[i] = NULL;
            fds[i+1].fd = -1;
            break;
        }
    }
}

// broadcast a message to all connected clients except the sender
void broadcast_message(const char *message,client_t *sender)
{
    for(int i=0;i<MAX_CLIENTS;i++)
    {
        if(clients[i]!=NULL && clients[i]!= sender)
        {
            send((*clients[i]).socket_fd,message,strlen(message),0);
        }
    }
}

// show all connected clients
void send_online_list(client_t *client)
{
    char list[BUFFER_SIZE] = "online users:\n";
    for(int i=0;i<MAX_CLIENTS;i++)
    {
        if(clients[i]!=NULL)
        {
            strncat(list,"-",sizeof(list)-strlen(list)-1);
            strncat(list,(*clients[i]).name,sizeof(list)-strlen(list)-1);
            strncat(list,"\n",sizeof(list)-strlen(list)-1);
        }
    }
    send((*client).socket_fd,list,strlen(list),0);
}

// send private messages to a specific client
int send_private_message(client_t *sender,const char *target_name,const char *text)
{
    char format[BUFFER_SIZE + NAME_SIZE];
    snprintf(format,sizeof(format),"whisper from [%s]: %s",(*sender).name,text);
    
    for(int i=0;i<MAX_CLIENTS;i++)
    {
        if(clients[i]!= NULL && strcmp((*clients[i]).name,target_name)==0)
        {
            send((*clients[i]).socket_fd,format,strlen(format),0);
            return 1;
        }
    }
    return 0;
}




