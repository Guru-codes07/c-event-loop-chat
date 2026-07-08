// system header files:
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<openssl/ssl.h>
#include<openssl/err.h>

//custom header files:
#include "network.h"
#include "common.h"
#include "protocol.h"

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

/* Send a message over an SSL connection */
int send_msg(int socket_fd, SSL *ssl, Message *msg) {
    if (ssl == NULL) {
        /* Fallback to plain socket (shouldn't happen) */
        return send(socket_fd, (char *)msg, sizeof(Message), 0);
    }
    
    int bytes_sent = SSL_write(ssl, (char *)msg, sizeof(Message));
    
    if (bytes_sent <= 0) {
        int err = SSL_get_error(ssl, bytes_sent);
        if (err == SSL_ERROR_WANT_WRITE) {
            return 0;  /* Try again later */
        }
        fprintf(stderr, "SSL_write error: %d\n", err);
        return -1;
    }
    
    return bytes_sent;
}

/* Receive a message over an SSL connection */
int recv_msg(int socket_fd, SSL *ssl, Message *msg) {
    if (ssl == NULL) {
        return recv(socket_fd, (char *)msg, sizeof(Message), 0);
    }
    
    int bytes_recv = SSL_read(ssl, (char *)msg, sizeof(Message));
    
    if (bytes_recv <= 0) {
        int err = SSL_get_error(ssl, bytes_recv);
        
        if (err == SSL_ERROR_ZERO_RETURN) {
            return -2;  /* Peer closed connection */
        }
        
        if (err == SSL_ERROR_WANT_READ) {
            return 0;   /* Try again later (incomplete) */
        }
        
        fprintf(stderr, "SSL_read error: %d\n", err);
        return -1;
    }
    
    return bytes_recv;
}

/* Initialize SSL context on server */
SSL_CTX* init_ssl_context(const char *cert_file, const char *key_file) {
    /* Create SSL context */
    SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) {
        fprintf(stderr, "SSL_CTX_new failed\n");
        return NULL;
    }
    
    /* Load certificate file */
    if (SSL_CTX_use_certificate_file(ctx, cert_file, SSL_FILETYPE_PEM) <= 0) {
        fprintf(stderr, "Failed to load certificate: %s\n", cert_file);
        SSL_CTX_free(ctx);
        return NULL;
    }
    
    /* Load private key file */
    if (SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) <= 0) {
        fprintf(stderr, "Failed to load private key: %s\n", key_file);
        SSL_CTX_free(ctx);
        return NULL;
    }
    
    /* Verify that cert and key match */
    if (!SSL_CTX_check_private_key(ctx)) {
        fprintf(stderr, "Private key does not match certificate\n");
        SSL_CTX_free(ctx);
        return NULL;
    }
    
    printf("SSL context initialized successfully\n");
    return ctx;
}

/* Perform TLS handshake (can be called multiple times if it returns 0) */
int perform_ssl_handshake(SSL *ssl, int is_server) {
    int result;
    
    if (is_server) {
        result = SSL_accept(ssl);
    } else {
        result = SSL_connect(ssl);
    }
    
    if (result == 1) {
        return 1;  /* Handshake complete */
    }
    
    int err = SSL_get_error(ssl, result);
    
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
        return 0;  /* Handshake in progress, try again later */
    }
    
    fprintf(stderr, "SSL handshake failed: %d\n", err);
    return -1;
}

// to remove \n or \r from a string
void strip_newline(char *str)
{
    str[strcspn(str,"\r\n")] = '\0';
}

