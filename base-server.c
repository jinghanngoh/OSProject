#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <openssl/sha.h>
#include "messages.h"

// PacketRequest structure based on the project specification
struct PacketRequest {
    uint8_t hash[32];  // 32 bytes for the SHA256 hash
    uint64_t start;    // Start of the range
    uint64_t end;      // End of the range
    uint8_t priority;  // Priority level (not used for now)
};

// Function to compute the SHA256 hash of a 64-bit number
void computeSHA256(uint64_t num, uint8_t* outputHash) {
    unsigned char data[8];
    memcpy(data, &num, sizeof(uint64_t));
    
    SHA256(data, sizeof(data), outputHash);
}

// Function to reverse the hash by brute-forcing within the specified range
uint64_t reverseHash(uint8_t* targetHash, uint64_t start, uint64_t end) {
    uint8_t hash[SHA256_DIGEST_LENGTH];
    uint64_t num;

    for (num = start; num < end; num++) {
        computeSHA256(num, hash);

        if (memcmp(hash, targetHash, SHA256_DIGEST_LENGTH) == 0) {
            return num;
        }
    }
    return UINT64_MAX;
}

int main(int argc, char* argv[]) {
    int port = 5003;

    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Port number must be between 1 and 65535\n");
            exit(EXIT_FAILURE);
        }
    }

    struct sockaddr_in server_address, client_address;

    // Create a TCP socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        fprintf(stderr, "Unable to create server socket\n");
        exit(EXIT_FAILURE);
    }

    // Enable address reuse
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
        fprintf(stderr, "setsockopt SO_REUSEADDR failed");
        exit(EXIT_FAILURE);
    }

    // Enable port reuse
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int)) < 0) {
        fprintf(stderr, "setsockopt SO_REUSEPORT failed");
        exit(EXIT_FAILURE);
    }

    // Set up the server address structure
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);

    // Bind the socket to the address and port number
    if (bind(server_socket, (struct sockaddr*) &server_address, sizeof(server_address)) < 0) {
        fprintf(stderr, "Unable to bind socket\n");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, 100) < 0) {
        fprintf(stderr, "Listen failed\n");
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d...\n", port);

    // Main loop to handle client requests
    int client_socket;
    socklen_t client_address_size = sizeof(client_address);
    while (true) {
        client_socket = accept(server_socket, (struct sockaddr*) &client_address, &client_address_size);
        if (client_socket < 0) {
            fprintf(stderr, "Server accept failed\n");
            exit(EXIT_FAILURE);
        }
        fprintf(stdout, "Accepted a connection from %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

        struct PacketRequest client_request;
        ssize_t bytes_read = recv(client_socket, &client_request, sizeof(client_request), 0);

        if (bytes_read < 0) {
            fprintf(stderr, "Error receiving request\n");
            close(client_socket);
            continue;
        }

        // Convert start and end to host byte order (big-endian to little-endian)
        uint64_t start = be64toh(client_request.start);
        uint64_t end = be64toh(client_request.end);

        // Reverse the hash to find the matching number in the range [start, end)
        uint64_t answer = reverseHash(client_request.hash, start, end);

        // Convert the result to network byte order before sending
        answer = htobe64(answer);

        // Send the response back to the client
        ssize_t bytes_sent = send(client_socket, &answer, sizeof(answer), 0);
        if (bytes_sent < 0) {
            fprintf(stderr, "Error sending response\n");
        }
        close(client_socket);
    }

    // Close the server socket
    close(server_socket);
    return 0;
}
