// system headers
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<poll.h> 

// custom headers 
#include "common.h"
#include "connections.h"
#include "network.h"
#include "commands.h"

/* HANDLE NEW CONNECTIONS
* implement basic scoket functions
* using recv() functions
* creating client structure 
* broadcast the message that "user joined"  */
void handle_new_connection(int listen_fd)
{
    // client address structure
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
 // accept new connection
 int client_fd = accept(listen_fd,(struct sockaddr*)&client_addr,&client_len);
 if(client_fd<0)
 {
    perror("accept failed");
    return;
 }
  
 // reading the username from the client
 char buffer[BUFFER_SIZE];
  int bytes_read = recv(client_fd,buffer,BUFFER_SIZE-1,0);
  if(bytes_read<=0)
  {
    perror("recv error");
    close(client_fd);
    return;
  }

  // null terminate the buffer
 buffer[bytes_read] = '\0';
 strip_newline(buffer);

 // create and initialize the client struct using malloc()
 client_t *new_client = malloc(sizeof(client_t));
 if(new_client == NULL)
 {
    perror("malloc failed");
    close(client_fd);
    return;
 }

 // removing garbage data
 memset(new_client,0,sizeof(client_t));
 (*new_client).socket_fd = client_fd;
 strncpy((*new_client).name,buffer,NAME_SIZE -1);
 (*new_client).name[NAME_SIZE-1] = '\0'; // ensure null termination

 // if the server is full, reject the new connection
 if(!add_client(new_client))
 {
    char *full_msg = "The server is full, try again later. \n";
    send(client_fd,full_msg,strlen(full_msg),0);
    close(client_fd);
    free(new_client);
    return;
 } 

  /* Send welcome message to the new client */
    char welcome[BUFFER_SIZE];
    snprintf(welcome, sizeof(welcome), "Welcome %s! Type /who to see online users, /w <user> <msg> to whisper.\n", (*new_client).name);
    send((*new_client).socket_fd, welcome, strlen(welcome), 0);

 /* Announcing the joining of neew users */
 char join_msg[BUFFER_SIZE + NAME_SIZE];
 snprintf(join_msg,sizeof(join_msg),"📣 %s has joined the chat. \n",(*new_client).name);
 printf("%s",join_msg);
 broadcast_message(join_msg,new_client);

}

/* HANDLE CLIENT DATA
* The client socket is ready for reading
* recv() function from the client 
* if recv() fails, remove the client
* otherwise use the handle_command() function */
void handle_client_data(client_t *client)
{
    char buffer[BUFFER_SIZE];
    char message[BUFFER_SIZE + NAME_SIZE];
    
    // remove garbage data
    memset(buffer,0,sizeof(buffer));
    int bytes_read = recv((*client).socket_fd,buffer,BUFFER_SIZE-1,0);
    if(bytes_read <= 0)
    {
        close((*client).socket_fd);
        remove_client(client);

        snprintf(message,sizeof(message),"📣 %s has left the chat. \n",(*client).name);
        printf("%s",message);
        broadcast_message(message,client);
        free(client);
        return;
    } 
     
    buffer[bytes_read] = '\0';
    
    // commands for /who and /w
    handle_command(client,buffer);
}

/* MAIN FUNCTION AND POLL() FUNCT LOOP */
int main(void)
{
 
    // create a listening socket
    int listen_fd = create_server_socket();
    if(listen_fd < 0)
    {
        fprintf(stderr,"Failed to create listening socket\n");
        return 1;
    }
    
    for(int i=0;i<MAX_CLIENTS+1;i++)
    {
        fds[i].fd = -1;      // mark all fds as unused
    }
    fds[0].fd = listen_fd;   // first fd is the listening socket
    fds[0].events = POLLIN;  // monitor for incoming connections
     nfds = 1;            // number of fds in the array

    /* MAIN POLL LOOP */
    while(1)
 {
        int activity = poll(fds,nfds,-1);
        if(activity < 0)
        {
            perror("poll error");
            continue;
        }

 
       if(fds[0].revents & POLLIN)
     {
        handle_new_connection(listen_fd);
     }

     for(int i=1;i<nfds;i++)
    {
    if(fds[i].fd == -1)
    {
      continue;
    }
     if(fds[i].revents & POLLIN)
    {
     handle_client_data(clients[i-1]);
    }
   }
  }    

  close(listen_fd);
  return 0;
}


