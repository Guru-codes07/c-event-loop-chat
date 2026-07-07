#ifndef CONNECTIONS_H
#define CONNECTIONS_H
 
#include <poll.h>
#include "protocol.h"
 
#define MAX_CLIENTS 100
#define NAME_SIZE 32
 
/* Client structure with binary protocol support */
typedef struct {
    int socket_fd;
    char name[NAME_SIZE];
    MessageBuffer recv_buf;      /* For partial message buffering */
    uint32_t next_message_id;    /* Outgoing message counter */
    time_t last_activity;
    int poll_index;
} client_t;
 
/* Global client table — defined in connections.c */
extern client_t *clients[MAX_CLIENTS];
extern struct pollfd fds[MAX_CLIENTS + 1];
extern int nfds;
extern client_t *fd_to_client[MAX_CLIENTS + 1];

/* Client management functions */
int add_client(client_t *client);
void remove_client(client_t *client);
client_t *find_client_by_name(const char *name);
 
/* Message broadcasting */
void broadcast_message(Message *msg, client_t *exclude);
 
#endif
