#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<poll.h>
#include<stdint.h>
#include<time.h>
#include<arpa/inet.h>

#define PORT 8080
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024
#define MAX_PAYLOAD_SIZE 1024
#define PROTOCOL_VERSION 1

/* Message types from protocol.h */
typedef enum {
    MSG_CONNECTION = 1,
    MSG_CHAT = 2,
    MSG_PRIVATE_MSG = 3,
    MSG_WHO_COMMAND = 4,
    MSG_DISCONNECT = 5,
    MSG_SERVER = 6,
    MSG_ERROR = 7
} MessageType;

/* Message structure from protocol.h */
typedef struct {
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

/* CRC32 calculation */
uint32_t calculate_crc32(const char *data, uint16_t length)
{
    uint32_t crc = 0xFFFFFFFF;
    for (uint16_t i = 0; i < length; i++) {
        uint8_t byte = data[i];
        crc ^= byte;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc ^ 0xFFFFFFFF;
}

/* Send a complete message */
int send_msg(int sockfd, Message *msg)
{
    uint8_t header[24];
    
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
    
    if (send(sockfd, header, 24, 0) < 0)
        return -1;
    if (send(sockfd, msg->payload, msg->length, 0) < 0)
        return -1;
    
    return 0;
}

/* Receive a complete message (blocking) */
int recv_msg(int sockfd, Message *msg)
{
    uint8_t header[24];
    
    if (recv(sockfd, header, 24, MSG_WAITALL) < 24)
        return -1;
    
    msg->version = header[0];
    msg->type = header[1];
    msg->flags = header[2];
    
    memcpy(&msg->length, header + 4, 2);
    msg->length = ntohs(msg->length);
    
    if (msg->length > MAX_PAYLOAD_SIZE)
        return -1;
    
    uint32_t msg_id;
    memcpy(&msg_id, header + 8, 4);
    msg->message_id = ntohl(msg_id);
    
    uint32_t ts;
    memcpy(&ts, header + 12, 4);
    msg->timestamp = ntohl(ts);
    
    uint32_t crc;
    memcpy(&crc, header + 16, 4);
    msg->crc32 = ntohl(crc);
    
    if (recv(sockfd, msg->payload, msg->length, MSG_WAITALL) < msg->length)
        return -1;
    
    msg->payload[msg->length] = '\0';
    
    uint32_t calculated = calculate_crc32(msg->payload, msg->length);
    if (calculated != msg->crc32)
        return -1;
    
    return 0;
}

/* Parse and send commands */
void parse_and_send_message(int sockfd, char *input, uint32_t *msg_id)
{
    Message msg = {0};
    msg.version = PROTOCOL_VERSION;
    msg.message_id = (*msg_id)++;
    msg.timestamp = time(NULL);
    
    /* Check if it's a command */
    if (input[0] == '/') {
        if (strncmp(input, "/who", 4) == 0) {
            /* /who command */
            msg.type = MSG_WHO_COMMAND;
            strncpy(msg.payload, "", MAX_PAYLOAD_SIZE - 1);
            msg.length = 0;
        } 
        else if (strncmp(input, "/w ", 3) == 0) {
            /* /w username:message private message */
            char *rest = input + 3;  /* Skip "/w " */
            strncpy(msg.payload, rest, MAX_PAYLOAD_SIZE - 1);
            msg.type = MSG_PRIVATE_MSG;
            msg.length = strlen(msg.payload);
        }
        else if (strcmp(input, "/help") == 0) {
            printf("\nAvailable commands:\n");
            printf("  /who                    - List online users\n");
            printf("  /w <user>:<message>     - Send private message\n");
            printf("  /help                   - Show this help\n");
            printf("  exit                    - Disconnect\n\n");
            return;
        }
        else {
            printf("Unknown command. Type /help for available commands.\n");
            return;
        }
    } 
    else {
        /* Regular chat message */
        msg.type = MSG_CHAT;
        strncpy(msg.payload, input, MAX_PAYLOAD_SIZE - 1);
        msg.length = strlen(msg.payload);
    }
    
    if (send_msg(sockfd, &msg) < 0) {
        perror("send error");
    }
}

int main()
{
    // creating a socket
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("socket failed");
        return 1;
    }
    
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);
    
    // connecting the socket
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connection failed");
        return 1;
    }
    printf("Connected to the server on %s:%d\n", SERVER_IP, PORT);
    
    // ask for username
    char username[50];
    printf("Enter your username: ");
    fflush(stdout);
    
    if (fgets(username, sizeof(username), stdin) == NULL) {
        close(client_socket);
        return 1;
    }
    username[strcspn(username, "\n")] = '\0';
    
    // Send username using binary protocol
    Message conn_msg = {0};
    conn_msg.version = PROTOCOL_VERSION;
    conn_msg.type = MSG_CONNECTION;
    conn_msg.message_id = 1;
    conn_msg.timestamp = time(NULL);
    strncpy(conn_msg.payload, username, MAX_PAYLOAD_SIZE - 1);
    conn_msg.length = strlen(conn_msg.payload);
    
    if (send_msg(client_socket, &conn_msg) < 0) {
        perror("send failed");
        close(client_socket);
        return 1;
    }
    
    // initialize poll array 
    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = client_socket;
    fds[1].events = POLLIN;
    
    char buffer[BUFFER_SIZE];
    uint32_t next_msg_id = 2;
    
    printf("Connected! Type 'exit' to quit, '/help' for commands.\n");
    printf("you: ");
    fflush(stdout);
    
    // main poll() loop 
    while (1) {
        int activity = poll(fds, 2, -1);
        if (activity < 0) {
            perror("poll error");
            break;
        }
        
        // Handle stdin (user input)
        if (fds[0].revents & POLLIN) {
            memset(buffer, 0, sizeof(buffer));
            if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                break;
            }
            buffer[strcspn(buffer, "\n")] = '\0';
            
            // the exit command
            if (strcmp(buffer, "exit") == 0) {
                printf("Goodbye\n");
                break;
            }
            
            // Parse and send message with proper command handling
            parse_and_send_message(client_socket, buffer, &next_msg_id);
            
            printf("you: ");
            fflush(stdout);
        }
        
        // Handle server messages
        if (fds[1].revents & POLLIN) {
            Message recv_msg_data;
            if (recv_msg(client_socket, &recv_msg_data) < 0) {
                printf("\nServer disconnected or protocol error.\n");
                break;
            }
            
            printf("\r%s\n", recv_msg_data.payload);
            printf("you: ");
            fflush(stdout);
        }
    }
    
    // Send disconnect message
    Message disc_msg = {0};
    disc_msg.version = PROTOCOL_VERSION;
    disc_msg.type = MSG_DISCONNECT;
    disc_msg.message_id = next_msg_id++;
    disc_msg.timestamp = time(NULL);
    disc_msg.length = 0;
    send_msg(client_socket, &disc_msg);
    
    close(client_socket);
    return 0;
}


