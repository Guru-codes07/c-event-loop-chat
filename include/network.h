
#ifndef NETWORK_H
#define NETWORK_H
#include "protocol.h"
#include "connections.h"

/* creating a socket thhat binds to a port and listsens for
 an incoming client */
int create_server_socket(void);
 
 // message functions now with TLS
int send_msg(int socket_fd, SSL *ssl, Message *msg);
int recv_msg(int socket_fd, SSL *ssl, Message *msg);
 
 
/* removing \n or \r from a string*/
void strip_newline(char *str);
 
#endif
