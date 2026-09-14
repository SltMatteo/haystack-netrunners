#include "error.h"
#include "socket_layer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#define MAX_CLIENT_SIZE 2048

int tcp_client_init(uint16_t port) {
    struct sockaddr_in address;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error on socket creation");
        return ERR_IO;
    }

    memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (connect(sockfd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        perror("Error on connect");
        close(sockfd);
        return ERR_IO;
    }

    return sockfd;
}


/**
 * Calculates the size of a file.
 *
 * @param filename The name of the file.
 * @return The size of the file in bytes, or -1 if an error occurred.
 */

int main(int argc, char *argv[]) {
    if(argc != 3) {
        printf("Invalid number of arguments");
        return ERR_INVALID_ARGUMENT;
    }
    M_REQUIRE_NON_NULL(argv);

    char buffer[MAX_CLIENT_SIZE];

    uint16_t port = atoi(argv[1]);
    printf("Talking to %d\n", port);

    // Get the file and copy its content into the buffer
    const char *filename = argv[2];
    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Unable to open input file");
        return ERR_IO;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ERR_IO;
    }
    long file_size = ftell(file);
    if (file_size < 0 || (size_t)file_size >= sizeof(buffer)) {
        fclose(file);
        fprintf(stderr, "Input must be smaller than %d bytes\n", MAX_CLIENT_SIZE);
        return ERR_INVALID_ARGUMENT;
    }
    int written = snprintf(buffer, sizeof(buffer), "%ld", file_size);
    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        fclose(file);
        return ERR_IO;
    }

    int sockfd = tcp_client_init(port); // Client initialization

    if (sockfd < 0) {
        fclose(file);
        return sockfd;
    }

    // Send length
    size_t message_len = strlen(buffer);
    ssize_t error = tcp_send(sockfd, buffer, message_len);
    if (error < 0 || (size_t)error != message_len) {
        close(sockfd);
        fclose(file);
        return error;
    }

    printf("Sending size %s:\n", buffer);

    // Receive size acknowledgement ("Small file")

    error = tcp_read(sockfd, buffer, sizeof(buffer) - 1);
    if (error < 0) {
        close(sockfd);
        fclose(file);
        return error;
    }
    buffer[error] = '\0';

    // Verify that the server acknowledged the file size
    printf("buffer: \"%s\"\n", buffer);
    if (strcmp(buffer, "Small file") != 0) {
        close(sockfd);
        fclose(file);
        fprintf(stderr, "Server did not acknowledge file size\n");
        return ERR_RUNTIME;
    }
    printf("Server responded: \"%s\"\n", buffer);

    // Write the file to the buffer
    if (fseek(file, 0, SEEK_SET) != 0 ||
        fread(buffer, 1, (size_t)file_size, file) != (size_t)file_size) {
        close(sockfd);
        fclose(file);
        return ERR_IO;
    }
    fclose(file);

    // Send the file
    error = tcp_send(sockfd, buffer, (size_t)file_size);
    if (error < 0 || error != file_size) {
        close(sockfd);
        return error;
    }

    printf("Sending %s:\n", filename);

    // Receive file acknowledgement
    error = tcp_read(sockfd, buffer, sizeof(buffer) - 1);
    if (error < 0) {
        close(sockfd);
        return error;
    }
    buffer[error] = '\0';

    // Verify that the server received the file content
    if (strcmp(buffer, "Accepted") != 0) {
        close(sockfd);
        fprintf(stderr, "Server did not acknowledge receiving the complete file\n");
        return ERR_RUNTIME;
    }

    printf("Done\n");
    close(sockfd);

    return ERR_NONE;
}
