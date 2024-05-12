#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_NAME_LENGTH 100
#define MAX_GRADE_LENGTH 3
#define MAX_LINE_LENGTH (MAX_NAME_LENGTH + MAX_GRADE_LENGTH + 3) // 3 for ", "

void addStudentGrade(const char *filename, const char *name, const char *grade);
void searchStudent(const char *filename, const char *name);
void sortAll(const char *filename);
void showAll(const char *filename);
void listGrades(const char *filename);
void listSome(const char *filename, int numEntries, int pageNumber);
void printUsage();
void logTaskCompletion(const char *taskName);
int readInput(char *buffer, size_t size);

void addStudentGrade(const char *filename, const char *name, const char *grade) {
    int file_desc = open(filename, O_RDWR|O_CREAT, 0666);

    if (file_desc == -1) {
        char errorMsg[] = "Error: Unable to open file.\n";
        write(STDOUT_FILENO, errorMsg, sizeof(errorMsg) - 1); // Using write instead of printf
        logTaskCompletion("Error: Unable to open file.");
        return;
    }

    char currentName[MAX_NAME_LENGTH-1];
    int pos = 0;
    int bytesRead;
    char c;

    while ((bytesRead = read(file_desc, &c, 1)) > 0) {
        if (c == ',') {
            currentName[pos] = '\0'; // Null-terminate the string
            if (strcmp(currentName, name) == 0) {
                char errorMsg[] = "Error: Student already exists.\n";
                write(STDOUT_FILENO, errorMsg, sizeof(errorMsg) - 1); // Using write instead of printf
                char logline[MAX_LINE_LENGTH + 50];
                strcpy(logline, "Error: Student ");
                strncat(logline, name, MAX_NAME_LENGTH - 1);
                strcat(logline, " already exists.");
                logTaskCompletion(logline);
                close(file_desc);
                return;
            }
            pos = 0; // Reset position for next name
        } else if (c == '\n') {
            pos = 0; // Reset position for next name
            currentName[0] = '\0'; // Reset currentName to an empty string
        } else {
            currentName[pos++] = c; // Store character in the name buffer
        }
    }

    // Close the file before reopening in write mode
    close(file_desc);

    // Reopen the file in append mode to add the new student's grade
    file_desc = open(filename, O_WRONLY | O_APPEND);
    if (file_desc == -1) {
        char errorMsg[] = "Error: Unable to open file.\n";
        write(STDOUT_FILENO, errorMsg, sizeof(errorMsg) - 1); // Using write instead of printf
        logTaskCompletion("Error: Unable to open file.");
        return;
    }

    char buffer[MAX_LINE_LENGTH + 50]; // Ensure enough space for the message
    strcpy(buffer, name);
    strcat(buffer, ", ");
    strcat(buffer, grade);
    strcat(buffer, "\n");

    write(file_desc, buffer, strlen(buffer)); // Using write instead of dprintf

    close(file_desc);

    char logline[MAX_LINE_LENGTH + 50];
    strcpy(logline, "Add Student Grade: ");
    strcat(logline, name);
    strcat(logline, ", ");
    strcat(logline, grade);
    logTaskCompletion(logline);
}

void searchStudent(const char *filename, const char *inputName) {
    int file_desc = open(filename, O_RDONLY);
    if (file_desc == -1) {
        char errorMsg[] = "Error: Unable to open file.\n";
        write(STDOUT_FILENO, errorMsg, sizeof(errorMsg) - 1);
        char logline[] = "Error: file not found.";
        logTaskCompletion(logline);
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE_LENGTH];
    char name[MAX_NAME_LENGTH];
    char grade[MAX_GRADE_LENGTH];
    int nameIndex = 0;
    int bytesRead;
    char c;

    while ((bytesRead = read(file_desc, &c, 1)) > 0) {
        if (c == ',') {
            name[nameIndex] = '\0'; // Null-terminate the name
            if (strcmp(name, inputName) == 0) {
                // Student found, extract and print grade
                bytesRead = read(file_desc, grade, MAX_GRADE_LENGTH);
                if (bytesRead > 0) {
                    grade[bytesRead] = '\0'; // Null-terminate the grade
                    char studentMsg[MAX_LINE_LENGTH];
                    int msgLength = snprintf(studentMsg, MAX_LINE_LENGTH, "Student %s's grade: %s\n", name, grade);
                    write(STDOUT_FILENO, studentMsg, msgLength);
                    char logline[MAX_LINE_LENGTH];
                    strcpy(logline, "Student searched");
                    strncat(logline, studentMsg, msgLength);
                    logTaskCompletion(logline);
                    close(file_desc);
                    return;
                }
            }
            nameIndex = 0; // Reset name index for next name
        } else if (c == '\n') {
            // Reset name index and name buffer for the next line
            nameIndex = 0;
        } else {
            // Store character in the name buffer
            name[nameIndex++] = c;
        }
    }

    // If execution reaches here, student was not found
    char notFoundMsg[MAX_LINE_LENGTH];
    int notFoundMsgLength = snprintf(notFoundMsg, MAX_LINE_LENGTH, "Student %s not found.\n", inputName);
    write(STDOUT_FILENO, notFoundMsg, notFoundMsgLength);
    char logline[MAX_LINE_LENGTH];
                    strcpy(logline, "Student ");
                    strncat(logline, notFoundMsg, notFoundMsgLength);
                    strcat(logline, " not found. ");
                    logTaskCompletion(logline);
    close(file_desc);
}
void sortAll(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error: Unable to open file.\n");
        logTaskCompletion("Error: file not found.");
        exit(EXIT_FAILURE);
    }

    write(STDOUT_FILENO, "Write how it will be sorted.(grade or name)\n", strlen("Write how it will be sorted.(grade or name)\n"));
    char line[MAX_LINE_LENGTH];
    scanf("%s",line);
    if(strcmp(line, "grade")==0){
        // Sorting by grade implementation
        FILE *file = fopen(filename, "r+");
        if (file == NULL) {
            perror("Error opening file");
            exit(EXIT_FAILURE);
        }

        char lines[MAX_LINE_LENGTH+100][MAX_LINE_LENGTH+100];
        char temp[MAX_LINE_LENGTH+100];
        int count = 0;

        // Read all lines from the file
        while (fgets(lines[count], sizeof(lines[count]), file)) {
            count++;
        }

        // Perform sorting based on student grade
        for (int i = 0; i < count - 1; i++) {
            for (int j = 0; j < count - i - 1; j++) {
                char grade1[MAX_GRADE_LENGTH];
                char grade2[MAX_GRADE_LENGTH];
                sscanf(lines[j], "%*[^,], %[^\n]", grade1);
                sscanf(lines[j + 1], "%*[^,], %[^\n]", grade2);
                if (strcmp(grade1, grade2) > 0) {
                    strcpy(temp, lines[j]);
                    strcpy(lines[j], lines[j + 1]);
                    strcpy(lines[j + 1], temp);
                }
            }
        }

        for (int i = 0; i < count; i++) {
            printf("%d. %s",i+1,lines[i]);
        }
        logTaskCompletion("Sort All (by Grade)");
        fclose(file);
    }
    else if(strcmp(line, "name")==0){
        // Sorting by name implementation
        FILE *file = fopen(filename, "r+");
        if (file == NULL) {
            perror("Error opening file");
            exit(EXIT_FAILURE);
        }

        char lines[MAX_LINE_LENGTH+100][MAX_LINE_LENGTH+100];
        char temp[MAX_LINE_LENGTH+100];
        int count = 0;

        // Read all lines from the file
        while (fgets(lines[count], sizeof(lines[count]), file)) {
            count++;
        }

        // Perform sorting based on student name
        for (int i = 0; i < count - 1; i++) {
            for (int j = 0; j < count - i - 1; j++) {
                char name1[MAX_NAME_LENGTH];
                char name2[MAX_NAME_LENGTH];
                sscanf(lines[j], "%[^,],", name1);
                sscanf(lines[j + 1], "%[^,],", name2);
                if (strcmp(name1, name2) > 0) {
                    strcpy(temp, lines[j]);
                    strcpy(lines[j], lines[j + 1]);
                    strcpy(lines[j + 1], temp);
                }
            }
        }

        for (int i = 0; i < count; i++) {
            printf("%d. %s",i+1,lines[i]);
        }
        logTaskCompletion("Sort All (by Name)");
        fclose(file);
    }
    else{
        printf("Type error...");
        return;
    }

}

// Function to display all student grades in the file
void showAll(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error: Unable to open file.\n");
        logTaskCompletion("Error: file not found.");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file)) {
        printf("%s", line);
    }
    logTaskCompletion("Show All");
    fclose(file);

}

// Function to list first 5 student grades in the file
void listGrades(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error: Unable to open file.\n");
        logTaskCompletion("Error: file not found.");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE_LENGTH];
    for (int i = 0; i < 5; i++) {
        if (fgets(line, sizeof(line), file)) {
            printf("%s", line);
        } else {
            break;
        }
    }
    logTaskCompletion("List Grades (First 5)");
    fclose(file);
}

// Function to list a specific range of student grades in the file
void listSome(const char *filename, int numEntries, int pageNumber) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error: Unable to open file.\n");
        logTaskCompletion("Error: file not found.");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE_LENGTH];
    int start = (pageNumber - 1) * numEntries;
    int end = start + numEntries;
    for (int i = 0; i < end; i++) {
        if (fgets(line, sizeof(line), file)) {
            if (i >= start) {
                printf("%s", line);
            }
        } else {
            break;
        }
    }
    char cnumEntries = numEntries +'0';
    char cpageNumber = pageNumber+'0';
    
    char logline[MAX_LINE_LENGTH];
        strcpy(logline, "List Some (numEntries ");
        strncat(logline, &cnumEntries, 1);
        strcat(logline, ", pageNumber ");
        strncat(logline, &cpageNumber, 1);
        strcat(logline, ")");
    logTaskCompletion(logline);
    fclose(file);
}

// Function to print usage instructions
void printUsage() {
    write(STDOUT_FILENO, "Commands:\n", strlen("Commands:\n"));
    write(STDOUT_FILENO, "Usage: gtuStudentGrades <command>\n", strlen("Usage: gtuStudentGrades <command>\n"));
    write(STDOUT_FILENO, "  addStudentGrade \"Name Surname\" \"Grade\" \"grades.txt\"\n", strlen("  addStudentGrade \"Name Surname\" \"Grade\" \"grades.txt\"\n"));
    write(STDOUT_FILENO, "  searchStudent \"Name Surname\" \"grades.txt\"\n", strlen("  searchStudent \"Name Surname\" \"grades.txt\"\n"));
    write(STDOUT_FILENO, "  sortAll \"grades.txt\"\n", strlen("  sortAll \"grades.txt\"\n"));
    write(STDOUT_FILENO, "  showAll \"grades.txt\"\n", strlen("  showAll \"grades.txt\"\n"));
    write(STDOUT_FILENO, "  listGrades \"grades.txt\" \n", strlen("  listGrades \"grades.txt\" \n"));
    write(STDOUT_FILENO, "  listSome \"numofEntries\" \"pageNumber\" \"grades.txt\"\n", strlen("  listSome \"numofEntries\" \"pageNumber\" \"grades.txt\"\n"));
}

void logTaskCompletion(const char *taskName) {
    FILE *logFile = fopen("log.txt", "a");
    if (logFile == NULL) {
        perror("Error opening log file");
        exit(EXIT_FAILURE);
    }
    time_t currentTime;
    time(&currentTime);
    fprintf(logFile, "[%s] Task completed: %s\n", strtok(ctime(&currentTime), "\n"), taskName);
    fclose(logFile);
}


int readInput(char *buffer, size_t size) {
    int bytes_read = 0;
    char c;
    while (bytes_read < size - 1) {
        if (read(STDIN_FILENO, &c, 1) != 1) {
            perror("read");
            exit(EXIT_FAILURE);
        }
        if (c == '\n') {
            break; // Exit loop when newline is encountered
        }
        buffer[bytes_read++] = c;
    }
    buffer[bytes_read] = '\0'; // Terminate the buffer with null character
    return bytes_read;
}

char** split_command(char* str) {
    int num_tokens = 0;
    int in_quote = 0;

    char* ptr = str;
    while (*ptr) {
        if (*ptr == '"') {
            in_quote = !in_quote; 
        } else if (!in_quote && strchr(" ", *ptr)) {
            num_tokens++;
        }
        ptr++;
    }
    char** tokens;
    if (*ptr == '\0' && num_tokens == 0) {
        num_tokens++;
        tokens = malloc(2 * sizeof(char*));
        tokens[0] = malloc(strlen(str) + 1);
        strcpy(tokens[0], str);
        tokens[1] = NULL;
        return tokens;
    }
    tokens = malloc((num_tokens + 1) * sizeof(char*));
    if (!tokens) {
        return NULL; 
    }

    ptr = str;
    int token_index = 0;
    char* token_start = ptr;

    while (*ptr) {
        if (*ptr == '"') {
            in_quote = !in_quote;
        } else if (!in_quote && strchr(" ", *ptr)) {
            tokens[token_index] = malloc((ptr - token_start) + 1);
            if (!tokens[token_index]) {
                for (int j = 0; j < token_index; j++) {
                    free(tokens[j]);
                }
                free(tokens);
                return NULL;
            }
            strncpy(tokens[token_index], token_start, ptr - token_start);
            tokens[token_index][ptr - token_start] = '\0'; 

            token_index++;
            token_start = ptr + 1;
        }
        ptr++;
    }
    if (ptr > token_start) {
        tokens[token_index] = malloc(ptr - token_start + 1);
        if (!tokens[token_index]) {
            for (int j = 0; j < token_index; j++) {
                free(tokens[j]);
            }
            free(tokens);
            return NULL;
        }
        strncpy(tokens[token_index], token_start, ptr - token_start);
        tokens[token_index][ptr - token_start] = '\0';
        token_index++;
    }
    tokens[token_index] = NULL;
    for (int i = 0; i < token_index; i++) {
        if(tokens[i][0] == '"'){
            int string_len = strlen(tokens[i]);
            if (string_len > 1) {
                char *src = tokens[i] + 1;
                char *dst = tokens[i];
                while (*src && src < tokens[i] + string_len - 1) {
                    *dst++ = *src++;
                }
                *dst = '\0';
            }
        }
    }
    return tokens;
}

int main(int argc, char* argv[]) {
    char input[MAX_LINE_LENGTH];
    char filename[MAX_LINE_LENGTH];
    char *args[3]; // Maximum 3 arguments
    int numArgs;
    
    int bytes_read;
    int fd = open("grades.txt", O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    while (1) {
        write(STDOUT_FILENO, "Enter command: ", strlen("Enter command: "));

        bytes_read = readInput(input, sizeof(input));
        if (bytes_read == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        char** tokens = split_command(input);
        int numTokens = 0;
        while (tokens[numTokens] != NULL) {
            numTokens++;
        }

        for (int i = 0; i < numTokens; i++) {
            args[i] = tokens[i];
        }
        args[numTokens] = NULL;  // Ensure args array is terminated with NULL
        // Parse the command
        //printf("1%s 2%s 3%s 4%s",args[0],args[1],args[2],args[3]);
        if(strcmp(args[0],"\n")==0){
            printUsage();
        }
        else if (strcmp(args[0], "exit") == 0) {
            write(STDOUT_FILENO, "Exiting...\n", strlen("Exiting...\n"));
            break;
        } else if (strcmp(args[0], "gtuStudentGrades") == 0 ) {
            if (numTokens != 2) {
                write(STDOUT_FILENO, "Usage: gtuStudentGrades <filename>\n", strlen("Usage: gtuStudentGrades <filename>\n"));
                continue;
            }
            strcpy(filename, args[1]);
            int pid = fork();
            if (pid < 0) {
                perror("fork");
                return 1;
            } 
            else if (pid == 0) {
                // Child process
                FILE* file = fopen(filename, "w");
                if (file == NULL) {
                    perror("fopen");
                    return 1;
                }
                printf("File '%s' created successfully.\n", filename);
                fclose(file);
                logTaskCompletion("Create File");
                exit(0); // Child process exits
            } else {
                // Parent process
                int status;
                waitpid(pid, &status, 0); // Wait for child process to finish
                if (WIFEXITED(status)) {
                    printf("Child process exited with status %d.\n", WEXITSTATUS(status));
                } else {
                    printf("Child process terminated abnormally.\n");
                }
            }
        } else if (strcmp(args[0], "addStudentGrade") == 0 && numTokens == 4) {

            if (numTokens != 4) { // Check if numArgs is not equal to 3
                write(STDOUT_FILENO, "Usage: addStudentGrade <filename>\n", strlen("Usage: gtuStudentGrades <filename>\n"));
                continue;
            }
            strcpy(filename, args[3]);
            int pid = fork();
            if (pid < 0) {
                perror("fork");
                return 1;
            } else if (pid == 0) {
                // Child process
                addStudentGrade(filename, args[1], args[2]);
                exit(0); // Child process exits
            } else {
                // Parent process
                int status;
                waitpid(pid, &status, 0); // Wait for child process to finish
                if (WIFEXITED(status)) {
                    printf("Child process exited with status %d.\n", WEXITSTATUS(status));
                } else {
                    printf("Child process terminated abnormally.\n");
                }
            }
        } else if (strcmp(args[0], "searchStudent") == 0 && numTokens == 3) {
            // Ensure there are 2 arguments for searchStudent command
            // Usage: searchStudent <name> <filename>
            if (numTokens != 3) {
                write(STDOUT_FILENO, "Usage: searchStudent <name> <filename>\n", strlen("Usage: searchStudent <name> <filename>\n"));
                continue;
            }
            
            // Extract name and filename from args
            char* name = args[1];
            char* filename = args[2];
            
            // Create a child process to execute the searchStudent function
            int pid = fork();
            if (pid < 0) {
                perror("fork");
                return 1;
            } else if (pid == 0) {
                // Child process
                searchStudent(filename, name); // Note the parameter order: filename, name
                exit(0); // Child process exits
            } else {
                // Parent process
                int status;
                waitpid(pid, &status, 0); // Wait for child process to finish
                if (WIFEXITED(status)) {
                    printf("Child process exited with status %d.\n", WEXITSTATUS(status));
                } else {
                    printf("Child process terminated abnormally.\n");
                }
            }
        } else if (strcmp(args[0], "sortAll") == 0 ) {
            // Ensure there is 1 argument for sortAll command
            // Usage: sortAll <filename>
            if (numTokens != 2) {
                write(STDOUT_FILENO, "Usage: sortAll <filename>\n", strlen("Usage: sortAll <filename>\n"));
                continue;
            }
            
            // Extract filename from args
            strcpy(filename, args[1]);

            // Create a child process to execute the sortAll function
            int pid = fork();
            if (pid < 0) {
                perror("fork");
                return 1;
            } else if (pid == 0) {
                // Child process
                sortAll(filename);
                exit(0); // Child process exits
            } else {
                // Parent process
                int status;
                waitpid(pid, &status, 0); // Wait for child process to finish
                if (WIFEXITED(status)) {
                    printf("Child process exited with status %d.\n", WEXITSTATUS(status));
                } else {
                    printf("Child process terminated abnormally.\n");
                }
            }
        }else if (strcmp(args[0], "showAll") == 0 ) {
            // Ensure there is 1 argument for showAll command
            // Usage: showAll <filename>
            if (numTokens != 2) {
                write(STDOUT_FILENO, "Usage: showAll <filename>\n", strlen("Usage: showAll <filename>\n"));
                continue;
            }
            
            // Extract filename from args
            char* filename = args[1];

            // Create a child process to execute the showAll function
            int pid = fork();
            if (pid < 0) {
                perror("fork");
                return 1;
            } else if (pid == 0) {
                // Child process
                showAll(filename);
                exit(0); // Child process exits
            } else {
                // Parent process
                int status;
                waitpid(pid, &status, 0); // Wait for child process to finish
                if (WIFEXITED(status)) {
                    printf("Child process exited with status %d.\n", WEXITSTATUS(status));
                } else {
                    printf("Child process terminated abnormally.\n");
                }
            }
        }
        else if (strcmp(args[0], "listGrades") == 0 ) {
            // Ensure there is 1 argument for listGrades command
            // Usage: listGrades <filename>
            if (numTokens != 2) {
                write(STDOUT_FILENO, "Usage: listGrades <filename>\n", strlen("Usage: listGrades <filename>\n"));
                continue;
            }
            
            // Extract filename from args
            char* filename = args[1];

            // Create a child process to execute the listGrades function
            int pid = fork();
            if (pid < 0) {
                perror("fork");
                return 1;
            } else if (pid == 0) {
                // Child process
                listGrades(filename);
                exit(0); // Child process exits
            } else {
                // Parent process
                int status;
                waitpid(pid, &status, 0); // Wait for child process to finish
                if (WIFEXITED(status)) {
                    printf("Child process exited with status %d.\n", WEXITSTATUS(status));
                } else {
                    printf("Child process terminated abnormally.\n");
                }
            }
        }
        else if (strcmp(args[0], "listSome") == 0 ) {
            // Ensure there are 3 arguments for listSome command
            // Usage: listSome <numEntries> <pageNumber> <filename>
            if (numTokens != 4) {
                write(STDOUT_FILENO, "Usage: listSome <numEntries> <pageNumber> <filename>\n", strlen("Usage: listSome <numEntries> <pageNumber> <filename>\n"));
                continue;
            }
            
            // Extract numEntries, pageNumber, and filename from args
            int numEntries = atoi(args[1]);
            int pageNumber = atoi(args[2]);
            char* filename = args[3];

            // Create a child process to execute the listSome function
            int pid = fork();
            if (pid < 0) {
                perror("fork");
                return 1;
            } else if (pid == 0) {
                // Child process
                listSome(filename, numEntries, pageNumber);
                exit(0); // Child process exits
            } else {
                // Parent process
                int status;
                waitpid(pid, &status, 0); // Wait for child process to finish
                if (WIFEXITED(status)) {
                    printf("Child process exited with status %d.\n", WEXITSTATUS(status));
                } else {
                    printf("Child process terminated abnormally.\n");
                }
            }
        } else {
            printUsage();
        }
    }

    return 0;
}

