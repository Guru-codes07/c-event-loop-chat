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
#include "protocol.h"
#include "TLS.h"
#include "network.h"

#define PORT 8080
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024
#define NAME_SIZE 32

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
int parse_and_send_message(int sockfd, SSL *ssl, char *input, uint32_t *msg_id)
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

    if (send_msg(sockfd, ssl, &msg) < 0)
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

    switch (msg->type)
    {
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
    /* Initialize client-side TLS context (TLS.c) */
    if (tls_init_client() < 0)
    {
        fprintf(stderr, "[CLIENT] Failed to initialize TLS\n");
        return 1;
    }

    /* Create socket */
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0)
    {
        perror("[CLIENT] socket");
        tls_cleanup();
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
        tls_cleanup();
        return 1;
    }

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("[CLIENT] connect");
        close(client_socket);
        tls_cleanup();
        return 1;
    }

    printf("✓ Connected to server at %s:%d\n", SERVER_IP, PORT);

    /* Create SSL connection */
    SSL *ssl = SSL_new(tls_client_ctx);
    if (ssl == NULL)
    {
        fprintf(stderr, "[CLIENT] Failed to create SSL\n");
        close(client_socket);
        tls_cleanup();
        return 1;
    }

    if (!SSL_set_fd(ssl, client_socket))
    {
        fprintf(stderr, "[CLIENT] Failed to set SSL file descriptor\n");
        SSL_free(ssl);
        close(client_socket);
        tls_cleanup();
        return 1;
    }

    /* Perform TLS handshake (blocking, via TLS.c) */
    printf("Performing TLS handshake...\n");
    if (tls_handshake(ssl, 0) != 1)  /* 0 = client */
    {
        fprintf(stderr, "[CLIENT] TLS handshake failed\n");
        SSL_free(ssl);
        close(client_socket);
        tls_cleanup();
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
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(client_socket);
        tls_cleanup();
        return 1;
    }

    strip_newline(username);

    if (strlen(username) == 0)
    {
        fprintf(stderr, "[CLIENT] Username cannot be empty\n");
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(client_socket);
        tls_cleanup();
        return 1;
    }

    if (strlen(username) >= NAME_SIZE)
    {
        fprintf(stderr, "[CLIENT] Username too long\n");
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(client_socket);
        tls_cleanup();
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

    if (send_msg(client_socket, ssl, &conn_msg) < 0)
    {
        fprintf(stderr, "[CLIENT] Failed to send connection message\n");
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(client_socket);
        tls_cleanup();
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

            int ret = parse_and_send_message(client_socket, ssl, input_buffer, &next_msg_id);

            if (ret == 0 || ret == 1)
            {
                printf("%s> ", username);
                fflush(stdout);
            }
        }

        /* Handle server messages */
        if (fds[1].revents & POLLIN)
        {
            Message recv_msg_data = {0};
            int ret = recv_msg(client_socket, ssl, &recv_msg_data);

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

    send_msg(client_socket, ssl, &disc_msg);

    /* Cleanup */
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(client_socket);
    tls_cleanup();

    printf("Goodbye!\n");

    return 0;
}


