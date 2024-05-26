#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/time.h>

#define PATH_MAX 4096
#define BUFFER_SIZE (1024 * 4)  // 4 KB buffer size

// Buffer structure
typedef struct {
    int src_fd;
    int dest_fd;
    char src_name[PATH_MAX];
    char dest_name[PATH_MAX];
} FileData;

typedef struct {
    FileData* data;
    int size;
    int start;
    int end;
    int count;
} Buffer;

// Global variables
Buffer buffer;
int done = 0;
int files_copied = 0;
int dirs_created = 0;
int fifo_files_copied = 0;
long total_bytes_copied = 0;
pthread_mutex_t buffer_mutex;
pthread_cond_t buffer_cond;
pthread_cond_t buffer_not_full;
pthread_cond_t buffer_not_empty;
pthread_mutex_t stats_mutex;

// Function prototypes
void buffer_init(Buffer* buffer, int size);
int buffer_is_empty(Buffer* buffer);
int buffer_is_full(Buffer* buffer);
void buffer_write(Buffer* buffer, FileData file_data);
FileData buffer_read(Buffer* buffer);
void buffer_destroy(Buffer* buffer);
void handle_signal(int sig);
void copy_file(int src_fd, int dest_fd, long* bytes_copied);
void traverse_directory(const char* src_dir, const char* dest_dir);
void* manager_thread(void* args);
void* worker_thread(void* args);

// Function implementations
void buffer_init(Buffer* buffer, int size) {
    buffer->data = (FileData*)malloc(sizeof(FileData) * size);
    buffer->size = size;
    buffer->start = 0;
    buffer->end = 0;
    buffer->count = 0;
}

int buffer_is_empty(Buffer* buffer) {
    return buffer->count == 0;
}

int buffer_is_full(Buffer* buffer) {
    return buffer->count == buffer->size;
}

void buffer_write(Buffer* buffer, FileData file_data) {
    if (buffer_is_full(buffer)) {
        return;
    }
    buffer->data[buffer->end] = file_data;
    buffer->end = (buffer->end + 1) % buffer->size;
    buffer->count++;
}

FileData buffer_read(Buffer* buffer) {
    FileData file_data = buffer->data[buffer->start];
    buffer->start = (buffer->start + 1) % buffer->size;
    buffer->count--;
    return file_data;
}

void buffer_destroy(Buffer* buffer) {
    free(buffer->data);
}

void handle_signal(int sig) {
    done = 1;
    pthread_cond_broadcast(&buffer_cond);
    pthread_cond_broadcast(&buffer_not_full);
    pthread_cond_broadcast(&buffer_not_empty);
}

void copy_file(int src_fd, int dest_fd, long* bytes_copied) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read, bytes_written;
    long total_bytes = 0;

    while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0) {
        bytes_written = write(dest_fd, buffer, bytes_read);
        if (bytes_written == -1) {
            perror("write");
            break;
        }
        total_bytes += bytes_written;
    }

    if (bytes_read == -1) {
        perror("read");
    }

    *bytes_copied = total_bytes;
}

void traverse_directory(const char* src_dir, const char* dest_dir) {
    DIR* src_dp = opendir(src_dir);
    if (src_dp == NULL) {
        perror("opendir src_dir");
        return;
    }

    struct dirent* entry;
    struct stat path_stat;
    while ((entry = readdir(src_dp)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char src_path[PATH_MAX];
        char dest_path[PATH_MAX];

        snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, entry->d_name);
        snprintf(dest_path, sizeof(dest_path), "%s/%s", dest_dir, entry->d_name);

        if (stat(src_path, &path_stat) == -1) {
            perror("stat");
            continue;
        }

        if (S_ISDIR(path_stat.st_mode)) {
            if (mkdir(dest_path, 0755) == -1 && errno != EEXIST) {
                perror("mkdir dest_dir");
                continue;
            }

            pthread_mutex_lock(&stats_mutex);
            dirs_created++;
            pthread_mutex_unlock(&stats_mutex);

            traverse_directory(src_path, dest_path);
        } else if (S_ISREG(path_stat.st_mode)) {
            int src_fd = open(src_path, O_RDONLY);
            if (src_fd == -1) {
                perror("open src_fd");
                continue;
            }

            int dest_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (dest_fd == -1) {
                perror("open dest_fd");
                close(src_fd);
                continue;
            }

            FileData file_data;
            file_data.src_fd = src_fd;
            file_data.dest_fd = dest_fd;
            snprintf(file_data.src_name, sizeof(file_data.src_name), "%s", src_path);
            snprintf(file_data.dest_name, sizeof(file_data.dest_name), "%s", dest_path);

            pthread_mutex_lock(&buffer_mutex);
            while (buffer_is_full(&buffer) && !done) {
                pthread_cond_wait(&buffer_not_full, &buffer_mutex);
            }

            if (done) {
                pthread_mutex_unlock(&buffer_mutex);
                close(src_fd);
                close(dest_fd);
                break;
            }

            buffer_write(&buffer, file_data);
            pthread_cond_signal(&buffer_not_empty);
            pthread_mutex_unlock(&buffer_mutex);
        } else if (S_ISFIFO(path_stat.st_mode)) {
            pthread_mutex_lock(&stats_mutex);
            fifo_files_copied++;
            pthread_mutex_unlock(&stats_mutex);
        }
    }

    closedir(src_dp);
}

void* manager_thread(void* args) {
    char* src_dir = ((char**)args)[0];
    char* dest_dir = ((char**)args)[1];

    struct stat st = {0};
    if (stat(dest_dir, &st) == -1) {
        if (mkdir(dest_dir, 0755) == -1) {
            perror("mkdir dest_dir");
            done = 1;
            pthread_cond_broadcast(&buffer_cond);
            pthread_cond_broadcast(&buffer_not_full);
            pthread_cond_broadcast(&buffer_not_empty);
            return NULL;
        }
    }

    traverse_directory(src_dir, dest_dir);

    pthread_mutex_lock(&buffer_mutex);
    done = 1;
    pthread_cond_broadcast(&buffer_cond);
    pthread_cond_broadcast(&buffer_not_empty);
    pthread_mutex_unlock(&buffer_mutex);

    return NULL;
}

void* worker_thread(void* args) {
    while (1) {
        pthread_mutex_lock(&buffer_mutex);

        while (buffer_is_empty(&buffer) && !done) {
            pthread_cond_wait(&buffer_not_empty, &buffer_mutex);
        }

        if (done && buffer_is_empty(&buffer)) {
            pthread_mutex_unlock(&buffer_mutex);
            break;
        }

        if (!buffer_is_empty(&buffer)) {
            FileData file_data = buffer_read(&buffer);
            pthread_cond_signal(&buffer_not_full);
            pthread_mutex_unlock(&buffer_mutex);

            long bytes_copied = 0;
            copy_file(file_data.src_fd, file_data.dest_fd, &bytes_copied);

            pthread_mutex_lock(&stats_mutex);
            //printf("File copied: %s to %s\n", file_data.src_name, file_data.dest_name);
            files_copied++;
            total_bytes_copied += bytes_copied;
            pthread_mutex_unlock(&stats_mutex);

            close(file_data.src_fd);
            close(file_data.dest_fd);
        } else {
            pthread_mutex_unlock(&buffer_mutex);
        }
    }

    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        printf("Usage: %s <buffer_size> <num_workers> <src_dir> <dest_dir>\n", argv[0]);
        return 1;
    }

    int buffer_size = atoi(argv[1]);
    int num_workers = atoi(argv[2]);
    char* src_dir = argv[3];
    char* dest_dir = argv[4];

    buffer_init(&buffer, buffer_size);
    pthread_mutex_init(&buffer_mutex, NULL);
    pthread_cond_init(&buffer_cond, NULL);
    pthread_cond_init(&buffer_not_full, NULL);
    pthread_cond_init(&buffer_not_empty, NULL);
    pthread_mutex_init(&stats_mutex, NULL);

    signal(SIGINT, handle_signal);
    signal(SIGTSTP, handle_signal);  // Add this line to handle SIGTSTP

    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);

    pthread_t manager;
    char* dirs[] = { src_dir, dest_dir };
    pthread_create(&manager, NULL, manager_thread, dirs);

    pthread_t workers[num_workers];
    for (int i = 0; i < num_workers; ++i) {
        pthread_create(&workers[i], NULL, worker_thread, NULL);
    }

    pthread_join(manager, NULL);
    for (int i = 0; i < num_workers; ++i) {
        pthread_join(workers[i], NULL);
    }

    pthread_mutex_destroy(&buffer_mutex);
    pthread_cond_destroy(&buffer_cond);
    pthread_cond_destroy(&buffer_not_full);
    pthread_cond_destroy(&buffer_not_empty);
    pthread_mutex_destroy(&stats_mutex);
    buffer_destroy(&buffer);

    gettimeofday(&end_time, NULL);
    long seconds = end_time.tv_sec - start_time.tv_sec;
    long microseconds = end_time.tv_usec - start_time.tv_usec;
    double elapsed = seconds + microseconds * 1e-6;

    long minutes = seconds / 60;
    seconds %= 60;
    long milliseconds = microseconds / 1000;

    printf("\n---------------STATISTICS--------------------\n");
    printf("Consumers: %d - Buffer Size: %d\n", num_workers, buffer_size);
    printf("Number of Regular Files: %d\n", files_copied);
    printf("Number of FIFO Files: %d\n", fifo_files_copied);
    printf("Number of Directories: %d\n", dirs_created);
    printf("TOTAL BYTES COPIED: %ld\n", total_bytes_copied);
    printf("TOTAL TIME: %02ld:%02ld.%03ld (min:sec.mili)\n", minutes, seconds, milliseconds);

    return 0;
}
