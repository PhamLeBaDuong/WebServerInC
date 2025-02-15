// #include <winsock2.h>   
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
// #include <ws2tcpip.h>
#include "squeue.h"

#define PORT 9909
#define BUFFER_SIZE 104857600
#define THREAD_POOL_SIZE 20

pthread_t thread_pool [THREAD_POOL_SIZE];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_var = PTHREAD_COND_INITIALIZER;

const char *get_file_extension(const char *file_name) {
    const char *dot = strrchr(file_name, '.');
    if (!dot || dot == file_name) {
        return "";
    }
    return dot + 1;
}

const char *get_mime_type(const char *file_ext) {
    if (strcasecmp(file_ext, "html") == 0 || strcasecmp(file_ext, "htm") == 0) {
        return "text/html";
    }
    else if(strcasecmp(file_ext, "txt") == 0) {
        return "text/plain";
    }
    else if(strcasecmp(file_ext, "jpg") == 0 || strcasecmp(file_ext, "jpeg") == 0) {
        return "image/jpeg";
    }
    else {
        return "application/octet-stream";
    }
}

bool case_insensitive_compare(const char *str1, const char *str2) {
    while (*str1 && *str2) {
        if(tolower((unsigned char)*str1) != tolower((unsigned char)*str2)) {
            return false;
        }
        str1++;
        str2++;
        return *str1 == *str2;
        
    }
}

char *get_file_case_insensitive(const char *file_name) {
    DIR *dir = opendir(".");
    if (dir == NULL) {
        perror("opendir");
        return NULL;
    }

    struct dirent *entry;
    char *found_file_name = NULL;
    while((entry = readdir(dir)) != NULL) {
        if (case_insensitive_compare(entry->d_name, file_name)) {
            found_file_name = entry->d_name;
            break;
        }
    }

    closedir(dir);
    return found_file_name;
}

char *url_decode(const char *src) {
    size_t src_len = strlen(src);
    char *decoded = malloc(src_len + 1);
    size_t decoded_len = 0;

    for(size_t i = 0; i < src_len; i++) {
        if(src[i] == '%' && i + 2 < src_len) {
            int hex_val;
            sscanf(src + i + 1, "%2x", &hex_val);
            decoded[decoded_len++] = hex_val;
            i += 2;
        } else {
            decoded[decoded_len++] = src[i];
        }
    }

    decoded[decoded_len] = '\0';
    return decoded;
}

void build_http_response(const char *file_name, const char *file_ext, char *response, size_t *response_len) {

    // If no such file, return response is 404 Not Found
    int file_fd = open(file_name, O_RDONLY);
    if (file_fd == -1) {
        snprintf(response, BUFFER_SIZE, "HTTP/1.1 404 Not Found\r\n""Content-Type: text/plain\r\n""\r\n""404 Not Found");
        *response_len = strlen(response);
        return;
    }

    //Build Header
    const char *mime_type = get_mime_type(file_ext);
    char *header = (char *)malloc(BUFFER_SIZE * sizeof(char));
    snprintf(header, BUFFER_SIZE, "HTTP/1.1 200 OK\r\n""Content-type: %s\r\n""\r\n",mime_type);


    // Get file size
    struct stat file_stat;
    fstat(file_fd, &file_stat);
    off_t file_size = file_stat.st_size;

    // Copy Built header to response buffer
    *response_len = 0;
    memcpy(response, header, strlen(header));
    *response_len += strlen(header);

    // Copy file content to response
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, response + *response_len, BUFFER_SIZE - *response_len)) > 0) {
        *response_len += bytes_read;
    }
    free(header);
    close(file_fd);
}

void *handle_client(void *arg) {
    int client_fd = *((int *)arg);
    char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));

    // receive request data from client and store into buffer
    ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
    if (bytes_received > 0) {
        // Check if request starts with "GET /"
        if (strncmp(buffer, "GET /", 5) == 0) {
            // Find the end of the URL (space before HTTP/1)
            char *space_pos = strchr(buffer + 5, ' ');
            if (space_pos && strncmp(space_pos + 1, "HTTP/1", 6) == 0) {
                // Extract filename from request
                size_t url_length = space_pos - (buffer + 5);
                char *url_encoded_file_name = malloc(url_length + 1);
                strncpy(url_encoded_file_name, buffer + 5, url_length);
                url_encoded_file_name[url_length] = '\0';

                // Decode URL
                char *file_name = url_decode(url_encoded_file_name);
                free(url_encoded_file_name);

                // Get file extension
                char file_ext[32];
                strcpy(file_ext, get_file_extension(file_name));

                // Build HTTP response
                char *response = (char *)malloc(BUFFER_SIZE * 2 * sizeof(char));
                size_t response_len;
                build_http_response(file_name, file_ext, response, &response_len);

                // Send HTTP response to client
                send(client_fd, response, response_len, 0);

                free(response);
                free(file_name);
            }
        }
        else if(strncmp(buffer, "POST /",6) == 0) {
            
        }
        else if(strncmp(buffer, "PUT /",5) == 0) {

        }
        else if(strncmp(buffer, "DELETE /",8) == 0) {

        }
    }

    close(client_fd);
    free(arg);
    free(buffer);
    return NULL;
}

void *thread_function(void *arg) {
    while (1) {
        int *pclient_fd = NULL;

        pthread_mutex_lock(&mutex);
        while((pclient_fd = dequeue()) == NULL) {
            pthread_cond_wait(&condition_var, &mutex);
        }
        pthread_mutex_unlock(&mutex);
        
        if(pclient_fd != NULL) {
           handle_client(pclient_fd);
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    //Initialize the WSA variables. Removed comment for Docker use
    // WSADATA ws;
    // if(WSAStartup(MAKEWORD(2,2), &ws) < 0) {
    //     perror("WSAStartup failed");
    //     exit(EXIT_FAILURE);
    // }
    
    int server_fd;
    struct sockaddr_in server_addr;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_create(&thread_pool[i], NULL, thread_function, NULL);
    }


    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int *client_fd = malloc(sizeof(int));

        if((*client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        //make sure only one thread messes with the queue at a time
        pthread_mutex_lock(&mutex);
        enqueue(client_fd);
        pthread_cond_signal(&condition_var);
        pthread_mutex_unlock(&mutex);

        // pthread_t thread_id;
        // pthread_create(&thread_id, NULL, handle_client, (void*)client_fd);
        // pthread_detach(thread_id);
    }

    close(server_fd);
    return 0;
}