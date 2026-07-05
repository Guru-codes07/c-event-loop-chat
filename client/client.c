#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<poll.h>
#define PORT 8080
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024
 
int main()
{
    // creating a socket
    int client_socket = socket(AF_INET,SOCK_STREAM,0);
    if(client_socket<0)
    {
      perror("socket failed");
      return 1;
    }
    
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET,SERVER_IP,&server_addr.sin_addr);
 
    // connecting the socket
    if(connect(client_socket,(struct sockaddr*)&server_addr,sizeof(server_addr))<0)
    {
        perror("connection failed");
        return 1;
    }
    printf("connected to the server on %s:%d\n",SERVER_IP,PORT);
 
    // ask for username
    char username[50];
    printf("Enter your username: ");
    fflush(stdout);
 
    if(fgets(username,sizeof(username),stdin)==NULL)
    {
        close(client_socket);
        return 1;
    }
    username[strcspn(username,"\n")] = '\0';
 
    // send username to the server
    char username_msg[67];
    snprintf(username_msg,sizeof(username_msg),"%s\n",username);
    if(send(client_socket,username_msg,strlen(username_msg),0)<0)
    {
        perror("send failed");
        close(client_socket);
        return 1;
    }
     
    // initialise poll array 
    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = client_socket;
    fds[1].events = POLLIN;
 
    char buffer[BUFFER_SIZE];
    
    // main poll() loop 
    while(1)
 {
       int activity = poll(fds,2,-1);
       if(activity<0)
       {
        perror("poll error");
        break;
       }
    
    
    if(fds[0].revents & POLLIN)
  {  
        memset(buffer,0,sizeof(buffer));
        if(fgets(buffer,sizeof(buffer),stdin)==NULL)
        {
         break;
        }
       buffer[strcspn(buffer,"\n")] = '\0';
 
       // the exit command
       if(strcmp(buffer,"exit")==0)
       {
        printf("Goodbye\n");
        break;
       }
    
    
    // send message to the server
    char msg_to_send[BUFFER_SIZE];
    snprintf(msg_to_send,sizeof(msg_to_send),"%.*s\n", BUFFER_SIZE - 2, buffer);
    if(send(client_socket,msg_to_send,strlen(msg_to_send),0)<0)
    {
        perror("send error");
        break;
    }
    printf("you: ");
    fflush(stdout);
}
 
        if (fds[1].revents & POLLIN) 
        {
            memset(buffer, 0, sizeof(buffer));
            
            int bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            
            if (bytes_read <= 0) 
            {
                printf("\nServer disconnected.\n");
                break;
            }
            
            buffer[bytes_read] = '\0';
            
            /* Print server message and prompt again */
            printf("\r%s\n", buffer);
            printf("you: ");
            fflush(stdout);
        }
    }
    
    close(client_socket);
    return 0;
}    


