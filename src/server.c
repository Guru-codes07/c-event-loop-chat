/* System headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>

 
/* Custom headers */
#include "protocol.h"
#include "connections.h"
#include "commands.h"
#include "TLS.h"
#include "database.h"
 
/* Constants */
#define PORT 8080
#define BACKLOG 10
#define BUFFER_SIZE 1024
 
/* Global control variable */
static int running = 1;
 
/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */
 
void log_message(const char *format, ...)
{
    va_list args;
    va_start(args, format);
 
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
 
    printf("[%s] ", timestamp);
    vprintf(format, args);
    va_end(args);
}
 

/**
 * Handle new connection
 * Accepts socket, performs TLS handshake, then receives username
 */
void handle_new_connection(int listen_fd)
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
 
    int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        perror("accept failed");
        return;
    }
 
    log_message("New connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    
    /* Create SSL object for this connection */
    SSL *ssl = SSL_new(tls_server_ctx);
    if (ssl == NULL) {
        log_message("Failed to create SSL object\n");
        close(client_fd);
        return;
    }
    
    /* Associate socket with SSL */
    if (!SSL_set_fd(ssl, client_fd)) {
        log_message("Failed to set SSL file descriptor\n");
        SSL_free(ssl);
        close(client_fd);
        return;
    }
    
    /* Perform TLS handshake (blocking) */
    int hs_result = tls_handshake(ssl, 1);  /* 1 = server */
    if (hs_result != 1) {
        log_message("TLS handshake failed\n");
        SSL_free(ssl);
        close(client_fd);
        return;
    }
    
    log_message("TLS handshake successful for %s:%d\n",
                inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
 
    /* Receive MSG_CONNECTION message with username (now over TLS) */
    Message msg = {0};
    if (recv_msg(client_fd, ssl, &msg) < 0) 
    {
        log_message("Failed to receive connection message\n");
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(client_fd);
        return;
    }
 
    /* Validate message type */
    if (msg.type != MSG_CONNECTION) 
    {
        log_message("Expected MSG_CONNECTION, got type %d\n", msg.type);
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(client_fd);
        return;
    }
 
    /* Extract and validate username */
    msg.payload[msg.length] = '\0';
    strip_newline(msg.payload);
 
    if (strlen(msg.payload) == 0) 
    {
        Message error_msg = {0};
        error_msg.version = PROTOCOL_VERSION;
        error_msg.type = MSG_ERROR;
        error_msg.timestamp = time(NULL);
        strncpy(error_msg.payload, "Username cannot be empty", MAX_PAYLOAD_SIZE - 1);
        error_msg.length = strlen(error_msg.payload);
        send_msg(client_fd, ssl, &error_msg);
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(client_fd);
        return;
    }
 
    /* Check for duplicate username */
    if (find_client_by_name(msg.payload) != NULL) 
    {
        Message error_msg = {0};
        error_msg.version = PROTOCOL_VERSION;
        error_msg.type = MSG_ERROR;
        error_msg.timestamp = time(NULL);
        snprintf(error_msg.payload, MAX_PAYLOAD_SIZE,"Username '%.32s' already taken", msg.payload);
        error_msg.length = strlen(error_msg.payload);
        send_msg(client_fd, ssl, &error_msg);
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(client_fd);
        return;
    }
 
    /* Create new client structure */
    client_t *new_client = malloc(sizeof(client_t));
    if (new_client == NULL) 
    {
        perror("malloc failed");
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(client_fd);
        return;
    }
 
    memset(new_client, 0, sizeof(client_t));
    new_client->socket_fd = client_fd;
    new_client->ssl = ssl;  /* Store SSL object */
    new_client->tls_handshake_complete = 1;
    new_client->next_message_id = 1;
    new_client->last_activity = time(NULL);
    strncpy(new_client->name, msg.payload, NAME_SIZE - 1);
    new_client->name[NAME_SIZE - 1] = '\0';
 
    /* Try to add client to server */
    if (!add_client(new_client)) 
    {
        Message error_msg = {0};
        error_msg.version = PROTOCOL_VERSION;
        error_msg.type = MSG_ERROR;
        error_msg.timestamp = time(NULL);
        strncpy(error_msg.payload, "Server is full. Try again later.", MAX_PAYLOAD_SIZE - 1);
        error_msg.length = strlen(error_msg.payload);
        send_msg(client_fd, ssl, &error_msg);
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(client_fd);
        free(new_client);
        return;
    }
 
    /* Send welcome message over TLS */
    Message welcome = {0};
    welcome.version = PROTOCOL_VERSION;
    welcome.type = MSG_SERVER;
    welcome.message_id = new_client->next_message_id++;
    welcome.timestamp = time(NULL);
    snprintf(welcome.payload, MAX_PAYLOAD_SIZE,"Welcome %s! Type /who to see online users, /w <user>:<msg> to whisper.",new_client->name);
    welcome.length = strlen(welcome.payload);
    send_msg(new_client->socket_fd, new_client->ssl, &welcome);
 
    /* Announce join to all clients */
    Message join_msg = {0};
    join_msg.version = PROTOCOL_VERSION;
    join_msg.type = MSG_SERVER;
    join_msg.timestamp = time(NULL);
    snprintf(join_msg.payload, MAX_PAYLOAD_SIZE,"📣 %s has joined the chat.", new_client->name);
    join_msg.length = strlen(join_msg.payload);
 
    log_message("%s\n", join_msg.payload);
    broadcast_message(&join_msg, new_client);
}
 
/**
 * Handle data from existing client
 * Receives messages and routes to appropriate handlers
 */
void handle_client_data(client_t *client)
{
    if (client == NULL)
        return;
 
    Message msg = {0};
    int ret = receive_msg_nonblocking(client->socket_fd, client->ssl, &client->recv_buf, &msg);
 
    if (ret == 1) 
    {
        /* Complete message received */
        client->last_activity = time(NULL);
 
        /* Route message to appropriate handler */
        switch (msg.type) 
        {
            case MSG_CHAT:
                handle_msg_chat(client, &msg);
                break;
 
            case MSG_PRIVATE_MSG:
                handle_msg_private(client, &msg);
                break;
 
            case MSG_WHO_COMMAND:
                handle_msg_who(client, &msg);
                break;

            case MSG_HISTORY_COMMAND:
                handle_msg_history(client,&msg);
                break;    
 
            case MSG_DISCONNECT:
                handle_msg_disconnect(client,&msg);
                return;
 
            case MSG_CONNECTION:
                log_message("Unexpected MSG_CONNECTION from %s (already connected)\n",client->name);
                break;
 
            default:
                log_message("Unknown message type: %d from %s\n", msg.type, client->name);
        }
 
    } 
    else if (ret == -2) 
    {
        /* Peer closed connection gracefully */
        log_message("Connection closed by %s\n", client->name);
 
        Message leave_msg = {0};
        leave_msg.version = PROTOCOL_VERSION;
        leave_msg.type = MSG_SERVER;
        leave_msg.timestamp = time(NULL);
        snprintf(leave_msg.payload, MAX_PAYLOAD_SIZE,"📣 %s has left the chat.", client->name);
        leave_msg.length = strlen(leave_msg.payload);
 
        broadcast_message(&leave_msg, client);
 
        close(client->socket_fd);
        remove_client(client);
        free(client);
 
    } 
    
    else if (ret == -1) 
    {
        /* Protocol error (checksum failure, size violation, etc.) */
        log_message("Protocol error from %s\n", client->name);
 
        Message error_msg = {0};
        error_msg.version = PROTOCOL_VERSION;
        error_msg.type = MSG_ERROR;
        error_msg.timestamp = time(NULL);
        strncpy(error_msg.payload, "Protocol error detected", MAX_PAYLOAD_SIZE - 1);
        error_msg.length = strlen(error_msg.payload);
        send_msg(client->socket_fd, client->ssl, &error_msg);
 
        close(client->socket_fd);
        remove_client(client);
        free(client);
    }
    /* ret == 0: incomplete message, will retry on next poll cycle */
}
 

 // SIGNAL HANDLER

 void signal_handler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM) 
    {
        log_message("Shutdown signal received\n");
        running = 0;
    }
}

 /**
 * Main event loop
 * Usage: ./server [cert_file] [key_file]
 * 
 * Default paths: certs/server.crt, certs/server.key
 */
int main(int argc, char *argv[])
{
    const char *cert_file = "certs/server.crt";
    const char *key_file = "certs/server.key";
    
    /* Allow custom cert/key paths as arguments */
    if (argc >= 3) {
        cert_file = argv[1];
        key_file = argv[2];
    }
    
    /* Initialize clients array */
    for (int i = 0; i < MAX_CLIENTS; i++) 
    {
        clients[i] = NULL;
    }
 
    /* Register signal handlers */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    
    /* Initialize TLS */
    if (tls_init_server(cert_file, key_file) < 0) {
        fprintf(stderr, "Failed to initialize TLS\n");
        return 1;
    }
 
    /* Create server socket */
    int listen_fd = create_server_socket();
    if (listen_fd < 0) 
    {
        fprintf(stderr, "Failed to create listening socket\n");
        tls_cleanup();
        return 1;
    }

    /* Initialize database */
    if (db_init() < 0)
    {
        fprintf(stderr, "Failed to initialize database: %s\n", db_get_error());
        close(listen_fd);
        tls_cleanup();
        return 1;
    }
 
    /* Initialize poll array */
    memset(fds, 0, sizeof(fds));
    fds[0].fd = listen_fd;
    fds[0].events = POLLIN;
    nfds = 1;
 
    log_message("Server started. Waiting for connections...\n");
 
    /* Main poll event loop */
    while (running) 
    {
        int activity = poll(fds, nfds, -1);
 
        if (activity < 0) 
        {
            if (errno == EINTR) 
            {
                continue;  /* Interrupted by signal, retry */
            }
            perror("poll error");
            break;
        }
 
        /* Check for new connections on listening socket */
        if (fds[0].revents & POLLIN) 
        {
            handle_new_connection(listen_fd);
        }
 
        /* Check for data from existing clients */
        for (int i = 1; i < nfds; i++) 
        {
          if (fds[i].revents & POLLIN) 
          {
            client_t *client = fd_to_client[i];  /* Direct lookup! */
            if (client != NULL) 
            {
               handle_client_data(client);
            }
          }
        }
    }
 
    /* Cleanup on shutdown */
    log_message("Shutting down...\n");
 
    for (int i = 0; i < MAX_CLIENTS; i++) 
    {
        if (clients[i] != NULL) 
        {
            close(clients[i]->socket_fd);
            if (clients[i]->ssl != NULL) {
                SSL_shutdown(clients[i]->ssl);
                SSL_free(clients[i]->ssl);
            }
            free(clients[i]);
        }
    }
 
    close(listen_fd);

    /* Close database */
    if (db_cleanup() < 0)
    {
        fprintf(stderr, "Database cleanup error: %s\n", db_get_error());
    }
    
    /* Cleanup TLS */
    tls_cleanup();
    
    log_message("Server stopped\n");
 
    return 0;
}



