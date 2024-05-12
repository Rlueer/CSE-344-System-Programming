#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#define _XOPEN_SOURCE 700

#define FIFO1 "fifo1"
#define FIFO2 "fifo2"


int child_count = 0;
int array_size=10;  // Declare array size variable

// Function to handle SIGCHLD signal
void handle_child_termination(int signum) {
    int status;
    pid_t pid;
    
    // Reap all children that have exited
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // Print process ID of the terminated child
        char buffer[256];
        int len = snprintf(buffer, sizeof(buffer), "Child process %d exited with status: %d\n", pid, WEXITSTATUS(status)); // Format the message
        if(write(STDOUT_FILENO, buffer, len)<0){
            perror("Error writing to stdout");
            exit(EXIT_FAILURE);
        }
        child_count++;
    }
}

int main(int argc, char *argv[]) {
    // Remove FIFOs if they already exist
    unlink(FIFO1);
    unlink(FIFO2);

    // Loop to get valid array size from the user
    while (1) {
        if(write(STDOUT_FILENO, "Enter array size: ", strlen("Enter array size: "))<0){
            perror("Error writing to stdout");
            exit(EXIT_FAILURE);
        }
        
        
        // Read an integer from the user input
        int result = scanf("%d", &array_size);
        
        // Check if scanf successfully read an integer
        if (result == 1 && array_size>0) {
            // Input is valid, exit the loop
            break;
        } else {
            // Input is invalid, print an error message
            if(write(STDOUT_FILENO, "Invalid input. Please enter an integer value.\n", strlen("Invalid input. Please enter an integer value.\n"))<0){
            perror("Error writing to stdout");
            exit(EXIT_FAILURE);
            }
            // Clear the input buffer
            while (getchar() != '\n');
        }
    }

    // Create FIFOs
    if (mkfifo(FIFO1, 0666) == -1) {
        perror("Failed to create FIFO1");
        exit(EXIT_FAILURE);
    }

    if (mkfifo(FIFO2, 0666) == -1) {
        perror("Failed to create FIFO2");
        exit(EXIT_FAILURE);
    }

    // Set up the SIGCHLD handler using `sigaction`
    struct sigaction sa;
    // Initialize the signal mask to block all signals
    sigfillset(&sa.sa_mask);

    sa.sa_handler = handle_child_termination; // Set the handler function
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP; // Optional flags: restart system calls and ignore stopped children

    // Set the SIGCHLD handler
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("Failed to set up SIGCHLD handler");
        exit(EXIT_FAILURE);
    }

    // Generate an array of random numbers
    srand(time(NULL)); // Seed the random number generator
    int array[array_size];  // Define array with user-specified size
    for (int i = 0; i < array_size; i++) {
        array[i] = rand() % 10; // Random numbers between 0 and 9
    }

    // Fork to create two child processes
    pid_t pid1, pid2;
    if ((pid1 = fork()) == 0) {
        // Child process 1
        int j=0;
        while (j<5) {   // Wait for child processes to complete
            // Sleep for 2 seconds and print "proceeding"
            if(write(STDOUT_FILENO, "proceeding\n", 11)<0){
            perror("Error writing to stdout");
            exit(EXIT_FAILURE);
            }
            sleep(2); 
            j++;
        }

        // Open FIFO1 for reading
        int fd = open(FIFO1, O_RDONLY);
        if (fd == -1) {
            perror("Child 1 failed to open FIFO1 for reading");
            exit(EXIT_FAILURE);
        }

        int sum = 0;
        int array1[array_size]; // Define array with user-specified size
        read(fd, array1, sizeof(array1)); // Read numbers from FIFO1 and calculate sum
        for (int i = 0; i < array_size; i++) {
            sum += array1[i];
        }

        int fd2 = open(FIFO2, O_WRONLY);
        
        if (fd2 == -1) {
            perror("Child 1 failed to open FIFO2 for writing");
            exit(EXIT_FAILURE);
        }
        
        write(fd2, &sum, sizeof(sum)); // Write sum to FIFO2
        
        close(fd); // Close files
        close(fd2);

        exit(EXIT_SUCCESS);
    } else if ((pid2 = fork()) == 0) {
        // Child process 2
        int fd = open(FIFO2, O_RDONLY);
        
        if (fd == -1) {
            perror("Child 2 failed to open FIFO2 for reading");
            exit(EXIT_FAILURE);
        }
        
        int array2[array_size]; // Define array with user-specified size
        read(fd, array2, sizeof(array2));   // Read array from FIFO2
        
        char command[10];
        read(fd, command, sizeof(command));     // Read command from FIFO2

        close(fd);

        int fdx = open(FIFO2, O_RDONLY);
        if (fdx == -1) {
            perror("Child 2 failed to open FIFO2 for reading");
            exit(EXIT_FAILURE);
        }
        int sum=0;  
        read(fd, &sum, sizeof(sum));        // Read sum from FIFO2
        
        if (strcmp(command, "multiply") == 0) {  // Check if the command is "multiply"
            
            int multiplication = 1;
            for (int i = 0; i < array_size; i++) {
                multiplication *= array2[i];        // Calculate multiplication
            }
            
            int total_result = sum + multiplication;    // Calculate total result

            // Print the total result to stdout
            char buffer[256];
            int len = snprintf(buffer, sizeof(buffer), "Multiplicaton: %d , Sum: %d\nTotal result: %d\n", multiplication,sum,total_result);

            if(write(STDOUT_FILENO, buffer, len)<0){
            perror("Error writing to stdout");
            exit(EXIT_FAILURE);
            }
            
            close(fd);  // Close file and exit
            
            exit(EXIT_SUCCESS);
        } else {
            perror("Invalid command received by Child 2");
            exit(EXIT_FAILURE);
        }
    } else {
        // Parent process
        int fd2 = open(FIFO2, O_WRONLY);    // Open FIFO2
        
        if (fd2 == -1) {
            perror("Failed to open FIFOs for writing");
            exit(EXIT_FAILURE);
        }

        if(write(fd2, array, sizeof(array))<0){
            perror("Error writing to FIFO2");
            exit(EXIT_FAILURE);
        }   

        // Write command and sum to FIFO2
        char command[] = "multiply";
        if(write(fd2, command, sizeof(command))<0){
            perror("Error writing to FIFO2");
            exit(EXIT_FAILURE);
        }

        int fd1 = open(FIFO1, O_WRONLY);    // Open FIFO1
        if (fd1 == -1) {
            perror("Failed to open FIFOs for writing");
            exit(EXIT_FAILURE);
        }
        
        if(write(fd1, array, sizeof(array))<0){// Write random numbers to FIFO1
            perror("Error writing to FIFO1");
            exit(EXIT_FAILURE);
        }   

        while (child_count < 2) {   // Wait for child processes to complete
            // Sleep for 2 seconds and print "proceeding"
            if(write(STDOUT_FILENO, "proceeding.\n", 12)<0){
            perror("Error writing to stdout");
            exit(EXIT_FAILURE);
            }
            sleep(2);
        }
        
        close(fd1); // Close files and remove FIFOs
        close(fd2); 
        unlink(FIFO1);
        unlink(FIFO2);
        exit(EXIT_SUCCESS);
    }
}
