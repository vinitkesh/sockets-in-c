/*
    Multi-client Group Chat Server in C
    - Listens for incoming connections on a specific port.
    - Broadcasts messages to all connected clients.
    - Uses pthreads to handle multiple clients concurrently.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 2048
#define PORT 8080

// Array to store client sockets
int client_sockets[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function to broadcast message to all clients
void broadcast_message(char *message, int sender_socket) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] != 0 && client_sockets[i] != sender_socket) {
            send(client_sockets[i], message, strlen(message), 0);
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

// Handle client communication
void *handle_client(void *client_socket) {
    int sockfd = *(int *)client_socket;
    char buffer[BUFFER_SIZE];
    int bytes_received;

    while ((bytes_received = recv(sockfd, buffer, sizeof(buffer), 0)) > 0) {
        buffer[bytes_received] = '\0';
        printf("Client %d: %s", sockfd, buffer);
        broadcast_message(buffer, sockfd);
    }

    // Remove client on disconnection
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] == sockfd) {
            client_sockets[i] = 0;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    close(sockfd);
    printf("Client %d disconnected\n", sockfd);
    pthread_exit(NULL);
}

// Main function to set up the server
int main() {
    int server_fd, new_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // Initialize client sockets to 0
    memset(client_sockets, 0, sizeof(client_sockets));

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind the socket
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    // Accept and handle incoming client connections
    while (1) {
        new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (new_socket < 0) {
            perror("Accept failed");
            continue;
        }

        // Add client to the list
        pthread_mutex_lock(&clients_mutex);
        int added = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] == 0) {
                client_sockets[i] = new_socket;
                added = 1;
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);

        if (!added) {
            printf("Max clients reached. Connection refused: %d\n", new_socket);
            close(new_socket);
            continue;
        }

        printf("New client connected: %d\n", new_socket);

        // Create thread for client
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void *)&new_socket) != 0) {
            perror("Failed to create thread");
        }
        pthread_detach(thread_id);
    }

    close(server_fd);
    return 0;
}
