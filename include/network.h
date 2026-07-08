
#ifndef NETWORK_H
#define NETWORK_H
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "protocol.h"
#include "connections.h"
 
// global SSL context which is shared by all connections
extern SSL_CTX *ssl_ctx;
extern client_t *clients[100];
 
/* creating a socket thhat binds to a port and listsens for
 an incoming client */
int create_server_socket(void);
 
// TLS/SSL functions()
SSL_CTX* init_ssl_context(const char *cert_file, const char *key_file);
int perform_ssl_handshake(SSL *ssl, int is_server);
 
// message functions now with TLS
int send_msg(int socket_fd, SSL *ssl, Message *msg);
int recv_msg(int socket_fd, SSL *ssl, Message *msg);
 
 
/* removing \n or \r from a string*/
void strip_newline(char *str);
 
#endif
