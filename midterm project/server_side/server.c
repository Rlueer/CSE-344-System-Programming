#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h> // Include the header file for waitpid function
#include <dirent.h>
#include <semaphore.h>

#define FIFO_PATH "/tmp/server_pipe"
#define LOG_FILE_PATH "server.log"

#define MAX_CLIENTS 100

// Global semaphore
sem_t sem;

// Array to store the PIDs of connected clients
pid_t connected_clients[MAX_CLIENTS];

// Global variables
int logFile = -1; // File descriptor for the log file
int maxClients;
int currentClients = 0;
int clientPID=-1;
int client_name_index= 1;
char clientName[100];
char clientFIFO[100];
char dirname[1024] ;
int parentPID = -500;

// Function prototypes
void initialize_server(char *dirname, int maxClients);
void handle_client_connection(int clientPID);
void handle_client_request(int clientPID, char *request,char *clientFIFO);
void handle_kill_signal(int sig);
void handle_child_termination(int sig);
void handle_readF_command(int clientFifoFd, const char* request);
void handle_writeT_command(int clientFifoFd, const char* request);
void handle_upload_command(int clientFifoFd, const char* request);
void handle_download_command(int clientFifoFd, const char* request);
void handle_download_command2(int clientFifoFd, const char* request);

// Function to check if a client PID is already connected
int is_client_connected(pid_t pid) {
    for (int i = 0; i < currentClients; i++) {
        if (connected_clients[i] == pid) {
            return 1;
        }
    }
    return 0;
}

void handle_child_termination(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        sem_wait(&sem);
        currentClients--; // Decrement current clients count
        sem_post(&sem);
    }
}
// Function to handle kill signal
void handle_kill_signal(int sig) {
    // Close log file if it's open
    if (logFile != -1) {
        close(logFile);
        logFile = -1;
    }

    // Send kill signals to all child processes
    for (int i = 0; i < currentClients; i++) {
        kill(connected_clients[i], SIGTERM);
    }
    return;
}

// Function to accept client connection
int accept_client() {

    int server_fd;
    pid_t client_pid;

    // Create/open the named pipe (FIFO) only if it hasn't been opened yet
    if (currentClients==0) {
        if (mkfifo(FIFO_PATH, 0666) == -1) {
            if (errno != EEXIST) {
                perror("mkfifo failed");
                return -1;
            }
        }
    }
    // Open FIFO for reading (blocking until a client writes to it)
    server_fd = open(FIFO_PATH, O_RDONLY);
    if (server_fd == -1) {
        perror("open failed");
        return -1;
    }

    // Read client's PID from the FIFO
    if (read(server_fd, &client_pid, sizeof(client_pid)) == -1) {
        perror("read failed");
        close(server_fd);
        return -1;
    }
    // Close the FIFO after reading client's PID
    close(server_fd);

    // Return the client's PID
    clientPID = client_pid;
    return client_pid;
    
}

// Function to initialize server
void initialize_server(char *dirname, int maxClients) {

    if (sem_init(&sem, 1, 1) == -1) {
        perror("sem_init failed");
        exit(EXIT_FAILURE);
    }
    
    struct stat st = {0};
    // Remove the directory if it already exists
    if (access(dirname, F_OK) == 0) {
        char rmCmd[256];
        snprintf(rmCmd, sizeof(rmCmd), "rm -rf %s", dirname);
        if (system(rmCmd) == -1) {
            perror("system command failed");
            exit(EXIT_FAILURE);
        }
    }
    // Create specified directory if it doesn't exist
    if (stat(dirname, &st) == -1) {
        if (mkdir(dirname, 0700) == -1) {
            perror("mkdir failed");
            exit(EXIT_FAILURE);
        }
    }

    // Open log file in the specified directory
    char logFilePath[256];
    snprintf(logFilePath, sizeof(logFilePath), "%s/%s", dirname, LOG_FILE_PATH);
    logFile = open(logFilePath, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (logFile == -1) {
        perror("open failed for log file");
        exit(EXIT_FAILURE);
    }

    // Print server's PID
    char msg[256];
    snprintf(msg, sizeof(msg), ">> Server Started PID %d\n", getpid());
    write(STDOUT_FILENO, msg, strlen(msg));
}

// Function to handle client connection
void handle_client_connection(int clientPID) {
    // Fork new process to handle client's requests
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
    if (pid == 0) {
        // Child process
        snprintf(clientFIFO, sizeof(clientFIFO), "/tmp/client_%d_fifo", clientPID);//!!!!!

        while (1) {
            sleep(0.2);   // Sleep for 1 second
    
            int client_pipe_fd = open(clientFIFO, O_RDONLY); /*!!!!!*/

            if (client_pipe_fd == -1) {
                perror("open failed for client FIFO");
                exit(EXIT_FAILURE);
            }
            char request[256]={0};
            ssize_t bytes_read = read(client_pipe_fd, request, sizeof(request));
            if (bytes_read == -1) {
                perror("read failed from client FIFO");
                close(client_pipe_fd);
                exit(EXIT_FAILURE);
            }
            request[bytes_read] = '\0';  // Null-terminate the request string

            if (strncmp(request, "upload ", 7) == 0) {
                dprintf(logFile, "Client PID %d requested: %s\n", clientPID, request);
                handle_upload_command(client_pipe_fd, request);
            }
            else if (strncmp(request, "download ", 9) == 0) {
                dprintf(logFile, "Client PID %d requested: %s\n", clientPID, request);
                handle_download_command(client_pipe_fd, request);
            }
            else if (strncmp(request, "quit", 4) == 0) {
                dprintf(logFile, "Client PID %d requested: %s\n", clientPID, request);
                write(client_pipe_fd, "quit", 4);
                printf(">> %s disconnected\n", clientName);
                dprintf(logFile, "%s disconnected..\n", clientName);
                kill(getpid(), SIGTERM);
                unlink(clientFIFO);
            }
            else{
                handle_client_request(clientPID, request,clientFIFO);   // Handle client's request
            }
            close(client_pipe_fd);
        }
        // Kill the child process
        kill(getpid(), SIGTERM);
    }
}

// Function to handle client request
void handle_client_request(int clientPID, char *request,char *clientFIFO) {
    int clientFifoFd = open(clientFIFO, O_WRONLY);
    if (clientFifoFd == -1) {
        perror("open failed for client FIFO");
        return;
    }
    // Log the client's request to the log file
    dprintf(logFile, "Client PID %d requested: %s\n", clientPID, request);

    // Parse client's request
    if (strcmp(request, "help") == 0) {
        char helpMsg[] = "Available commands are:\n help, list, readF, writeT, upload, download, archServer, quit, killServer\n";
        write(clientFifoFd, helpMsg, strlen(helpMsg));
        //dprintf(logFile, "%s", helpMsg);
    }
    else if(strcmp(request, "help list") == 0){
        write(clientFifoFd, "list\n    sends a request to display the list of files in Servers directory(also displays the list received from the Server)\n", 125);
    }
    else if(strcmp(request, "help readF") == 0){
        write(clientFifoFd, "readF <file> <line #>\n    requests to display the # line of the <file>, if no line number is given the whole contents of the file is requested (and displayed on the client side)\n", 179);
    }
    else if(strcmp(request, "help writeT") == 0){
        write(clientFifoFd, "writeT <file> <line #> <string>\n    request to write the content of “string” to the #th line the <file>, if the line # is not given writes to the end of file. If the file does not exists in Servers directory creates and edits the file at the same time\n", 257);
    }
    else if(strcmp(request, "help upload") == 0){
        write(clientFifoFd, "upload <file>\n    uploads the file from the current working directory of client to the Servers directory(beware of the cases no file in clients current working directory and file with the same name on Servers side)\n", 216);
    }
    else if(strcmp(request, "help download") == 0){
        write(clientFifoFd, "download <file>\n    request to receive <file> from Servers directory to client side\n", 85);
    }
    else if(strcmp(request, "help archServer") == 0){
        write(clientFifoFd, "archServer <fileName>.tar\n    Using fork, exec and tar utilities create a child process that will collect all the files currently available on the the Server side and store them in the <filename>.tar archive\n", 209);
    }
    else if(strcmp(request, "help killServer") == 0){
        write(clientFifoFd, "killServer\n   Send a kill request to the server\n", 49);
    }
    else if(strcmp(request, "help quit") == 0){
        write(clientFifoFd, "quit: Send write request to server side log file and quit\n", 59);
    }
    else if(strcmp(request, "help help") == 0){
        write(clientFifoFd, "display the list of possible client requests\n", 46);
    }
    else if (strcmp(request, "list") == 0) {
        // TODO: Implement list command
        char msg[2048]= {0};
        DIR *dir = opendir(dirname);
        if (dir == NULL) {
            perror("opendir failed");
            return;
        }
        // Read the directory entries
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            // Skip "." and ".." entries
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            // Concatenate the file name to the client message
            strcat(msg, entry->d_name);
            strcat(msg, "\n");
        }
        // Close the directory
        closedir(dir);
        write(clientFifoFd, msg, strlen(msg));

    } else if (strncmp(request, "readF", 5) == 0) {
        handle_readF_command(clientFifoFd, request);
    } else if (strncmp(request, "writeT", 6) == 0) {
        handle_writeT_command(clientFifoFd, request);
    } else if (strncmp(request, "upload", 6) == 0) {
    } else if (strncmp(request, "download", 8) == 0) {
    } else if (strncmp(request,"archServer", 10) == 0){
        char archive_name[1024];
        sscanf(request, "archServer %s", archive_name);
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            exit(-1);
        } else if (pid == 0) {
            //char *args[] = {"tar", "--exclude", archive_name, "-cf", archive_name, dirname, NULL};
            char *args[] = {"tar", "-C", dirname, "--exclude", archive_name, "-cf", archive_name, ".", NULL};
            execvp("tar", args);
            perror("execvp");
            exit(1);
        }
        else {
            int status;
            wait(&status);
            char send_buffer[2000] = {0};
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                DIR *dir = opendir(dirname);
                struct dirent *entry;
                int file_count = 0;
                long total_bytes = 0;
                struct stat st;

                while ((entry = readdir(dir)) != NULL) {
                    if (entry->d_type == DT_REG) { // If the entry is a regular file
                        file_count++;
                        stat(entry->d_name, &st);
                        total_bytes += st.st_size;
                    }
                }
                closedir(dir);

                snprintf(send_buffer, sizeof(send_buffer), 
                        "Archiving the current contents of the server...\n"
                        "Creating archive directory...\n"
                        "%d files downloaded ..%ld bytes transferred..\n"
                        "Calling tar utility .. child PID %d\n"
                        "Child returned with SUCCESS..\n"
                        "Copying the archive file..\n"
                        "Removing archive directory...\n"
                        "SUCCESS Server side files are archived in \"%s\"\n", 
                        file_count, total_bytes, getpid(), archive_name);
            }
            else {
                snprintf(send_buffer,sizeof(send_buffer),"Error creating archive (status: %d)\n", status);
            }
            //write(clientFifoFd,send_buffer,strlen(send_buffer));
            write(STDOUT_FILENO,send_buffer,strlen(send_buffer));
            // Use handle_download_command to send the file
            char download_request[1200];
            snprintf(download_request, sizeof(download_request), "download %s", archive_name);
            handle_download_command2(clientFifoFd, download_request);
        }
        close(clientFifoFd);
    } else if (strncmp(request, "killServer", 10) == 0) {
        // Handle killServer request
        char msg[256];
        snprintf(msg, sizeof(msg), ">> killServer request received from client PID %d. Terminating...\n", clientPID);
        
        write(clientFifoFd, "quit", 4);
        unlink(clientFIFO);
        write(logFile, msg, strlen(msg));
        write(STDOUT_FILENO, ">> kill signal from ", 20);
        write(STDOUT_FILENO, clientName, strlen(clientName));
        write(STDOUT_FILENO, ".. terminating...\n", 18);
        write(STDOUT_FILENO, ">> bye\n", 7);
        handle_kill_signal(SIGTERM);
        kill(getppid(), SIGTERM);  // Send SIGTERM signal to parent process (the server)
        // Clean up resources
        sem_destroy(&sem);
        exit(EXIT_SUCCESS);

    } else if (strcmp(request, "quit") == 0) {
        // Handle quit request
        write(clientFifoFd, "quit", 4);
        printf(">> %s disconnected\n", clientName);
        dprintf(logFile, "%s disconnected..\n", clientName);
        handle_child_termination(SIGTERM);
        unlink(clientFIFO);
    } else {
        // Invalid command
        char msg[256];
        snprintf(msg, sizeof(msg), "Invalid command from %s: %s\n", clientName, request);
        write(logFile, msg, strlen(msg));
        snprintf(msg, sizeof(msg), "   Invalid command: %s\n", request);
        write(clientFifoFd, msg, strlen(msg));
    }
    close(clientFifoFd);
}

void handle_readF_command(int clientFifoFd, const char* request) {
    char filename[256];
    int lineNum = -1;
    sscanf(request, "readF %s %d", filename, &lineNum);

    // Acquire semaphore before opening the file
    sem_wait(&sem);

    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        char errorMsg[512];
        snprintf(errorMsg, sizeof(errorMsg), "Error opening file: %s\n", filename);
        write(clientFifoFd, errorMsg, strlen(errorMsg));

        // Release semaphore on error
        sem_post(&sem);
        return;
    }

    // If lineNum is provided and valid (> 0), read the specific line
    if (lineNum > 0) {
        char lineBuffer[4096];
        int currentLine = 1;

        // Read line by line until the desired line or end of file
        while (fgets(lineBuffer, sizeof(lineBuffer), file) != NULL) {
            if (currentLine == lineNum) {
                // Write the line to the client FIFO
                size_t len = strlen(lineBuffer);
                if (write(clientFifoFd, lineBuffer, len) != len) {
                    perror("write failed");
                    fclose(file);

                    // Release semaphore on error
                    sem_post(&sem);
                    return;
                }
                break; // Exit loop after writing the line
            }
            currentLine++;
        }

        if (currentLine < lineNum) {
            // If the specified line number is out of range, report an error
            char errorMsg[512];
            snprintf(errorMsg, sizeof(errorMsg), "Line %d not found in file: %s\n", lineNum, filename);
            write(clientFifoFd, errorMsg, strlen(errorMsg));
        }
    } else {
        // Read and send the entire file in chunks
        char buffer[4096];
        ssize_t bytes_read;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
            if (write(clientFifoFd, buffer, bytes_read) != bytes_read) {
                perror("write failed");
                fclose(file);
                // Release semaphore on error
                sem_post(&sem);
                return;
            }
        }
    }

    fclose(file);
    // Release semaphore after finishing file operations
    sem_post(&sem);
}
// got error doesnt work for first line
void handle_writeT_command(int clientFifoFd, const char* request) {
    char filename[1000];
    int lineNum = -1;
    char content[1000];

    //if linenum entered -1 it means write to the end of the file
    sscanf(request, "writeT %s %d %[^\n]", filename, &lineNum, content);

    // Acquire semaphore before opening or modifying the file
    sem_wait(&sem);
    FILE *file = fopen(filename, "r+");
    if (file == NULL) {
        // If the file does not exist, create it
        file = fopen(filename, "w");
        if (file == NULL) {
            char errorMsg[1024];
            snprintf(errorMsg, sizeof(errorMsg), "Error opening file: %s\n", filename);
            write(clientFifoFd, errorMsg, strlen(errorMsg));

            // Release semaphore on error
            sem_post(&sem);
            return;
        }
    }

    if (lineNum == -1) {
        // If -1 number is given, append the string to the end of the file
        fseek(file, 0, SEEK_END);
    } else {
        // If a line number is given, loop through the file line by line until you reach the specified line
        char line[256];
        int currentLineNum = -1;
        while (fgets(line, sizeof(line), file) != NULL) {
            currentLineNum++;
            if (currentLineNum == lineNum-1) {
                break;
            }
        }
    }
    // Write the string to the file
    fputs(content, file);
    fputs("\n", file);

    fclose(file);

    // Release semaphore after finishing file operations
    sem_post(&sem);
}

void handle_upload_command(int clientFifoFd, const char* request) {
    char filename[256];
    sscanf(request, "upload %s", filename); // Extract filename from the request

    // Acquire semaphore before file operations
    sem_wait(&sem);

    // Check if file already exists in the server's directory
    if (access(filename, F_OK) != -1) {
        char errorMsg[300];
        snprintf(errorMsg, sizeof(errorMsg), "Error: File %s already exists on the server.\n", filename);
        write(clientFifoFd, errorMsg, strlen(errorMsg));
        // Release semaphore on error
        sem_post(&sem);
        return;
    }

    // Open the file for writing on the server
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        char errorMsg[300];
        snprintf(errorMsg, sizeof(errorMsg), "Error opening file: %s\n", filename);
        write(clientFifoFd, errorMsg, strlen(errorMsg));
        // Release semaphore on error
        sem_post(&sem);
        return;
    }

    // Read the file size from the client
    size_t fileSize;
    if (read(clientFifoFd, &fileSize, sizeof(fileSize)) != sizeof(fileSize)) {
        char errorMsg[300];
        snprintf(errorMsg, sizeof(errorMsg), "Error reading file size from client FIFO\n");
        write(clientFifoFd, errorMsg, strlen(errorMsg));
        fclose(file);
        // Release semaphore on error
        sem_post(&sem);
        return;
    }

    // Read the file data from the client and write it to the file
    char buffer[4096];
    size_t totalBytesRead = 0;
    while (totalBytesRead < fileSize) {
        ssize_t bytesRead = read(clientFifoFd, buffer, sizeof(buffer));
        if (bytesRead <= 0) {
            perror("Error reading file data from client FIFO");
            fclose(file);
            // Release semaphore on error
            sem_post(&sem);
            return;
        }
        if (fwrite(buffer, 1, bytesRead, file) != bytesRead) {
            perror("Error writing file data");
            fclose(file);
            // Release semaphore on error
            sem_post(&sem);
            return;
        }
        totalBytesRead += bytesRead;
    }

    // Close the file and inform the client of successful upload
    fclose(file);
    // Release semaphore after file operations
    sem_post(&sem);
}

void handle_download_command(int clientFifoFd, const char* request) {
    char filename[256];
    sscanf(request, "download %s", filename); // Extract filename from the request
    // Acquire semaphore before file operations
    sem_wait(&sem);
    // Open the file for reading
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        char errorMsg[300];
        snprintf(errorMsg, sizeof(errorMsg), "Error opening file: %s\n", filename);
        write(clientFifoFd, errorMsg, strlen(errorMsg));
        // Release semaphore on error
        sem_post(&sem);
        return;
    }
    close(clientFifoFd);
    // Release semaphore on error
    sem_post(&sem);

    clientFifoFd = open(clientFIFO, O_WRONLY);
    if(clientFifoFd == -1){
        perror("open failed for client FIFO");
        return;
    }

    // Acquire semaphore before file operations
    sem_wait(&sem);

    // Get the file sizes
    fseek(file, 0, SEEK_SET);
    size_t fileSize = 0;
    char ch;
    while (fread(&ch, 1, 1, file) == 1) {
        fileSize++;
    }
    rewind(file);  // Reset the file pointer to the beginning of the file

    // Read the file data and send it to the client
    char buffer[4096]={0};
    ssize_t bytes_read=0;
    size_t totalBytesRead = 0;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0 ) {
        if (write(clientFifoFd, buffer, bytes_read) == -1) {
            perror("write failed");
            close(clientFifoFd);
            fclose(file);
            // Release semaphore on error
            sem_post(&sem);
            exit(EXIT_FAILURE);
        }
        totalBytesRead += bytes_read;
    }
    //printf("totalBytesRead: %ld\n", totalBytesRead);

    fclose(file);

    close(clientFifoFd);
    // Release semaphore after file operations
    sem_post(&sem);
}

void handle_download_command2(int clientFifoFd, const char* request) {
    char filename[256];
    sscanf(request, "download %s", filename); // Extract filename from the request

    // Acquire semaphore before file operations
    sem_wait(&sem);
    // Open the file for reading
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        char errorMsg[300];
        snprintf(errorMsg, sizeof(errorMsg), "Error opening file: %s\n", filename);
        write(clientFifoFd, errorMsg, strlen(errorMsg));
        // Release semaphore on error
        sem_post(&sem);
        return;
    }
    close(clientFifoFd);
    // Release semaphore on error
    sem_post(&sem);

    FILE *fifo = fopen(clientFIFO, "wb");
    if(fifo == NULL){
        perror("fopen failed for client FIFO");
        return;
    }

    // Acquire semaphore before file operations
    sem_wait(&sem);

    // Read the file data and send it to the client
    char buffer[4096]={0};
    size_t bytes_read=0;
    size_t totalBytesRead = 0;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0 ) {
        if (fwrite(buffer, 1, bytes_read, fifo) != bytes_read) {
            perror("fwrite failed");
            fclose(fifo);
            fclose(file);
            // Release semaphore on error
            sem_post(&sem);
            exit(EXIT_FAILURE);
        }
        totalBytesRead += bytes_read;
    }
    //printf("totalBytesRead: %ld\n", totalBytesRead);

    fclose(file);
    fclose(fifo);
    // Release semaphore after file operations
    sem_post(&sem);
}

void handle_sigint(int sig) {
    // Log the received signal
    if (getpid() == parentPID) {
        // Log the received signal
        write(STDOUT_FILENO, ">> Ctrl+C signal received. Exiting...\n", 39);

        char msg[256];
        snprintf(msg, sizeof(msg), ">>Ctrl+C signal received. Exiting...\n");

        unlink(clientFIFO);
        write(logFile, msg, strlen(msg));

        handle_kill_signal(SIGTERM);
            // Clean up resources
        sem_destroy(&sem);
        // Other cleanup code...
        exit(EXIT_SUCCESS);
    }
}

// Main function
int main(int argc, char *argv[]) {
    
    parentPID = getpid();
    if (getcwd(dirname, sizeof(dirname)) == NULL) { // Get the current working directory
        perror("getcwd() error");
        return 1;
    }

    unlink(FIFO_PATH);

    // Set up signal handler for SIGINT
    signal(SIGINT, handle_sigint);

    if (argc != 3) {
        char msg[] = "Usage: <dirname> <maxClients>\n";
        write(STDERR_FILENO, msg, strlen(msg));
        exit(EXIT_FAILURE);
    }

    char *dirname = argv[1];    // Parse command line arguments
    maxClients = atoi(argv[2]);
    char clientFIF[maxClients][50];

    initialize_server(dirname, maxClients);     // Initialize server
    
    signal(SIGCHLD, handle_child_termination);    // Set up signal handler for kill signal

    write(STDOUT_FILENO, ">> waiting for clients...\n", strlen(">> waiting for clients...\n"));

    while (1) {
        clientPID = accept_client(); 

        if (clientPID == -1 ) {
            char msg[] = "Error accepting client connection\n";
            write(STDERR_FILENO, msg, strlen(msg));
            continue; // Handle error and continue accepting connections
        }
        if (is_client_connected(clientPID)) {
            continue; // Handle error and continue accepting connections
        }
        if (currentClients >= maxClients) {
            char errorMsg[256];
            snprintf(errorMsg, sizeof(errorMsg), ">> Connection request PID %d rejected. Queue FULL\n", clientPID);
            write(STDOUT_FILENO, errorMsg, strlen(errorMsg));
            write(logFile, errorMsg, strlen(errorMsg));
            continue;
        }
        // Add the new client's PID to the array of connected clients
        connected_clients[currentClients] = clientPID;

        snprintf(clientName, 10, "client%02d", client_name_index++);

        char msg[256];  // Print client's connection message
        snprintf(msg, sizeof(msg), ">> Client PID %d connected as \"%s\"\n", clientPID, clientName);
        write(STDOUT_FILENO, msg, strlen(msg));

        handle_client_connection(clientPID);    // Handle client connection

        currentClients++;   // Increment current clients count
        
    }

    return 0;
} 
