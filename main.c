#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include "sub.h"
#include "keyValStore.h"

#define BUFFERSIZE 1024
#define PORT 5678

int main() {
    printf("Starting server...\n");

    struct sockaddr_in client;
    socklen_t client_len;
    char in[BUFFERSIZE];
    int bytes_read;
    int connection_fd;

    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_fd < 0) {
        fprintf(stderr, "Socket could not be created.\n");
        exit(-1);
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    int result = bind(socket_fd, (struct sockaddr *) &server, sizeof(server));
    if (result < 0) {
        fprintf(stderr, "Socket could not be bound.\n");
        exit(-1);
    }

    int ret = listen(socket_fd, 5);
    if (ret < 0) {
        fprintf(stderr, "Socket was unable to listen.\n");
        exit(-1);
    }

    printf("Server started. Listening to incoming connections...\n");

    while (1) {
        connection_fd = accept(socket_fd, (struct sockaddr *) &client, &client_len);

        printf("New client connected.\n");

        write(connection_fd, "Hello!\n", 7);

        bytes_read = read(connection_fd, in, BUFFERSIZE);

        while (bytes_read > 0) {
            if (strcasecmp(in, "quit") == 0) continue;

            printf("sending back the %d bytes I received...\n", bytes_read);

            write(connection_fd, in, bytes_read);
            bytes_read = read(connection_fd, in, BUFFERSIZE);

        }

        printf("Client disconnected\n");

        close(connection_fd);
    }

    close(connection_fd);

    return 0;
}


