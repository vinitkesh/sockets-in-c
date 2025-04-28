/*
    Arguments: server ip address
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     // used for read, write, close
#include <sys/socket.h> // used for socket, bind, listen, accept
#include <sys/types.h>  // used for socket, bind, listen, accept
#include <netinet/in.h> // used for sockaddr_in
#include <netdb.h>      // used for hostent
#include <bits/pthreadtypes.h>
#include <pthread.h>

#define SERVER_PORT 444
#define FILE_PORT 555
#define BUFF_SIZE 10000
#define MAX_ACCEPT_BACKLOG 5
#define EOF_MARKER "[EOF]"
#define EOF_MARKER_LEN 5

/*Structure to handle receiving of files*/
struct receive_file_format{
    int server_socket;
    int totatl_files;
};

/*Function to handle errors*/
void error(const char * msg){
    perror(msg);
    exit(1);
}


/*Function to handle receiving of files from the server using threads*/
void * receive_file(void* input) {

    struct receive_file_format * data = (struct receive_file_format *) input;
    int server_socket = data->server_socket;
    int total_files = data->totatl_files;
    
    char buffer[BUFF_SIZE];

    for(int i=0;i < total_files;i++) {

        // Get filename
        memset(buffer, 0, BUFF_SIZE);
        recv(server_socket, buffer, BUFF_SIZE, 0);
        if(strncmp("[FILE SERVER] Sending: ", buffer, 23)!=0){
            printf("[ERROR] Error receiving file\n");
            exit(1);
        }

        char *filename = buffer + 23;
        char *output_filename = strdup(filename);
        if (output_filename == NULL) {
            error("[ERROR] Memory allocation failed");
        }
        ssize_t bytes_received;

        
        memset(buffer, 0, BUFF_SIZE);
        strcpy(buffer, "[FILE SERVER] Starting file transfer");
        send(server_socket, buffer, strlen(buffer), 0);
        memset(buffer, 0, BUFF_SIZE);

        FILE *file = fopen(output_filename, "wb");
        if (file == NULL) {
            error("[ERROR] Cannot open output file");
        }

        //Check if server has file
        recv(server_socket, buffer, BUFF_SIZE, 0); 
        if(strncmp("[ERROR] File not found: ", buffer, 24)==0){
            send(server_socket, "[FILE SERVER] OK", 27, 0);
            printf("%s\n", buffer);
            continue;
        }
        send(server_socket, "[FILE SERVER] OK", 27, 0);
        printf("[FILE SERVER] File transfer started : %s\n", output_filename);

        // Receive and write to file in chunks
        while (1) {
            bytes_received = recv(server_socket, buffer, BUFF_SIZE, 0);
            // Find where the marker begins
            char temp[BUFF_SIZE + 1];
            memcpy(temp, buffer, bytes_received);
            temp[bytes_received] = '\0';
            // Check if our marker is in this chunk
            if (strstr(temp, EOF_MARKER)!=NULL) {
                char *marker_pos = strstr(temp, EOF_MARKER);
                size_t pos = marker_pos - temp;
                
                // Write the part of the data before the marker
                if (pos > 0) {
                    fwrite(buffer, 1, pos, file);
                }
                break;  // Exit the loop since the marker was found.
            } else {
                // Write entire chunk if no marker is found.
                fwrite(buffer, 1, bytes_received, file);
            }
            memset(buffer, 0, BUFF_SIZE);
        }

        if (bytes_received == -1) {
            error("[ERROR] Error receiving file");
        }

        printf("[FILE SERVER] File received successfully: %s\n", output_filename);
        fclose(file);
        free(output_filename);

        send(server_socket, "[FILE SERVER] File received", 27, 0);
    }

    printf("[FILE SERVER] All files received\n");
    close(server_socket);

    free(data);
    pthread_exit(NULL);
}


/*Function to set up tcp connection on port no 555
    (server and client roles reversed)*/
int set_connection() {
    
    // Creating listening sock
    int listen_file_socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    // Setting sock opt reuse addr
    int enable = 1;
    setsockopt(listen_file_socket_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

    // Creating an object of struct socketaddr_in
    struct sockaddr_in server_addr;

    // Setting up server addr
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(FILE_PORT);

    if (bind(listen_file_socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        error("[FILE SERVER ERROR] Bind failed\n");
    }
    if (listen(listen_file_socket_fd, MAX_ACCEPT_BACKLOG) < 0) {
        error("[FILE SERVER ERROR] Listen failed\n");
    }
    printf("[FILE SERVER] Server listening on port %d for files...\n", FILE_PORT);
 
    return listen_file_socket_fd;
}

/* Main Function */
int main(int argc, char* argv[]){

    //variable declarations
    if(argc < 2){
        error("[ERROR] Server ip address not provided\nProgram terminated\n");
    }
    char buffer[BUFF_SIZE];   
    int socket_fd,n;
    struct sockaddr_in server_address;  
    struct hostent *server;     
    socklen_t server_len;

    int file_listen_sock_fd=-1;

    printf("[CLIENT] Opening socket...\n");

    //create a socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);   
    if(socket_fd < 0){
        error("[ERROR] Error opening socket\n");
    }

    //get the server
    server = gethostbyname(argv[1]);    
    if(server == NULL){
        error("[ERROR] Error, no such host\n");
    }
    

    //set up server address
    memset((char *) &server_address, 0, sizeof(server_address)); //clear the server_address if anything is there
    server_address.sin_family = AF_INET;   //set the family
    bcopy((char *)server->h_addr_list[0], (char *)&server_address.sin_addr.s_addr, server->h_length);   //copy the server address
    server_address.sin_port = htons(SERVER_PORT);   //set the port - host to network short
    

    //connect to the server
    if(connect(socket_fd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0){   
        error("[ERROR] Connection failed\n");
    }
    printf("[CLIENT] Successfully connected to server\n");

    while(1){

        //write the message to the server
        memset(buffer, 0, BUFF_SIZE);
        fgets(buffer, BUFF_SIZE, stdin);   
        n = send(socket_fd, buffer, strlen(buffer), 0);  
        if(n<0){
            error("[ERROR] Error on sending message\n");
        }

        //Close the connection if client entered exit
        if(strncmp("exit",buffer,4)==0){
            break;
        }

        memset(buffer, 0, BUFF_SIZE);   //clear the buffer
        recv(socket_fd, buffer, BUFF_SIZE, 0);

        //If input was improper
        if(strncmp("[SERVER] Improper input", buffer, 23)==0){
            printf("[SERVER] Improper input to server\n");
        }
        else{
            //Set up tcp 555 connection from client side
            printf("[SERVER] Start the file server\n");
            //Send started signal
            memset(buffer, 0, BUFF_SIZE);

            //If already exisiting connection set up
            if(file_listen_sock_fd == -1){
                file_listen_sock_fd = set_connection();
                strcpy(buffer, "[FILE SERVER] Started connection");
            }
            else{
                strcpy(buffer, "[FILE SERVER] Started: Using pre-existing connection");
                printf("[FILE SERVER] Using pre-existing connection\n", buffer);
            }
            send(socket_fd, buffer, strlen(buffer), 0);
            // Accept client connection

            // Creating an object of struct socketaddr_in
            struct sockaddr_in client_s_addr;
            socklen_t client_s_addr_len = sizeof(client_s_addr);
            int conn_file_sock_fd = accept(file_listen_sock_fd, (struct sockaddr *)&client_s_addr, &client_s_addr_len);
        

            printf("[FILE SERVER] Connected to server\n");
            
            //receive the number of files
            memset(buffer, 0, BUFF_SIZE);   //clear the buffer
            recv(socket_fd, buffer, BUFF_SIZE, 0);
            char * total = buffer+24;
            int total_files = atoi(total);
            printf("[SERVER] Sending %d files\n", total_files);
            

            // Launch threads for file transfer
            struct receive_file_format * data = (struct receive_file_format *)malloc(sizeof(struct receive_file_format));
            data->server_socket = conn_file_sock_fd;
            data->totatl_files = total_files;

            pthread_t thread_id;
            printf("[FILE SERVER] Creating thread for file transfer\n");
            if (pthread_create(&thread_id, NULL, receive_file, (void*)data) != 0) {
                perror("[ERROR] Failed to create thread");
                free(data);
            }
            // Detach the thread to allow independent execution
            pthread_detach(thread_id);
            printf("[FILE SERVER] Thread launched for file transfer\n");

        }
    }
    // Close the listening socket if it is opened by client
    if(file_listen_sock_fd != -1){
        printf("[FILE SERVER] Closing connection...\n");
        close(file_listen_sock_fd);
    }

    printf("[CLIENT] Closing connection...\n");
    shutdown(socket_fd,SHUT_RDWR);   //close the socket
    close(socket_fd);
    return 0;
}

/*
 * ================================
 * Function: memset()
 * -------------------------------
 * Purpose:
 *  - Fills a block of memory with a specified value.
 * 
 * Prototype:
 *  void *memset(void *s, int c, size_t n);
 * 
 * Parameters:
 *  - s: Pointer to the memory block.
 *  - c: Value to be set (converted to unsigned char).
 *  - n: Number of bytes to set.
 * 
 * Returns:
 *  - Pointer to the memory block s.
 *
 * Example:
 *  char buffer[100];
 *  memset(buffer, 0, sizeof(buffer));  // Clears buffer with zeros
 */

/*
 * ================================
 * Function: bcopy()
 * -------------------------------
 * Purpose:
 *  - Copies n bytes from src to dest.
 * 
 * Prototype:
 *  void bcopy(const void *src, void *dest, size_t n);
 * 
 * Parameters:
 *  - src: Source memory location.
 *  - dest: Destination memory location.
 *  - n: Number of bytes to copy.
 * 
 * Returns:
 *  - Nothing.
 * 
 * Note: Deprecated. Use memcpy() instead.
 *
 * Example:
 *  char src[] = "Hello";
 *  char dest[10];
 *  bcopy(src, dest, strlen(src) + 1);
 */

/*
 * ================================
 * Function: bind()
 * -------------------------------
 * Purpose:
 *  - Assigns a socket to an IP address and port.
 * 
 * Prototype:
 *  int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
 * 
 * Parameters:
 *  - sockfd: Socket descriptor.
 *  - addr: Pointer to sockaddr structure containing address info.
 *  - addrlen: Size of the sockaddr structure.
 * 
 * Returns:
 *  - 0 on success, -1 on error.
 * 
 * Example:
 *  struct sockaddr_in server_addr;
 *  server_addr.sin_family = AF_INET;
 *  server_addr.sin_addr.s_addr = INADDR_ANY;
 *  server_addr.sin_port = htons(555);
 *  bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
 */

/*
 * ================================
 * Function: listen()
 * -------------------------------
 * Purpose:
 *  - Marks a socket as a passive socket to listen for incoming connections.
 * 
 * Prototype:
 *  int listen(int sockfd, int backlog);
 * 
 * Parameters:
 *  - sockfd: Socket descriptor.
 *  - backlog: Maximum number of pending connections.
 * 
 * Returns:
 *  - 0 on success, -1 on error.
 * 
 * Example:
 *  listen(socket_fd, 5);
 */

/*
 * ================================
 * Function: accept()
 * -------------------------------
 * Purpose:
 *  - Accepts an incoming connection on a listening socket.
 * 
 * Prototype:
 *  int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
 * 
 * Parameters:
 *  - sockfd: Listening socket descriptor.
 *  - addr: Pointer to sockaddr structure to store client address.
 *  - addrlen: Size of sockaddr structure.
 * 
 * Returns:
 *  - New socket descriptor on success, -1 on error.
 * 
 * Example:
 *  struct sockaddr_in client_addr;
 *  socklen_t client_len = sizeof(client_addr);
 *  int client_fd = accept(socket_fd, (struct sockaddr *)&client_addr, &client_len);
 */

/*
 * ================================
 * Function: connect()
 * -------------------------------
 * Purpose:
 *  - Establishes a connection to a server.
 * 
 * Prototype:
 *  int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
 * 
 * Parameters:
 *  - sockfd: Socket descriptor.
 *  - addr: Pointer to sockaddr structure containing server address.
 *  - addrlen: Size of the sockaddr structure.
 * 
 * Returns:
 *  - 0 on success, -1 on error.
 * 
 * Example:
 *  connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
 */

/*
 * ================================
 * Function: recv()
 * -------------------------------
 * Purpose:
 *  - Receives data from a connected socket.
 * 
 * Prototype:
 *  ssize_t recv(int sockfd, void *buf, size_t len, int flags);
 * 
 * Parameters:
 *  - sockfd: Socket descriptor.
 *  - buf: Pointer to buffer to store received data.
 *  - len: Maximum size of the buffer.
 *  - flags: Special options (usually 0).
 * 
 * Returns:
 *  - Number of bytes received, -1 on error.
 * 
 * Example:
 *  char buffer[1024];
 *  ssize_t bytes_received = recv(socket_fd, buffer, sizeof(buffer), 0);
 */

/*
 * ================================
 * Function: send()
 * -------------------------------
 * Purpose:
 *  - Sends data to a connected socket.
 * 
 * Prototype:
 *  ssize_t send(int sockfd, const void *buf, size_t len, int flags);
 * 
 * Parameters:
 *  - sockfd: Socket descriptor.
 *  - buf: Pointer to data to be sent.
 *  - len: Size of the data.
 *  - flags: Special options (usually 0).
 * 
 * Returns:
 *  - Number of bytes sent, -1 on error.
 * 
 * Example:
 *  char message[] = "Hello, Server!";
 *  send(socket_fd, message, strlen(message), 0);
 */

/*
 * ================================
 * Function: shutdown()
 * -------------------------------
 * Purpose:
 *  - Shuts down part or all of a socket connection.
 * 
 * Prototype:
 *  int shutdown(int sockfd, int how);
 * 
 * Parameters:
 *  - sockfd: Socket descriptor.
 *  - how: Specifies how the connection should be shut down:
 *         SHUT_RD   - No more receiving.
 *         SHUT_WR   - No more sending.
 *         SHUT_RDWR - No more receiving and sending.
 * 
 * Returns:
 *  - 0 on success, -1 on error.
 * 
 * Example:
 *  shutdown(socket_fd, SHUT_RDWR);
 */

/*
 * ================================
 * Function: close()
 * -------------------------------
 * Purpose:
 *  - Closes a file or socket descriptor.
 * 
 * Prototype:
 *  int close(int fd);
 * 
 * Parameters:
 *  - fd: File descriptor or socket descriptor.
 * 
 * Returns:
 *  - 0 on success, -1 on error.
 * 
 * Example:
 *  close(socket_fd);
 */

/*
 * ================================
 * Function: socket()
 * -------------------------------
 * Purpose:
 *  - Creates a new socket.
 * 
 * Prototype:
 *  int socket(int domain, int type, int protocol);
 * 
 * Parameters:
 *  - domain: Communication domain (AF_INET for IPv4).
 *  - type: Socket type (SOCK_STREAM for TCP).
 *  - protocol: Protocol to use (usually 0 for automatic selection).
 * 
 * Returns:
 *  - Socket descriptor on success, -1 on error.
 * 
 * Example:
 *  int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
 */

/*
 * ================================
 * Function: fgets()
 * -------------------------------
 * Purpose:
 *  - Reads a line from the specified stream.
 * 
 * Prototype:
 *  char *fgets(char *str, int n, FILE *stream);
 * 
 * Parameters:
 *  - str: Pointer to the buffer where the string is stored.
 *  - n: Maximum number of characters to read.
 *  - stream: Input stream (stdin for standard input).
 * 
 * Returns:
 *  - Pointer to the string on success, NULL on error.
 * 
 * Example:
 *  char buffer[100];
 *  fgets(buffer, 100, stdin);
 */

/*
 * ================================
 * Function: fopen()
 * -------------------------------
 * Purpose:
 *  - Opens a file for reading, writing, or both.
 * 
 * Prototype:
 *  FILE *fopen(const char *filename, const char *mode);
 * 
 * Parameters:
 *  - filename: Name of the file to open.
 *  - mode: Mode of file access ("r" for read, "wb" for write binary).
 * 
 * Returns:
 *  - Pointer to FILE object on success, NULL on error.
 * 
 * Example:
 *  FILE *file = fopen("output.txt", "wb");
 */

/*
 * ================================
 * Function: fwrite()
 * -------------------------------
 * Purpose:
 *  - Writes data from a buffer to a file.
 * 
 * Prototype:
 *  size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
 * 
 * Parameters:
 *  - ptr: Pointer to the data.
 *  - size: Size of each element.
 *  - nmemb: Number of elements.
 *  - stream: File pointer.
 * 
 * Returns:
 *  - Number of elements successfully written.
 * 
 * Example:
 *  fwrite(buffer, 1, bytes_received, file);
 */

/*
 * ================================
 * Function: perror()
 * -------------------------------
 * Purpose:
 *  - Prints an error message based on the value of errno.
 * 
 * Prototype:
 *  void perror(const char *s);
 * 
 * Parameters:
 *  - s: Custom error message to print.
 * 
 * Returns:
 *  - Nothing.
 * 
 * Example:
 *  perror("[ERROR] File open failed");
 */

/*
 * ================================
 * Function: pthread_create()
 * -------------------------------
 * Purpose:
 *  - Creates a new thread.
 * 
 * Prototype:
 *  int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);
 * 
 * Parameters:
 *  - thread: Pointer to pthread_t to store thread ID.
 *  - attr: Thread attributes (NULL for default).
 *  - start_routine: Pointer to the function to run.
 *  - arg: Argument passed to the function.
 * 
 * Returns:
 *  - 0 on success, error code on failure.
 * 
 * Example:
 *  pthread_t thread_id;
 *  pthread_create(&thread_id, NULL, receive_file, (void *)data);
 */

/*
 * ================================
 * Function: pthread_detach()
 * -------------------------------
 * Purpose:
 *  - Detaches a thread so that it cleans up automatically when finished.
 * 
 * Prototype:
 *  int pthread_detach(pthread_t thread);
 * 
 * Parameters:
 *  - thread: Thread ID.
 * 
 * Returns:
 *  - 0 on success, error code on failure.
 * 
 * Example:
 *  pthread_detach(thread_id);
 */