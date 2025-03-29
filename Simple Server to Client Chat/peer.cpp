#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

#define SERVER_IP "192.168.5.95"
// #define SERVER_IP "192.168.2.2"
#define SERVER_PORT 5555

using namespace std;

int main()
{
    int sockfd;
    struct sockaddr_in server_addr;

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        std::cerr << "Error creating socket." << std::endl;
        return 1;
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0)
    {
        std::cerr << "Invalid address or Address not supported." << std::endl;
        close(sockfd);
        return 1;
    }

    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        std::cerr << "Connection to server failed." << std::endl;
        close(sockfd);
        return 1;
    }

    std::cout << "Connected to server at " << SERVER_IP << ":" << SERVER_PORT << std::endl;

    // Keep connection alive
    string message;
    while (true)
    {
        cout<<"[ME] ";
        getline(std::cin,message);
        if(message=="exit"){
            break;
        }
        if (send(sockfd, message.c_str(), message.size(), 0) == -1)
        {
            std::cerr << "Server disconnected." << std::endl;
            break;
        }
    }

    // Close socket
    close(sockfd);
    return 0;
}
