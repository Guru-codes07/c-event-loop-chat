#ifndef COMMANDS_H
#define COMMANDS_H
#include"connections.h"
#include"common.h"

/* Function for declaring /who and /w commands in the program
which will be used by the clients */
void handle_command(client_t *client,const char *command);

#endif