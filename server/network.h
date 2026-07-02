#ifndef NETWORK_H
#define NETWORK_H

/* creating a socket thhat binds to a port and listsens for
 an incoming client */
int create_server_socket(void);

/* removing \n or \r from a string*/
void strip_newline(char *str);

#endif