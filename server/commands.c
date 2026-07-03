#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include"commands.h"
#include"connections.h"
#include"common.h"
#include "network.h"
 

// Function for declaring /who and /w commands in the program
// which will be used by the clients
void handle_command(client_t *client,const char *command)
{
    char trimmed_command[BUFFER_SIZE];
    strncpy(trimmed_command,command,sizeof(trimmed_command)-1);
    trimmed_command[sizeof(trimmed_command)-1] = '\0';
    strip_newline(trimmed_command);

    // Handle /who command
    if(strcmp(trimmed_command,"/who")==0)
    {
        send_online_list(client);
        return;
    }

    // Handle /w command
    if(strncmp(trimmed_command,"/w ",3)==0)
    {
        char *target = trimmed_command + 3;
        char *space =   strchr(target,' ');
        if(space == NULL)
        {
            char *usage = "To use the /w command, type: /w <username> <message>\n";
            send((*client).socket_fd,usage,strlen(usage),0);
            return;
        }
        *space = '\0';
        char *text = space + 1;
        if(!send_private_message(client,target,text))
        {
            char error_msg[NAME_SIZE + 45];
            snprintf(error_msg,sizeof(error_msg),"user %s not found.\n",target);
            send((*client).socket_fd,error_msg,strlen(error_msg),0);
        }
        return;
    }
    char format[BUFFER_SIZE + NAME_SIZE];
    snprintf(format,sizeof(format),"%s: %s\n",(*client).name,command);
    printf("%s",format);
    broadcast_message(format,client);
}