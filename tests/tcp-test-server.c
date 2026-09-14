#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "socket_layer.h"
#include "error.h"

#define MAX_SERVER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("only need port number");
        return ERR_INVALID_ARGUMENT;
    }
    M_REQUIRE_NON_NULL(argv);

    char buffer[MAX_SERVER_SIZE + 1] = {0};

    uint16_t port = atoi(argv[1]);
    printf("Server started on port %d\n", port);
    int sockfd = tcp_server_init(port); // Server initialization

    if (sockfd < 0) {
        return sockfd;
    }

    // Infinite loop to keep the server running
    while (1) {
        printf("Waiting for a size...\n");
        int clientfd = tcp_accept(sockfd); // Accepting a client
        if (clientfd < 0) {
            close(sockfd);
            return clientfd;
        }

        // Receiving the file length
        ssize_t error = tcp_read(clientfd, buffer, sizeof(buffer) - 1);
        if (error < 0) {
            close(clientfd);
            close(sockfd);
            return error;
        }
        buffer[error] = '\0';

        long len = strtol(buffer, NULL, 10); // Convert the string to a long

        if (len < 0 || len > MAX_SERVER_SIZE) { // If the file is too large
            printf("Received a size: %ld --> rejected\n", len);
            size_t response_len = strlen("file too large") + 1;
            error = tcp_send(clientfd, "file too large", response_len);
            if (error < 0 || (size_t)error != response_len) {
                close(clientfd);
                close(sockfd);
                return error;
            }
            close(clientfd);
            continue;
        }

        printf("Received a size: %ld --> accepted\n", len);

        // Acknowledge receiving the size
        size_t response_len = strlen("Small file") + 1;
        error = tcp_send(clientfd, "Small file", response_len);
        if (error < 0 || (size_t)error != response_len) {
            close(clientfd);
            close(sockfd);
            return error;
        }

        printf("About to receive file of %ld bytes\n", len);

        error = tcp_read(clientfd, buffer, (size_t)len);
        if (error < 0) {
            close(clientfd);
            close(sockfd);
            return error;
        }
        buffer[error] = '\0';

        printf("Received a file:\n%s\n", buffer);

        // Acknowledge receiving the file
        response_len = strlen("Accepted") + 1;
        error = tcp_send(clientfd, "Accepted", response_len);
        if (error < 0 || (size_t)error != response_len) {
            close(clientfd);
            close(sockfd);
            return error;
        }
        close(clientfd);
    }
    close(sockfd);
    return ERR_NONE;
}
