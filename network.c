// system header files:
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<netinet/in.h>

//custom header files:
#include "network.h"
#include "common.h"

int create_server_socket(void)
{
    int listen_fd = socket(AF_INET,SOCK_STREAM,0);
    if(listen_fd <0)
    {
        perror("socket failure");
        return -1;
    }

    /* allowing the socket to be reused immediately after the
    program terminates , avoids address already in use error */ 
    int opt = 1;
    if(setsockopt(listen_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt))<0)
    {
        perror("setsockopt failure");
        close(listen_fd);
        return -1;
    }
  
    // server address structure
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // binding the socket to the specified port
    if(bind(listen_fd,(struct sockaddr*)&server_addr,sizeof(server_addr))<0)
    {
        perror("bind failure");
        close(listen_fd);
        return -1;
    }
    
    // listening for incoming connections
    if(listen(listen_fd,MAX_CLIENTS)<0)
    {
        perror("listen failure");
        close(listen_fd);
        return -1;
    }
    printf("The server is listening on port %d (fd = %d)\n",PORT,listen_fd);
    return listen_fd;
}

// to remove \n or \r from a string
void strip_newline(char *str)
{
    str[strcspn(str,"\r\n")] = '\0';
}

