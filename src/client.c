#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define PORT 8080
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024
#define MAX_PAYLOAD_SIZE 1024
#define PROTOCOL_VERSION 1
#define NAME_SIZE 32

/* Message types */
typedef enum 
{
    MSG_CONNECTION = 1,
    MSG_CHAT = 2,
    MSG_PRIVATE_MSG = 3,
    MSG_WHO_COMMAND = 4,
    MSG_DISCONNECT = 5,
    MSG_SERVER = 6,
    MSG_ERROR = 7,
    MSG_HISTORY_COMMAND = 8
} MessageType;

/* Message structure */
typedef struct 
{
    uint8_t version;
    uint8_t type;
    uint8_t flags;
    uint8_t _reserved;
    uint16_t length;
    uint16_t _reserved2;
    uint32_t message_id;
    uint32_t timestamp;
    uint32_t crc32;
    char payload[MAX_PAYLOAD_SIZE];
} Message;

/**
 * Calculate CRC32 checksum
 */
uint32_t calculate_crc32(const char *data, uint16_t length)
{
    uint32_t crc = 0xFFFFFFFF;
    
    for (uint16_t i = 0; i < length; i++) 
    {
        uint8_t byte = data[i];
        crc ^= byte;
        
        for (int j = 0; j < 8; j++) 
        {
            if (crc & 1) 
            {
                crc = (crc >> 1) ^ 0xEDB88320;
            } 
            else 
            {
                crc >>= 1;
            }
        }
    }
    
    return crc ^ 0xFFFFFFFF;
}

/**
 * Remove trailing newline
 */
void strip_newline(char *str)
{
    str[strcspn(str, "\r\n")] = '\0';
}

/**
 * Send all bytes over TLS
 */
static int send_all(SSL *ssl, const void *buffer, size_t length)
{
    const char *data = (const char *)buffer;
    size_t total = 0;
    
    while (total < length) 
    {
        int bytes = SSL_write(ssl, data + total, length - total);
        
        if (bytes <= 0)
        {
            int err = SSL_get_error(ssl, bytes);
            if (err == SSL_ERROR_WANT_WRITE) {
                usleep(10000);
                continue;
            }
            perror("[CLIENT] SSL_write");
            return -1;
        }
        
        total += bytes;
    }
    
    return 0;
}

/**
 * Receive all bytes over TLS
 */
static int recv_all(SSL *ssl, void *buffer, size_t length)
{
    char *data = (char *)buffer;
    size_t total = 0;
    
    while (total < length) 
    {
        int bytes = SSL_read(ssl, data + total, length - total);
        
        if (bytes <= 0) 
        {
            int err = SSL_get_error(ssl, bytes);
            if (err == SSL_ERROR_ZERO_RETURN) {
                return -2;  /* Connection closed */
            }
            if (err == SSL_ERROR_WANT_READ) {
                usleep(10000);
                continue;
            }
            perror("[CLIENT] SSL_read");
            return -1;
        }
        
        total += bytes;
    }
    
    return 0;
}

/**
 * Send message over TLS
 */
int send_msg(SSL *ssl, Message *msg)
{
    if (msg == NULL || ssl == NULL) 
    {
        fprintf(stderr, "[CLIENT] Invalid SSL or message\n");
        return -1;
    }
    
    uint8_t header[24];
    
    /* Build header */
    header[0] = PROTOCOL_VERSION;
    header[1] = msg->type;
    header[2] = msg->flags;
    header[3] = 0;
    
    uint16_t len = htons(msg->length);
    memcpy(header + 4, &len, 2);
    memcpy(header + 6, &(uint16_t){0}, 2);
    
    uint32_t msg_id = htonl(msg->message_id);
    memcpy(header + 8, &msg_id, 4);
    
    uint32_t ts = htonl(msg->timestamp);
    memcpy(header + 12, &ts, 4);
    
    msg->crc32 = calculate_crc32(msg->payload, msg->length);
    uint32_t crc = htonl(msg->crc32);
    memcpy(header + 16, &crc, 4);
    memcpy(header + 20, &(uint32_t){0}, 4);
    
    if (send_all(ssl, header, 24) < 0)
        return -1;
    
    if (send_all(ssl, msg->payload, msg->length) < 0)
        return -1;
    
    return 0;
}

/**
 * Receive message over TLS
 */
int recv_msg(SSL *ssl, Message *msg)
{
    if (msg == NULL || ssl == NULL) 
    {
        fprintf(stderr, "[CLIENT] Invalid SSL or message\n");
        return -1;
    }
    
    uint8_t header[24];
    
    /* Receive header */
    int ret = recv_all(ssl, header, 24);
    if (ret < 0)
        return ret;
    
    /* Parse header */
    msg->version = header[0];
    msg->type = header[1];
    msg->flags = header[2];
    
    memcpy(&msg->length, header + 4, 2);
    msg->length = ntohs(msg->length);
    
    if (msg->length > MAX_PAYLOAD_SIZE) 
    {
        fprintf(stderr, "[CLIENT] Payload size %u exceeds maximum %d\n", 
                msg->length, MAX_PAYLOAD_SIZE);
        return -1;
    }
    
    uint32_t msg_id;
    memcpy(&msg_id, header + 8, 4);
    msg->message_id = ntohl(msg_id);
    
    uint32_t ts;
    memcpy(&ts, header + 12, 4);
    msg->timestamp = ntohl(ts);
    
    uint32_t crc;
    memcpy(&crc, header + 16, 4);
    msg->crc32 = ntohl(crc);
    
    /* Receive payload */
    if (msg->length > 0) 
    {
        ret = recv_all(ssl, msg->payload, msg->length);
        if (ret < 0)
            return ret;
    }
    
    msg->payload[msg->length] = '\0';
    
    /* Verify CRC32 */
    uint32_t calculated = calculate_crc32(msg->payload, msg->length);
    if (calculated != msg->crc32) 
    {
        fprintf(stderr, "[CLIENT] CRC32 mismatch\n");
        return -1;
    }
    
    return 0;
}

/**
 * Show help
 */
void show_help(void)
{
    printf("\n");
    printf("╔════════════════════════════════════════════╗\n");
    printf("║          AVAILABLE COMMANDS                ║\n");
    printf("╠════════════════════════════════════════════╣\n");
    printf("║ /who                  - List online users  ║\n");
    printf("║ /w <user>:<message>   - Send private msg   ║\n");
    printf("║ /history              - Show chat history  ║\n");
    printf("║ /help                 - Display this help  ║\n");
    printf("║ exit                  - Disconnect         ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");
}

/**
 * Parse user input and send message
 */
int parse_and_send_message(SSL *ssl, char *input, uint32_t *msg_id)
{
    if (input == NULL || strlen(input) == 0)
        return 0;
    
    Message msg = {0};
    msg.version = PROTOCOL_VERSION;
    msg.message_id = (*msg_id)++;
    msg.timestamp = time(NULL);
    
    if (input[0] == '/') 
    {
        if (strncmp(input, "/who", 4) == 0) 
        {
            msg.type = MSG_WHO_COMMAND;
            msg.length = 0;
        } 
        else if (strncmp(input, "/w ", 3) == 0) 
        {
            char *rest = input + 3;
            
            if (strchr(rest, ':') == NULL) 
            {
                printf("[ERROR] Format: /w <user>:<message>\n");
                return -1;
            }
            
            strncpy(msg.payload, rest, MAX_PAYLOAD_SIZE - 1);
            msg.payload[MAX_PAYLOAD_SIZE - 1] = '\0';
            msg.type = MSG_PRIVATE_MSG;
            msg.length = strlen(msg.payload);
        }
        else if (strncmp(input, "/history", 8) == 0) 
        {
            msg.type = MSG_HISTORY_COMMAND;
            msg.length = 0;
        }
        else if (strcmp(input, "/help") == 0) 
        {
            show_help();
            return 1;
        }
        else 
        {
            printf("[ERROR] Unknown command '%s'. Type /help for commands.\n", input);
            return -1;
        }
    } 
    else if (strcmp(input, "exit") == 0) 
    {
        return 1;
    }
    else 
    {
        msg.type = MSG_CHAT;
        strncpy(msg.payload, input, MAX_PAYLOAD_SIZE - 1);
        msg.payload[MAX_PAYLOAD_SIZE - 1] = '\0';
        msg.length = strlen(msg.payload);
    }
    
    if (send_msg(ssl, &msg) < 0) 
    {
        printf("[ERROR] Failed to send message\n");
        return -1;
    }
    
    return 0;
}

/**
 * Handle server message
 */
void handle_server_message(const Message *msg)
{
    if (msg == NULL)
        return;
    
    switch (msg->type) {
        case MSG_CHAT:
            printf("\r%s\n", msg->payload);
            break;
        
        case MSG_PRIVATE_MSG:
            printf("\r🔒 %s\n", msg->payload);
            break;
        
        case MSG_SERVER:
            printf("\r[SERVER] %s\n", msg->payload);
            break;
        
        case MSG_ERROR:
            printf("\r❌ %s\n", msg->payload);
            break;
        
        default:
            printf("\r[MESSAGE TYPE %d] %s\n", msg->type, msg->payload);
            break;
    }
}

/**
 * Main client function
 */
int main(void)
{
    /* Initialize OpenSSL */
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    
    /* Create SSL context */
    SSL_CTX *ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (ssl_ctx == NULL) 
    {
        fprintf(stderr, "[CLIENT] Failed to create SSL context\n");
        return 1;
    }
    
    /* Disable peer verification for testing */
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);
    
    /* Create socket */
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) 
    {
        perror("[CLIENT] socket");
        SSL_CTX_free(ssl_ctx);
        return 1;
    }
    
    /* Connect to server */
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) 
    {
        fprintf(stderr, "[CLIENT] Invalid server IP\n");
        close(client_socket);
        SSL_CTX_free(ssl_ctx);
        return 1;
    }
    
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) 
    {
        perror("[CLIENT] connect");
        close(client_socket);
        SSL_CTX_free(ssl_ctx);
        return 1;
    }
    
    printf("✓ Connected to server at %s:%d\n", SERVER_IP, PORT);
    
    /* Create SSL connection */
    SSL *ssl = SSL_new(ssl_ctx);
    if (ssl == NULL) 
    {
        fprintf(stderr, "[CLIENT] Failed to create SSL\n");
        close(client_socket);
        SSL_CTX_free(ssl_ctx);
        return 1;
    }
    
    SSL_set_fd(ssl, client_socket);
    
    /* Perform TLS handshake */
    printf("Performing TLS handshake...\n");
    if (SSL_connect(ssl) <= 0) 
    {
        fprintf(stderr, "[CLIENT] TLS handshake failed\n");
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(client_socket);
        SSL_CTX_free(ssl_ctx);
        return 1;
    }
    
    printf("✓ TLS handshake successful\n");
    printf("✓ Protocol: %s\n", SSL_get_version(ssl));
    printf("✓ Cipher: %s\n\n", SSL_get_cipher(ssl));
    
    /* Get username */
    char username[NAME_SIZE];
    printf("Enter username: ");
    fflush(stdout);
    
    if (fgets(username, sizeof(username), stdin) == NULL) 
    {
        fprintf(stderr, "[CLIENT] Error reading username\n");
        SSL_free(ssl);
        close(client_socket);
        SSL_CTX_free(ssl_ctx);
        return 1;
    }
    
    strip_newline(username);
    
    if (strlen(username) == 0) 
    {
        fprintf(stderr, "[CLIENT] Username cannot be empty\n");
        SSL_free(ssl);
        close(client_socket);
        SSL_CTX_free(ssl_ctx);
        return 1;
    }
    
    if (strlen(username) >= NAME_SIZE) 
    {
        fprintf(stderr, "[CLIENT] Username too long\n");
        SSL_free(ssl);
        close(client_socket);
        SSL_CTX_free(ssl_ctx);
        return 1;
    }
    
    /* Send connection message */
    Message conn_msg = {0};
    conn_msg.version = PROTOCOL_VERSION;
    conn_msg.type = MSG_CONNECTION;
    conn_msg.message_id = 1;
    conn_msg.timestamp = time(NULL);
    strncpy(conn_msg.payload, username, NAME_SIZE - 1);
    conn_msg.payload[NAME_SIZE - 1] = '\0';
    conn_msg.length = strlen(conn_msg.payload);
    
    if (send_msg(ssl, &conn_msg) < 0) 
    {
        fprintf(stderr, "[CLIENT] Failed to send connection message\n");
        SSL_free(ssl);
        close(client_socket);
        SSL_CTX_free(ssl_ctx);
        return 1;
    }
    
    /* Initialize poll */
    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = client_socket;
    fds[1].events = POLLIN;
    
    char input_buffer[BUFFER_SIZE];
    uint32_t next_msg_id = 2;
    
    printf("\n");
    show_help();
    printf("Type 'exit' to quit.\n");
    printf("%s> ", username);
    fflush(stdout);
    
    /* Main event loop */
    int running = 1;
    while (running) 
    {
        int activity = poll(fds, 2, -1);
        
        if (activity < 0) 
        {
            if (errno == EINTR)
                continue;
            perror("[CLIENT] poll");
            break;
        }
        
        /* Handle user input */
        if (fds[0].revents & POLLIN)
        {
            memset(input_buffer, 0, sizeof(input_buffer));
            
            if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL)
                break;
            
            strip_newline(input_buffer);
            
            if (strcmp(input_buffer, "exit") == 0) 
            {
                printf("\nDisconnecting...\n");
                running = 0;
                break;
            }
            
            int ret = parse_and_send_message(ssl, input_buffer, &next_msg_id);
            
            if (ret == 0) 
            {
                printf("%s> ", username);
                fflush(stdout);
            } 
            else if (ret == 1) 
            {
                printf("%s> ", username);
                fflush(stdout);
            }
        }
        
        /* Handle server messages */
        if (fds[1].revents & POLLIN) 
        {
            Message recv_msg_data = {0};
            int ret = recv_msg(ssl, &recv_msg_data);
            
            if (ret == -2)
            {
                printf("\n[SERVER] Connection closed\n");
                running = 0;
            } 
            else if (ret < 0) 
            {
                printf("\n[ERROR] Protocol error\n");
                running = 0;
            } 
            else 
            {
                handle_server_message(&recv_msg_data);
                printf("%s> ", username);
                fflush(stdout);
            }
        }
    }
    
    /* Send disconnect message */
    Message disc_msg = {0};
    disc_msg.version = PROTOCOL_VERSION;
    disc_msg.type = MSG_DISCONNECT;
    disc_msg.message_id = next_msg_id++;
    disc_msg.timestamp = time(NULL);
    disc_msg.length = 0;
    
    send_msg(ssl, &disc_msg);
    
    /* Cleanup */
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(client_socket);
    SSL_CTX_free(ssl_ctx);
    
    printf("Goodbye!\n");
    
    return 0;
}


