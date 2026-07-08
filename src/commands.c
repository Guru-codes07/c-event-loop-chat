#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include<unistd.h>
#include "commands.h"
#include "connections.h"
#include "protocol.h"
#include "database.h" 

// to handle  message chat 
void handle_msg_chat(client_t *client, Message *msg)
{
    if (client == NULL || msg == NULL)
        return;
 
    msg->payload[msg->length] = '\0';
 
    /* Create broadcast message */
    Message broadcast_msg = {0};
    broadcast_msg.version = PROTOCOL_VERSION;
    broadcast_msg.type = MSG_CHAT;
    broadcast_msg.message_id = msg->message_id;
    broadcast_msg.timestamp = time(NULL);
 
    /* Reserve space: "username: " + payload, safe truncation */
    int name_len = strlen(client->name);
    int available = MAX_PAYLOAD_SIZE - name_len - 3;  /* "name: \0" */
    
    if (available < 10) 
    {
        /* Username too long, just send the message */
        snprintf(broadcast_msg.payload, MAX_PAYLOAD_SIZE, "%s", msg->payload);
    } 
    else 
    {
        /* Safely format with truncation if needed */
        snprintf(broadcast_msg.payload, MAX_PAYLOAD_SIZE, "%s: %.*s",client->name, available, msg->payload);
    }
    broadcast_msg.length = strlen(broadcast_msg.payload);
 
    printf("[CHAT] %s\n", broadcast_msg.payload);
    
    /* Store message in database */
    if (db_store_message(client->name, msg->payload, broadcast_msg.timestamp, broadcast_msg.message_id) < 0) 
    {
        printf("[WARN] Failed to store message to database: %s\n", db_get_error());
        /* Note: We still broadcast the message even if storage fails */
    }
    
    broadcast_message(&broadcast_msg, client);
}

// private message handler:
void handle_msg_private(client_t *sender, Message *msg)
{
    if (sender == NULL || msg == NULL)
        return;
 
    msg->payload[msg->length] = '\0';
 
    /* Format: recipient_name:message_text */
    char *colon = strchr(msg->payload, ':');
    if (colon == NULL)
    {
        Message error_msg = {0};
        error_msg.version = PROTOCOL_VERSION;
        error_msg.type = MSG_ERROR;
        error_msg.message_id = msg->message_id;
        error_msg.timestamp = time(NULL);
        snprintf(error_msg.payload, MAX_PAYLOAD_SIZE, "Invalid private message format. Use: recipient:message");
        error_msg.length = strlen(error_msg.payload);
        send_msg(sender->socket_fd, sender->ssl, &error_msg);
        return;
    }
 
    *colon = '\0';
    char *recipient_name = msg->payload;
    char *message_text = colon + 1;
 
    /* Find recipient */
    client_t *recipient = find_client_by_name(recipient_name);
 
    if (recipient == NULL) 
    {
        Message error_msg = {0};
        error_msg.version = PROTOCOL_VERSION;
        error_msg.type = MSG_ERROR;
        error_msg.message_id = msg->message_id;
        error_msg.timestamp = time(NULL);
        /* Safely truncate recipient name if too long */
        int available = MAX_PAYLOAD_SIZE - 20;  /* "User '...' not found\0" */
        snprintf(error_msg.payload, MAX_PAYLOAD_SIZE, "User '%.*s' not found", available, recipient_name);
        error_msg.length = strlen(error_msg.payload);
        send_msg(sender->socket_fd, sender->ssl, &error_msg);
        return;
    }
 
    /* Send private message to recipient */
    Message private_msg = {0};
    private_msg.version = PROTOCOL_VERSION;
    private_msg.type = MSG_PRIVATE_MSG;
    private_msg.message_id = msg->message_id;
    private_msg.timestamp = time(NULL);
 
    /* Reserve space: "[PRIVATE from username]: " + message text */
    int sender_len = strlen(sender->name);
    int prefix_len = 18 + sender_len;  /* "[PRIVATE from ]: " = 18 chars */
    int available = MAX_PAYLOAD_SIZE - prefix_len - 1;
    
    if (available < 10) 
    {
        /* Sender name too long, just send the message text */
        snprintf(private_msg.payload, MAX_PAYLOAD_SIZE, "%s", message_text);
    } else 
    {
        /* Safely format with truncation */
        snprintf(private_msg.payload, MAX_PAYLOAD_SIZE, "[PRIVATE from %s]: %.*s", 
                 sender->name, available, message_text);
    }
    private_msg.length = strlen(private_msg.payload);
 
    if (send_msg(recipient->socket_fd, recipient->ssl, &private_msg) < 0) 
    {
        printf("Failed to send private message to %s\n", recipient->name);
    } 
    else 
    {
        printf("[PRIVATE] %s -> %s: %s\n", sender->name, recipient->name, message_text);
        if (db_store_private_message(sender->name, recipient->name, message_text, time(NULL), msg->message_id) < 0) 
        {
            printf("[WARN] Failed to store private message to database: %s\n", db_get_error());
            /* We still deliver the message even if storage fails */
        }
    }
}

// handles the WHO command:
void handle_msg_who(client_t *client, Message *msg)
{
    if (client == NULL || msg == NULL)
        return;
 
    Message response = {0};
    response.version = PROTOCOL_VERSION;
    response.type = MSG_SERVER;
    response.message_id = msg->message_id;
    response.timestamp = time(NULL);
 
    int offset = snprintf(response.payload, MAX_PAYLOAD_SIZE, "Online users: ");
 
    /* Build user list */
    for (int i = 0; i < MAX_CLIENTS; i++) 
    {
        if (clients[i] != NULL) 
        {
            int remaining = MAX_PAYLOAD_SIZE - offset - 1;
            if (remaining <= 0)
                break;
 
            offset += snprintf(response.payload + offset, remaining,"%s ", clients[i]->name);
        }
    }
 
    response.length = strlen(response.payload);
 
    if (send_msg(client->socket_fd, client->ssl, &response) < 0) 
    {
        printf("Failed to send user list to %s\n", client->name);
    } 
    else 
    {
        printf("[WHO] %s requested user list\n", client->name);
    }
}

// to handle message history :
void handle_msg_history(client_t *client, Message *msg)
{
    if (client == NULL || msg == NULL)
        return;

    /* Query recent 20 messages from database */
    int count = 0;
    StoredMessage *messages = db_get_recent_messages(20, &count);

    if (count == 0) 
    {
        /* No messages in history */
        Message response = {0};
        response.version = PROTOCOL_VERSION;
        response.type = MSG_SERVER;
        response.message_id = msg->message_id;
        response.timestamp = time(NULL);
        snprintf(response.payload, MAX_PAYLOAD_SIZE, "No chat history available");
        response.length = strlen(response.payload);
        send_msg(client->socket_fd, client->ssl, &response);
        return;
    }

    /* Build response with message history */
    Message response = {0};
    response.version = PROTOCOL_VERSION;
    response.type = MSG_SERVER;
    response.message_id = msg->message_id;
    response.timestamp = time(NULL);

    int offset = snprintf(response.payload, MAX_PAYLOAD_SIZE, "=== Chat History (Last %d messages) ===\n", count);

    /* Messages are returned in reverse order (newest first)
     * So we iterate backwards to display chronologically (oldest first) */
    for (int i = count - 1; i >= 0 && offset < MAX_PAYLOAD_SIZE - 150; i--) 
    {
        struct tm *tm_info = localtime(&messages[i].timestamp);
        char time_str[32];
        strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);

        int remaining = MAX_PAYLOAD_SIZE - offset - 1;
        offset += snprintf(response.payload + offset, remaining, "[%s] %s: %s\n", time_str, messages[i].sender_name, messages[i].message_text);
    }

    response.length = strlen(response.payload);
    
    if (send_msg(client->socket_fd, client->ssl, &response) < 0) 
    {
        printf("Failed to send history to %s\n", client->name);
    } 
    else 
    {
        printf("[HISTORY] %s requested chat history\n", client->name);
    }
    
    /* Clean up allocated memory */
    free(messages);
}


// to handle message disconnect:
void handle_msg_disconnect(client_t *client, Message *msg __attribute__((unused)))
{
    if (client == NULL)
        return;
 
    printf("[DISCONNECT] %s\n", client->name);
 
    /* Announce departure */
    Message leave_msg = {0};
    leave_msg.version = PROTOCOL_VERSION;
    leave_msg.type = MSG_SERVER;
    leave_msg.timestamp = time(NULL);
 
    snprintf(leave_msg.payload, MAX_PAYLOAD_SIZE,"📣 %s has left the chat.", client->name);
    leave_msg.length = strlen(leave_msg.payload);
 
    broadcast_message(&leave_msg, client);
 
    /* Clean up */
    close(client->socket_fd);
    remove_client(client);
    free(client);
}


