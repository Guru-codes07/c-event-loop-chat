#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "network.h"
#include "common.h"

/**
 * Create and bind server socket
 * Listen for incoming connections
 */
int create_server_socket(void)
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
    {
        perror("socket failure");
        return -1;
    }

    /* Allow socket reuse (avoid "Address already in use" error) */
    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt failure");
        close(listen_fd);
        return -1;
    }

    /* Bind to port */
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind failure");
        close(listen_fd);
        return -1;
    }

    /* Listen for connections */
    if (listen(listen_fd, MAX_CLIENTS) < 0)
    {
        perror("listen failure");
        close(listen_fd);
        return -1;
    }

    printf("Server listening on port %d (fd = %d)\n", PORT, listen_fd);
    return listen_fd;
}

/**
 * Remove trailing newline or carriage return from string
 */
void strip_newline(char *str)
{
    if (str == NULL)
        return;
    str[strcspn(str, "\r\n")] = '\0';
}

