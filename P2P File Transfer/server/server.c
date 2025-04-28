/*
    Arguments: None
*/
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <bits/pthreadtypes.h>
#include <pthread.h>

#define SERVER_PORT 444
#define FILE_PORT 555
#define BUFF_SIZE 10000
#define MAX_ACCEPT_BACKLOG 5
#define EOF_MARKER "[EOF]"
#define EOF_MARKER_LEN 5
#define MAX_FILES 100
#define MAX_FILENAME_LEN 256

/* Function to trim name of files */
void trim_whitespace(char *str) {
    char *end;
    while (*str == ' ') str++;
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\n')) end--;
    *(end + 1) = '\0';
}

/*Function to handle errors*/
void error(const char * msg){
    perror(msg);
    exit(1);
}

/* Function to parse filenames from "SEND" command */
int parse_filenames(const char *input, char filenames[MAX_FILES][MAX_FILENAME_LEN]) {
    if (strncmp(input, "SEND ", 5) != 0) {
        printf("[ERROR] Invalid input format\n");
        return -1; // Invalid command
    }

    // Get the part after "SEND "
    const char *file_list = input + 5;

    int count = 0;
    char *token;

    // Make a mutable copy of the file list
    char *file_list_copy = strdup(file_list);
    if (file_list_copy == NULL) {
        perror("Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    // Tokenize by comma to extract filenames
    token = strtok(file_list_copy, ",");
    while (token != NULL && count < MAX_FILES) {
        // Trim whitespace
        while (*token == ' ') token++;

        // Copy filename to the array
        for(int i=0;i<MAX_FILENAME_LEN-1;i++){
            filenames[count][i] = '\0';   
        }
        strncpy(filenames[count], token, MAX_FILENAME_LEN - 1);
        filenames[count][MAX_FILENAME_LEN - 1] = '\0'; // Ensure null-termination

        count++;
    

        token = strtok(NULL, ",");
    }

    free(file_list_copy);
    return count; // Return number of filenames parsed
}

struct file_transfer_format{
    int socket_fd;
    char filenames[MAX_FILES][MAX_FILENAME_LEN];
    int total_files;
};

void* file_transfer(void* data) {
    struct file_transfer_format * details = (struct file_transfer_format *)data;
    char buffer[BUFF_SIZE];

    int sockfd = details->socket_fd;

    for(int i=0;i < details->total_files; i++){

        char * filename = details->filenames[i];
        memset(buffer, 0, BUFF_SIZE); 
        snprintf(buffer, sizeof(buffer), "[FILE SERVER] Sending: %s", filename);
        printf("%s\n",buffer);
        
        send(sockfd, buffer, strlen(buffer), 0);

        memset(buffer, 0, BUFF_SIZE); 
        recv(sockfd, buffer, BUFF_SIZE, 0);

        if(strncmp("[FILE SERVER] Starting file transfer", buffer, 36)!=0){
            printf("[ERROR] Error sending file\n");
            exit(1);
        }

        trim_whitespace(filename);

        FILE *fp;
        if(access(filename, F_OK) != -1){
            fp = fopen(filename,"rb");
            memset(buffer, 0, BUFF_SIZE);
            snprintf(buffer, sizeof(buffer), "[FILE SERVER] File found: %s", filename);
            send(sockfd, buffer, strlen(buffer), 0);
            printf("%s\n", buffer);

            memset(buffer, 0, BUFF_SIZE);
            recv(sockfd, buffer, BUFF_SIZE, 0);

        }
        else{
            memset(buffer, 0, BUFF_SIZE);
            snprintf(buffer, sizeof(buffer), "[ERROR] File not found: %s", filename);
            send(sockfd, buffer, strlen(buffer), 0);
            printf("%s\n", buffer);
            
            memset(buffer, 0, BUFF_SIZE);
            recv(sockfd, buffer, BUFF_SIZE, 0);

            continue;
        }

        if(fp == NULL){
            error("[ERROR] Error in reading file");
        }
        printf("[FILE SERVER] Sending data to server\n");
        memset(buffer, 0, BUFF_SIZE); // clear the buffer
        size_t bytes_read;
        
        // Read the file and send in chunks
        while ((bytes_read = fread(buffer, 1, BUFF_SIZE, fp)) > 0) {
            
            size_t total_sent = 0;  
            while (total_sent < bytes_read) {
                ssize_t sent = send(sockfd, buffer + total_sent, bytes_read - total_sent, 0);
                if (sent < 0) {
                    fclose(fp);
                    error("[ERROR] Error sending file");
                }
                total_sent += sent;
            }
            memset(buffer, 0, BUFF_SIZE); // clear the buffer
        }    
        strcpy(buffer,EOF_MARKER);
        send(sockfd,buffer,EOF_MARKER_LEN,0);

        fclose(fp);

        memset(buffer, 0, BUFF_SIZE);
        recv(sockfd, buffer, BUFF_SIZE, 0);
        printf("[FILE SERVER] File sent successfully\n");
    }
    close(sockfd);
    free(data);
    pthread_exit(NULL);
}

int main() {

    printf("[SERVER] Starting server...\n");
    // Creating listening sock
    int listen_sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    // Setting sock opt reuse addr
    int enable = 1;
    setsockopt(listen_sock_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

    // Creating an object of struct socketaddr_in
    struct sockaddr_in server_addr;

    // Setting up server addr
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(listen_sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        error("[ERROR] Bind failed\n");
    }
    if (listen(listen_sock_fd, MAX_ACCEPT_BACKLOG) < 0) {
        error("[ERROR] Listen failed\n");
    }
    printf("[SERVER] Server listening on port %d\n", SERVER_PORT);

    // Creating an object of struct socketaddr_in
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // Accept client connection
    int conn_sock_fd = accept(listen_sock_fd, (struct sockaddr *)&client_addr, &client_addr_len);

    printf("[CLIENT] Client connected to server\n");
        

    while (1) {
        // Create buffer to store client message
        char buffer[BUFF_SIZE];
        memset(buffer, 0, BUFF_SIZE);

        // Read message from client to buffer
        ssize_t read_n = recv(conn_sock_fd, buffer, BUFF_SIZE, 0);

        // Print message from client
        printf("\n[CLIENT MSG] %s", buffer);

        // Client closed connection or error occurred
        if (strncmp("exit",buffer,4)==0) {
            printf("[CLIENT] Client disconnected.\n");
            shutdown(conn_sock_fd,SHUT_RDWR);

            //Accept new connection
            conn_sock_fd = accept(listen_sock_fd, (struct sockaddr *)&client_addr, &client_addr_len);
            printf("[CLIENT] Client connected to server\n");
            continue;
        }

        //Make last '\n' as '\0'
        buffer[strlen(buffer)-1] = '\0';

        //Get files from input
        char files[MAX_FILES][MAX_FILENAME_LEN];
        int total_files = parse_filenames(buffer, files);
        memset(buffer, 0, BUFF_SIZE);

        //If there are files to be sent
        if(total_files>0){

            printf("[SERVER] Sending requested files to client\n");

            //Asking client to start tcp connection at the file port
            strcpy(buffer, "[SERVER] Start file server");
            send(conn_sock_fd, buffer, strlen(buffer), 0);
            printf("[SERVER] Requesting client to start file server\n");

            //Waiting for started signal from client
            memset(buffer, 0, BUFF_SIZE);
            recv(conn_sock_fd, buffer, BUFF_SIZE, 0);

            //New connection established
            if(strncmp("[FILE SERVER] Started", buffer, 21) == 0){
                
                //Connect server to client's file port
                printf("%s\n",buffer);
                //create a socket
                int cl_s_sockfd = socket(AF_INET, SOCK_STREAM, 0);   
                if(cl_s_sockfd < 0){
                    error("[ERROR] Error opening socket");
                }

                //set up server's server address(client)
                struct sockaddr_in server_address_2;
                server_address_2.sin_family = AF_INET;
                server_address_2.sin_port = htons(FILE_PORT);
                inet_pton(AF_INET, "CLIENT_IP_ADDRESS", &server_address_2.sin_addr);

                //connect to the server
                if(connect(cl_s_sockfd, (struct sockaddr *) &server_address_2, sizeof(server_address_2)) < 0){
                    error("[ERROR] Connection failed");
                }

                printf("[FILE SERVER] Established connection to file server\n");

                //Sending number of files to be transferred
                memset(buffer, 0, BUFF_SIZE); 
                snprintf(buffer, sizeof(buffer), "[SERVER] Sending files: %d",total_files);
                printf("[SERVER] Sending %d files\n", total_files);
                send(conn_sock_fd, buffer, strlen(buffer), 0);

                //Create a thread to send files
                struct file_transfer_format *data = malloc(sizeof(struct file_transfer_format));
                data->socket_fd = cl_s_sockfd;
                data->total_files = total_files;
                memcpy(data->filenames, files, sizeof(files));

                pthread_t thread_id;
                printf("[FILE SERVER] Creating thread for file transfer\n");
                if (pthread_create(&thread_id, NULL, file_transfer, (void *)data) != 0) {
                    perror("[ERROR] Failed to create file transfer thread");
                    free(data);
                }
                pthread_detach(thread_id);
                printf("[FILE SERVER] Thread launched for file transfer\n");

                memset(buffer, 0, BUFF_SIZE); 
            }
            //else error
        }
        //files sent where not proper
        else{   
            strcpy(buffer, "[SERVER] Improper input");
            send(conn_sock_fd, buffer, strlen(buffer), 0);
        }

        printf("[SERVER] Action complete\n");
    }
}