/*
    Multi-client Group Chat Client in C
    - Connects to the server on a specific port.
    - Sends and receives messages to/from the server.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BUFFER_SIZE 2048
#define SERVER_IP "127.0.0.1"
#define PORT 8080

// Handle incoming messages from server
void *receive_messages(void *socket) {
    int sockfd = *(int *)socket;
    char buffer[BUFFER_SIZE];
    int bytes_received;

    while ((bytes_received = recv(sockfd, buffer, sizeof(buffer), 0)) > 0) {
        buffer[bytes_received] = '\0';
        printf("%s", buffer);
    }

    printf("Disconnected from server.\n");
    close(sockfd);
    exit(0);
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    printf("Connected to server at %s:%d\n", SERVER_IP, PORT);

    // Create thread for receiving messages
    pthread_t receive_thread;
    if (pthread_create(&receive_thread, NULL, receive_messages, (void *)&sockfd) != 0) {
        perror("Failed to create receive thread");
        exit(EXIT_FAILURE);
    }

    // Send messages to server
    while (1) {
        fgets(buffer, BUFFER_SIZE, stdin);
        send(sockfd, buffer, strlen(buffer), 0);
    }

    close(sockfd);
    return 0;
}
