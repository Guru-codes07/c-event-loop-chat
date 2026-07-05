#ifndef COMMANDS_H
#define COMMANDS_H
#include"connections.h"
#include"protocol.h"

/* Message handlers for binary protocol */
void handle_msg_chat(client_t *client, Message *msg);
void handle_msg_private(client_t *client, Message *msg);
void handle_msg_who(client_t *client, Message *msg);
void handle_msg_disconnect(client_t *client, Message *msg);


#endif