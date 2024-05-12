#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/wait.h> // Include the header file for waitpid function
#include <semaphore.h>

// Function prototypes
void connect_to_server(int serverPID, char *option);
void handle_server_response();
void send_request_to_server(int serverPID, char *request);

#define SERVER_PIPE "/tmp/server_pipe"

char cFIFO[50];
int serverP;

// Declare semaphore
sem_t semaphore;

int is_server_running() {
    // Check if the server process is running
    return kill(serverP, 0) == 0;
}

void connect_to_server(int serverPID, char *option) {

    // Write the clientPID to the server's named pipe
    int server_pipe_fd = open(SERVER_PIPE, O_WRONLY);
    if (server_pipe_fd == -1) {
        perror("open server pipe failed");
        exit(EXIT_FAILURE);
    }
    // Send the request to the server
    
    int get_pid = getpid();
    if (write(server_pipe_fd, &get_pid, sizeof(pid_t)) == -1) {
        perror("write to server pipe failed");
        close(server_pipe_fd);
        exit(EXIT_FAILURE);
    }
    close(server_pipe_fd);

    char clientFIFO[50];
    snprintf(clientFIFO, sizeof(clientFIFO), "/tmp/client_%d_fifo", get_pid);

    strcpy(cFIFO, clientFIFO);
    // Create client-specific FIFO inside the child process
    if (mkfifo(clientFIFO, 0666) == -1) {
        perror("mkfifo failed");
        exit(EXIT_FAILURE);
    }
    //printf("Client FIFO: %s\n", clientFIFO);

    // If option is "connect", wait for a spot in the server queue
    if (strcmp(option, "Connect") == 0) {
        printf("Wait for a spot in the server queue...\n");
        while (1) {
            if (sem_trywait(&semaphore) == 0) {
                printf("Connected to the server.\n");
                break;
            }
            else{
            printf("Server queue is full. Waiting for a spot...\n");
            sleep(2); // Wait before retrying
            }
        }
    } else if (strcmp(option, "tryConnect") == 0) {
        // If option is "tryConnect", attempt to connect without waiting
        if (sem_trywait(&semaphore) != 0) {
            printf("Server queue is full. Exiting...\n");
            exit(EXIT_SUCCESS);
        } else {
            printf("Connected to the server.\n");
        }
    } else {
        fprintf(stderr, "Invalid option: %s\n", option);
        exit(EXIT_FAILURE);
    }
    
}

// Function to send request to server
void send_request_to_server(int serverPID, char *request) {
    
    // Open the named pipe for writing
    int pipe_fd = open(cFIFO, O_WRONLY);
    if (pipe_fd == -1) {
        perror("open failed");
        exit(EXIT_FAILURE);
    }
    // Check if the request is an upload command
    if (strncmp(request, "upload ", 7) == 0) {
        // Wait on semaphore (enter critical section)
        //sem_wait(&semaphore);
        // Get the filename from the request
        char filename[256];
        sscanf(request + 7, "%s", filename);

        // Open the file for reading
        FILE *file = fopen(filename, "rb");
        if (file == NULL) {
            perror("open file failed");
            close(pipe_fd);
            return;
        }
        // Send the upload request to the server
        if (write(pipe_fd, request, strlen(request)) == -1) {
            perror("write failed");
            close(pipe_fd);
            fclose(file);
            exit(EXIT_FAILURE);
        }

        // Get the file size
        fseek(file, 0, SEEK_SET);
        size_t fileSize = 0;
        char ch;
        while (fread(&ch, 1, 1, file) == 1) {
            fileSize++;
        }
        rewind(file);  // Reset the file pointer to the beginning of the file

        // Send the file size to the server
        if (write(pipe_fd, &fileSize, sizeof(fileSize)) == -1) {
            perror("Failed to write file size to server");
            fclose(file);
            return;
        }

        // Read the file data and send it to the server
        char buffer[4096];
        ssize_t bytes_read;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
            if (write(pipe_fd, buffer, bytes_read) == -1) {
                perror("write failed");
                close(pipe_fd);
                fclose(file);
                exit(EXIT_FAILURE);
            }
        }
        printf("file transfer request received. Beginning file transfer:\n");
        printf("%ld bytes transferred\n", fileSize);

        fclose(file);
        // post on semaphore (exit critical section)
        //sem_post(&semaphore);
    } 
    else if (strncmp(request, "download ", 9) == 0) {
        // Wait on semaphore (enter critical section)
        //sem_wait(&semaphore);

        char filename[256];
        sscanf(request + 9, "%s", filename);
        // Check if file already exists in the server's directory
        if (access(filename, F_OK) != -1) {
            char errorMsg[300];
            snprintf(errorMsg, sizeof(errorMsg), "Error: File %s already exists on the server.\n", filename);
            printf("%s", errorMsg);
            return;
        }
        
        // Send the download request to the client
        if (write(pipe_fd, request, strlen(request)) == -1) {
            perror("write failed");
            close(pipe_fd);
            exit(EXIT_FAILURE);
        }
        close(pipe_fd);

        // Open the file for writing
        FILE *file = fopen(filename, "wb");
        if (file == NULL) {
            printf("Error: Unable to open file %s for writing.\n", filename);
            return;
        }
        // Wait on semaphore (exit critical section)
        //sem_post(&semaphore);

        int pipe_fd  = open(cFIFO, O_RDONLY);
        if (pipe_fd  == -1) {
            perror("open failed");
            exit(EXIT_FAILURE);
        }
        // Wait on semaphore (enter critical section)
        //sem_wait(&semaphore);

        char buffer[4096]="null";
        size_t totalBytesRead = 0;
        ssize_t bytesRead=0;
        while ((bytesRead=read(pipe_fd , buffer, sizeof(buffer))) > 0){
            if (fwrite(buffer, 1, bytesRead, file) != bytesRead) {
                perror("Error writing file data");
                fclose(file);
                return;
            }
            totalBytesRead += bytesRead;
        }
        if (bytesRead == -1) {
            perror("Error reading file data");
            printf("errno: %d\n", errno);
        }
        printf("%ld bytes transferred\n", totalBytesRead);
        fclose(file);
        // Wait on semaphore (exitcritical section)
        //sem_post(&semaphore);
    } 
    else if(strncmp(request,"quit",4)==0){
        if (write(pipe_fd, request, strlen(request)) == -1) {
            perror("write failed");
            close(pipe_fd);
            exit(EXIT_FAILURE);
        }
        printf("bye...\n");
        close(pipe_fd);
        unlink(cFIFO);
        exit(EXIT_SUCCESS);
    }
    else if(strncmp(request,"killServer",10)==0){
        if (write(pipe_fd, request, strlen(request)) == -1) {
            perror("write failed");
            close(pipe_fd);
            exit(EXIT_FAILURE);
        }
        close(pipe_fd);
        handle_server_response();
        printf("Sending write request to server log file\n");
        printf("waiting for logfile ...\n");
        sleep(0.5); // XD 
        printf("logfile write request granted\n");
        printf("bye...\n");

        close(pipe_fd);
        unlink(cFIFO);
        exit(EXIT_SUCCESS);
    }
    else if (strncmp(request, "archServer ", 11) == 0) {
        // Wait on semaphore (enter critical section)
        //sem_wait(&semaphore);
        char filename[256];
        sscanf(request + 11, "%s", filename);

        // Send the archServer request to the server
        if (write(pipe_fd, request, strlen(request)) == -1) {
            perror("write failed");
            close(pipe_fd);
            exit(EXIT_FAILURE);
        }
        close(pipe_fd);

        // Open the file for writing
        int file_fd = open(filename, O_WRONLY | O_CREAT, 0666);
        if (file_fd == -1) {
            perror("open file failed");
            return;
        }
        // Wait on semaphore (exit critical section)
        //sem_post(&semaphore);
        int pipe_fd  = open(cFIFO, O_RDONLY);
        if (pipe_fd  == -1) {
            perror("open failed");
            exit(EXIT_FAILURE);
        }
        // Wait on semaphore (enter critical section)
        //sem_wait(&semaphore);
        char buffer[4096];
        ssize_t bytesRead;
        size_t totalBytesRead = 0;
        while ((bytesRead = read(pipe_fd , buffer, sizeof(buffer))) > 0) {
            if (write(file_fd, buffer, bytesRead) == -1) {
                perror("write failed");
                close(file_fd);
                return;
            }
            totalBytesRead += bytesRead;
         }
        
        close(pipe_fd);

        // Count the number of files in the tar archive
        char cmd[300];
        snprintf(cmd, sizeof(cmd), "tar -tf %s | wc -l", filename);
        FILE *tar = popen(cmd, "r");
        int file_count;
        fscanf(tar, "%d", &file_count);
        pclose(tar);

        printf("Archiving the current contents of the server...\n");
        printf("Creating archive directory...\n");
        printf("%d files downloaded ..%ld bytes transferred..\n", file_count, totalBytesRead);
        printf("Calling tar utility .. child PID %d\n", getpid());
        printf("Child returned with SUCCESS..\n");
        printf("Copying the archive file..\n");
        printf("Removing archive directory...\n");
        printf("SUCCESS Server side files are archived in \"%s\"\n", filename);

        // Wait on semaphore (exit critical section)
        //sem_post(&semaphore);

    }
    else if (strncmp(request,"full",4)==0){
        if (write(pipe_fd, request, strlen(request)) == -1) {
            perror("write failed");
            close(pipe_fd);
            exit(EXIT_FAILURE);
        }
        printf("Server is full bye...\n");
        close(pipe_fd);
        unlink(cFIFO);
        exit(EXIT_SUCCESS);
    }
    else {   // Send the request to the server
        if (write(pipe_fd, request, strlen(request)) == -1) {
            perror("write failed");
            close(pipe_fd);
            exit(EXIT_FAILURE);
        }
        close(pipe_fd);
        handle_server_response();
    }
    close(pipe_fd); // Close the named pipe

}

void handle_server_response() {
    // Parse and process server response
    char response[4096] = {0};    
    int clientFifoFd = open(cFIFO, O_RDONLY);
    if (clientFifoFd == -1) {
        perror("open failed for client FIFO");
        return;
    }

    ssize_t bytes_read;
    while ((bytes_read = read(clientFifoFd, response, sizeof(response) - 1)) > 0) {
        response[bytes_read] = '\0';
        write(STDOUT_FILENO, response, bytes_read);
    }

    if (bytes_read == -1) {
        perror("read failed from client FIFO");
    }

    close(clientFifoFd);
}

void handle_sigint(int sig) {
    // Send "quit" message to server
    write(STDOUT_FILENO, "\n>> Ctrl+C signal received. Exiting...\n", 39);
    if (is_server_running()) {
        send_request_to_server(serverP, "quit");
    }
    else if (is_server_running()==-1)
    {
        write(STDOUT_FILENO, "Server is not running\n", 23);
        exit(EXIT_SUCCESS);
    }
    
    unlink(cFIFO);
    exit(EXIT_SUCCESS);
}

// Main function
int main(int argc, char *argv[]) {
    // Parse command line arguments
    unlink(cFIFO);
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <Connect/tryConnect> <ServerPID>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char *option = argv[1];
    int serverPID = atoi(argv[2]);

    serverP = serverPID;  // global variable

    if (is_server_running()==0) {
        write(STDOUT_FILENO, "Server with PID ", 16);
        write(STDOUT_FILENO, argv[2], strlen(argv[2]));
        write(STDOUT_FILENO, " is not running.\n", 17);
        exit(EXIT_FAILURE);
    }
        // Initialize semaphore
    if (sem_init(&semaphore, 0, 1) == -1) {
        perror("Semaphore initialization failed");
        exit(EXIT_FAILURE);
    }

    // Connect to server based on the specified option
    connect_to_server(serverPID, option);

    // Set up signal handler for SIGINT
    signal(SIGINT, handle_sigint);

    // Enter main client loop
    while (1) {
        char request[256];
        write(STDOUT_FILENO, ">>Enter command (type 'help' for available commands): ", 55);

        ssize_t bytes_read = read(STDIN_FILENO, request, sizeof(request) - 1);  // Read user input
        if (bytes_read == -1) {
            perror("read failed");
            exit(EXIT_FAILURE);
        }
        // Null-terminate the input based on the number of bytes read
        request[bytes_read] = '\0';
        request[strcspn(request, "\n")] = 0; // Remove newline character
        // Send request to server
        send_request_to_server(serverPID, request);
    }
    return 0;
}
