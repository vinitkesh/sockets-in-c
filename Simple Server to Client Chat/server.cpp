#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

#define SERVER_PORT 5555
#define BUFFER_SIZE 1024

int main() {
    int server_fd, new_socket; // server socket file descriptor and new socket file descriptor
    struct sockaddr_in address; // generic socket address struct from sys/socket
    socklen_t addr_len = sizeof(address); // len
    char buffer[BUFFER_SIZE]; // declaring a buffer for all comms

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "Socket creation failed." << std::endl;
        return 1;
    }

    // Bind address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SERVER_PORT);
    
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == -1) {
        std::cerr << "Bind failed." << std::endl;
        close(server_fd);
        return 1;
    }

    // Listen for connections
    if (listen(server_fd, 5) == -1) {
        std::cerr << "Listen failed." << std::endl;
        close(server_fd);
        return 1;
    }
    
    std::cout << "Server listening on port " << SERVER_PORT << "..." << std::endl;

    // Accept connections
    while (true) {
        new_socket = accept(server_fd, (struct sockaddr*)&address, &addr_len);
        if (new_socket == -1) {
            std::cerr << "Accept failed." << std::endl;
            continue;
        }
        std::cout << "New connection accepted." << std::endl;

        // Receive messages
        while (true) {
            memset(buffer, 0, BUFFER_SIZE);
            int bytes_received = recv(new_socket, buffer, BUFFER_SIZE, 0);
            if (bytes_received <= 0) {
                std::cerr << "Client disconnected or error occurred." << std::endl;
                break;
            }
            std::cout << "Received: " << buffer << std::endl;
        }

        close(new_socket);
    }

    close(server_fd);
    return 0;
}