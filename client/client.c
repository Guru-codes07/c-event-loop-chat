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

/* Message structure (24-byte header + payload) */
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
 * Calculate CRC32 checksum for message integrity
 * Uses polynomial 0xEDB88320 (reflected CRC32)
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
 * Remove trailing newline or carriage return from string
 */
void strip_newline(char *str)
{
    str[strcspn(str, "\r\n")] = '\0';
}

/**
 * Send all bytes (ensures complete transmission)
 * Blocks until entire buffer is sent or error occurs
 */
static int send_all(int sockfd, const void *buffer, size_t length)
{
    size_t total = 0;
    
    while (total < length) 
    {
        ssize_t bytes = send(sockfd, (const char *)buffer + total, length - total, 0);
        
        if (bytes <= 0)
         {
            perror("send");
            return -1;
        }
        
        total += bytes;
    }
    
    return 0;
}

/**
 * Receive all bytes (ensures complete reception)
 * Blocks until entire buffer is received or error occurs
 */
static int recv_all(int sockfd, void *buffer, size_t length)
{
    size_t total = 0;
    
    while (total < length) 
    {
        ssize_t bytes = recv(sockfd, (char *)buffer + total, length - total, 0);
        
        if (bytes <= 0) 
         {
            if (bytes == 0) 
            {
                /* Connection closed by server */
                return -2;
            }
            perror("recv");
            return -1;
        }
        
        total += bytes;
    }
    
    return 0;
}

/**
 * Send a complete message with binary protocol encoding
 * Header (24 bytes) + Payload (variable)
 * 
 * Header layout:
 *   [0]     version
 *   [1]     type
 *   [2]     flags
 *   [3]     reserved
 *   [4-5]   length (network byte order)
 *   [6-7]   reserved
 *   [8-11]  message_id (network byte order)
 *   [12-15] timestamp (network byte order)
 *   [16-19] crc32 (network byte order)
 *   [20-23] reserved
 */
int send_msg(int sockfd, Message *msg)
{
    if (msg == NULL || sockfd < 0) 
    {
        fprintf(stderr, "Invalid socket or message\n");
        return -1;
    }
    
    uint8_t header[24];
    
    /* Build header */
    header[0] = PROTOCOL_VERSION;
    header[1] = msg->type;
    header[2] = msg->flags;
    header[3] = 0;  /* Reserved */
    
    uint16_t len = htons(msg->length);
    memcpy(header + 4, &len, 2);
    memcpy(header + 6, &(uint16_t){0}, 2);  /* Reserved */
    
    uint32_t msg_id = htonl(msg->message_id);
    memcpy(header + 8, &msg_id, 4);
    
    uint32_t ts = htonl(msg->timestamp);
    memcpy(header + 12, &ts, 4);
    
    /* Calculate and encode CRC32 */
    msg->crc32 = calculate_crc32(msg->payload, msg->length);
    uint32_t crc = htonl(msg->crc32);
    memcpy(header + 16, &crc, 4);
    memcpy(header + 20, &(uint32_t){0}, 4);  /* Reserved */
    
    /* Send header and payload */
    if (send_all(sockfd, header, 24) < 0)
        return -1;
    
    if (send_all(sockfd, msg->payload, msg->length) < 0)
        return -1;
    
    return 0;
}

/**
 * Receive a complete message with binary protocol decoding
 * Blocking call — waits for full message or error
 */
int recv_msg(int sockfd, Message *msg)
{
    if (msg == NULL || sockfd < 0) 
    {
        fprintf(stderr, "Invalid socket or message\n");
        return -1;
    }
    
    uint8_t header[24];
    
    /* Receive header */
    int ret = recv_all(sockfd, header, 24);
    if (ret < 0)
        return ret;  /* -1 = error, -2 = connection closed */
    
    /* Parse header */
    msg->version = header[0];
    msg->type = header[1];
    msg->flags = header[2];
    
    memcpy(&msg->length, header + 4, 2);
    msg->length = ntohs(msg->length);
    
    /* Validate payload size */
    if (msg->length > MAX_PAYLOAD_SIZE) 
    {
        fprintf(stderr, "Payload size %u exceeds maximum %d\n", 
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
        ret = recv_all(sockfd, msg->payload, msg->length);
        if (ret < 0)
            return ret;
    }
    
    msg->payload[msg->length] = '\0';
    
    /* Verify CRC32 */
    uint32_t calculated = calculate_crc32(msg->payload, msg->length);
    if (calculated != msg->crc32) 
    {
        fprintf(stderr, "CRC32 checksum mismatch (expected %u, got %u)\n", 
                msg->crc32, calculated);
        return -1;
    }
    
    return 0;
}

/**
 * Display help message with available commands
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
 * Parse user input and construct appropriate message
 * Handles commands (/who, /w, /history, /help, exit) and regular chat
 * 
 * Returns: 0 = message sent, 1 = local command handled, -1 = error
 */
int parse_and_send_message(int sockfd, char *input, uint32_t *msg_id)
{
    if (input == NULL || strlen(input) == 0)
        return 0;  /* Empty input, ignore */
    
    Message msg = {0};
    msg.version = PROTOCOL_VERSION;
    msg.message_id = (*msg_id)++;
    msg.timestamp = time(NULL);
    
    /* Check for commands */
    if (input[0] == '/') 
    {
        if (strncmp(input, "/who", 4) == 0) 
        {
            /* List online users */
            msg.type = MSG_WHO_COMMAND;
            msg.length = 0;
        } 
        else if (strncmp(input, "/w ", 3) == 0) 
        {
            /* Private message: /w recipient:message */
            char *rest = input + 3;  /* Skip "/w " */
            
            /* Validate format: must contain ':' */
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
            /* Request chat history */
            msg.type = MSG_HISTORY_COMMAND;
            msg.length = 0;
        }
        else if (strcmp(input, "/help") == 0) 
        {
            /* Show help (local command, don't send to server) */
            show_help();
            return 1;  /* Local command handled */
        }
        else 
        {
            printf("[ERROR] Unknown command '%s'. Type /help for available commands.\n", input);
            return -1;
        }
    } 
    else if (strcmp(input, "exit") == 0) 
    {
        /* Handled separately in main loop */
        return 1;
    }
    else 
    {
        /* Regular chat message */
        msg.type = MSG_CHAT;
        strncpy(msg.payload, input, MAX_PAYLOAD_SIZE - 1);
        msg.payload[MAX_PAYLOAD_SIZE - 1] = '\0';
        msg.length = strlen(msg.payload);
    }
    
    /* Send message to server */
    if (send_msg(sockfd, &msg) < 0) 
    {
        printf("[ERROR] Failed to send message to server.\n");
        return -1;
    }
    
    return 0;  /* Message sent successfully */
}

/**
 * Handle server messages based on message type
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
        
        case MSG_HISTORY_COMMAND:
            /* Server sends history as MSG_SERVER messages */
            printf("\r%s\n", msg->payload);
            break;
        
        default:
            printf("\r[MESSAGE TYPE %d] %s\n", msg->type, msg->payload);
            break;
    }
}

// THE MAIN() 
int main(void)
{
    /* Create socket */
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("socket");
        return 1;
    }
    
    /* Connect to server */
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) 
    {
        fprintf(stderr, "Invalid server IP address: %s\n", SERVER_IP);
        close(client_socket);
        return 1;
    }
    
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) 
    {
        perror("connect");
        close(client_socket);
        return 1;
    }
    
    printf("✓ Connected to server at %s:%d\n", SERVER_IP, PORT);
    
    /* Get username from user */
    char username[NAME_SIZE];
    printf("\nEnter username: ");
    fflush(stdout);
    
    if (fgets(username, sizeof(username), stdin) == NULL) 
    {
        fprintf(stderr, "Error reading username\n");
        close(client_socket);
        return 1;
    }
    
    strip_newline(username);
    
    /* Validate username */
    if (strlen(username) == 0) 
    {
        fprintf(stderr, "Username cannot be empty\n");
        close(client_socket);
        return 1;
    }
    
    if (strlen(username) >= NAME_SIZE) 
    {
        fprintf(stderr, "Username too long (max %d characters)\n", NAME_SIZE - 1);
        close(client_socket);
        return 1;
    }
    
    /* Send connection message with username */
    Message conn_msg = {0};
    conn_msg.version = PROTOCOL_VERSION;
    conn_msg.type = MSG_CONNECTION;
    conn_msg.message_id = 1;
    conn_msg.timestamp = time(NULL);
    strncpy(conn_msg.payload, username, NAME_SIZE - 1);
    conn_msg.payload[NAME_SIZE - 1] = '\0';
    conn_msg.length = strlen(conn_msg.payload);
    
    if (send_msg(client_socket, &conn_msg) < 0) 
    {
        fprintf(stderr, "Failed to send connection message\n");
        close(client_socket);
        return 1;
    }
    
    /* Initialize poll array for multiplexed I/O */
    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO;      /* User input */
    fds[0].events = POLLIN;
    fds[1].fd = client_socket;     /* Server messages */
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
                continue;  /* Interrupted by signal */
            perror("poll");
            break;
        }
        
        /* Handle user input from stdin */
        if (fds[0].revents & POLLIN)
         {
            memset(input_buffer, 0, sizeof(input_buffer));
            
            if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL)
             {
                /* EOF or read error */
                break;
            }
            
            strip_newline(input_buffer);
            
            /* Check for exit command */
            if (strcmp(input_buffer, "exit") == 0) 
            {
                printf("\nDisconnecting...\n");
                running = 0;
                break;
            }
            
            /* Parse and send message */
            int ret = parse_and_send_message(client_socket, input_buffer, &next_msg_id);
            
            if (ret == 0) 
            {
                /* Message was sent */
                printf("%s> ", username);
                fflush(stdout);
            } 
            else if (ret == 1) 
            {
                /* Local command was handled (e.g., /help) */
                printf("%s> ", username);
                fflush(stdout);
            }
            /* If ret == -1, error was printed by parse_and_send_message */
        }
        
        /* Handle server messages */
        if (fds[1].revents & POLLIN) 
        {
            Message recv_msg_data = {0};
            int ret = recv_msg(client_socket, &recv_msg_data);
            
            if (ret == -2)
             {
                /* Server closed connection */
                printf("\n[SERVER] Connection closed by server.\n");
                running = 0;
            } 
            else if (ret < 0) 
            {
                /* Protocol error */
                printf("\n[ERROR] Protocol error or corrupted message.\n");
                running = 0;
            } 
            else 
            {
                /* Valid message received */
                handle_server_message(&recv_msg_data);
                printf("%s> ", username);
                fflush(stdout);
            }
        }
    }
    
    /* Send disconnect message to server */
    Message disc_msg = {0};
    disc_msg.version = PROTOCOL_VERSION;
    disc_msg.type = MSG_DISCONNECT;
    disc_msg.message_id = next_msg_id++;
    disc_msg.timestamp = time(NULL);
    disc_msg.length = 0;
    
    send_msg(client_socket, &disc_msg);
    
    /* Cleanup */
    close(client_socket);
    printf("Goodbye!\n");
    
    return 0;
}


