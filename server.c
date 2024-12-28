#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#define BUFFER_SIZE 1048576
#define PORT 5003
#define SHARED_FOLDER "/home/user/shared"
#define MAX_FILENAME_LEN 255

// Hàm gửi danh sách file tới client
void send_file_list(int client_socket, const char *base_path);

// Kiểm tra file trong thư mục chia sẻ
int file_exists(const char *filename);
// Hàm gửi file tới client
void send_file_to_client(int client_socket, const char *filename);

// Hàm tạo thư mục nếu chưa tồn tại
void create_directory_if_not_exists(const char *dir);
// Hàm nhận file từ client
void receive_file_from_client(int client_socket, const char *filename);

//Xử lý các yêu cầu từ client
void *handle_client(void *arg);
//Khởi tạo server
void start_server();

void send_file_list(int client_socket, const char *base_path) {
    DIR *d;
    struct dirent *dir;

    d = opendir(base_path);

    if (d) {
        char file_list[BUFFER_SIZE] = "";
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
                continue;
            }

            struct stat path_stat;
            char fullpath[BUFFER_SIZE];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", base_path, dir->d_name);
            stat(fullpath, &path_stat);

            if (S_ISDIR(path_stat.st_mode)) {
                strcat(file_list, "[DIR] ");
            } else if (S_ISREG(path_stat.st_mode)) { 
                strcat(file_list, "[FILE] ");
            }
            strcat(file_list, dir->d_name);
            strcat(file_list, "\n");
        }
        send(client_socket, file_list, strlen(file_list), 0);
        closedir(d);
    } else {
        char response[] = "Error reading directory.";
        send(client_socket, response, strlen(response), 0);
    }
}

int file_exists(const char *filename) {
    char filepath[BUFFER_SIZE];
    snprintf(filepath, sizeof(filepath), "%s/%s", SHARED_FOLDER, filename);
    FILE *file = fopen(filepath, "r");
    if (file) {
        fclose(file);
        return 1;
    }
    return 0;
}


void send_file_to_client(int client_socket, const char *filename) {
    char filepath[BUFFER_SIZE];
    snprintf(filepath, sizeof(filepath), "%s/%s", SHARED_FOLDER, filename);

    FILE *file = fopen(filepath, "rb");
    if (!file) {
        char response[] = "FILE NOT FOUND";
        send(client_socket, response, strlen(response), 0);
        return;
    } else {
        char response[] = "FILE AVAILABLE";
        send(client_socket, response, strlen(response), 0);
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }

    fclose(file);
}

void create_directory_if_not_exists(const char *dir) {
    struct stat st = {0};

    if (stat(dir, &st) == -1) {
        mkdir(dir, 0700);  
    }
}

void receive_file_from_client(int client_socket, const char *filename) {
    create_directory_if_not_exists(SHARED_FOLDER);

    char filepath[BUFFER_SIZE];
    snprintf(filepath, sizeof(filepath), "%s/%s", SHARED_FOLDER, filename);

    FILE *file = fopen(filepath, "wb");
    if (!file) {
        perror("Cannot create file");
        return;
    }

    char buffer_data[BUFFER_SIZE];
    ssize_t bytes_received;

    while ((bytes_received = recv(client_socket, buffer_data, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer_data, 1, bytes_received, file);

    }
    if (bytes_received < 0) {
        perror("Error while receiving file");
    } else {
        printf("File received and saved as: %s\n", filepath);
    }

    fclose(file);
}

void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    read(client_socket, buffer, sizeof(buffer));
    printf("Received request: %s\n", buffer);

    if (strncmp(buffer, "LIST", 4) == 0) {
        char directory[BUFFER_SIZE] = SHARED_FOLDER;
        if (strlen(buffer) > 5) {
            int it;
            char buffer_cut[1024];
            for (it = 5; it < strlen(buffer) - 1; ++it) {
                buffer_cut[it - 5] = buffer[it];
            }
            snprintf(directory, sizeof(directory), "%s/%s", SHARED_FOLDER, buffer_cut);
        }
        send_file_list(client_socket, directory);
    } 
    else if (strncmp(buffer, "REQUEST", 7) == 0) {
        char filename[BUFFER_SIZE];
        sscanf(buffer, "REQUEST %s", filename);

        if (file_exists(filename)) {
            send_file_to_client(client_socket, filename);
        } else {
            char response[] = "FILE NOT FOUND";
            send(client_socket, response, strlen(response), 0);
        }
    } 
    else if (strncmp(buffer, "UPLOAD", 6) == 0) {
        char filename[BUFFER_SIZE];
        sscanf(buffer, "UPLOAD %s", filename);

        receive_file_from_client(client_socket, filename);
        char rsp[] = "I'm here!";
        send(client_socket, rsp, strlen(rsp), 0);
    } 
    else {
        char response[] = "INVALID REQUEST";
        send(client_socket, response, strlen(response), 0);
    }

    close(client_socket);
    return NULL;
}


void start_server() {
    int server_fd, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    while (1) {
        client_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];  
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        printf("Client connected from IP: %s, Port: %d\n", client_ip, ntohs(client_addr.sin_port));

        int *client_sock = (int *)malloc(sizeof(int));
        *client_sock = client_socket;

        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, client_sock);
        pthread_detach(thread);
    }

    close(server_fd);
}

int main() {
    start_server();
    return 0;
}
