#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/stat.h>  
#include <sys/types.h>
#include <dirent.h>

#define BUFFER_SIZE 1048576
#define MAX_FILENAME_LEN 255  
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 5003
#define RECEIVED_FOLDER "/home/user/received"
#define CLIENT_FOLDER "/home/user/received"
#define SCANF_FILENAME "%255s"

// Hàm gửi yêu cầu tới server để nhận danh sách file
void request_file_list(char *dir);
// Gửi yêu cầu list
void send_list_request(int sock, const char *path);
// Hàm nhận danh sách file và thư mục từ server
void receive_file_list(int sock);
//Xử lý yêu cầu liệt kê file
void list_choice();
// Hàm tạo thư mục nếu chưa tồn tại

// Hàm gửi yêu cầu tải file từ server
void request_file_from_server();
// Hàm nhận file từ server
void receive_file_from_server(int sock, const char *filename);
//tạo thư mục
void create_directory_if_not_exists(const char *dir);

//Tải file lên server
void show_and_upload_file_to_server();

void request_file_list(char *dir) {
    int sock;
    struct sockaddr_in server_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(sock);
        return;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return;
    }
    send_list_request(sock, dir);
    receive_file_list(sock);

    close(sock);
}

void receive_file_list(int sock) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
    if (bytes_received > 0) {
        printf("\nFiles and folders:\n%s\n", buffer);
    } else {
        if (bytes_received == 0) {
            printf("\nConnection closed by server.\n");
        } else {
            perror("Error receiving file list");
        }
    }
}

void list_choice() {
    int choice;
    char string_choice[1024];
    getchar();
    printf("Enter 1 to show root folder or enter path folder:");
    fgets(string_choice, 1024, stdin);
    if (isdigit(string_choice[0])) {
        choice = (int) string_choice[0] - 48;
        int it;
        char string_choice_cut[1024] = "";
        for (it = 2; it < strlen(string_choice); ++it) {
            string_choice_cut[it - 2] = string_choice[it];
        }
        request_file_list(string_choice_cut);
    }
}


void send_list_request(int sock, const char *path) {
    char request[BUFFER_SIZE];
    if (path) {
        snprintf(request, sizeof(request), "LIST %s", path);
    } else {
        snprintf(request, sizeof(request), "LIST");
    }
    send(sock, request, strlen(request), 0);
}

void request_file_from_server() {
    char filename[BUFFER_SIZE];
    printf("\nEnter filename: ");
    scanf("%s", filename);
    const char *filename_ptr = filename;
    int sock;
    struct sockaddr_in server_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(sock);
        return;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return;
    }

    char request[BUFFER_SIZE];
    snprintf(request, sizeof(request), "REQUEST %s", filename_ptr);
    write(sock, request, strlen(request));
    printf("Request sent: %s\n", request);

    char response[BUFFER_SIZE];
    memset(response, 0, BUFFER_SIZE);
    read(sock, response, sizeof(response));
    printf("Response from server: %s\n", response);

    if (strncmp(response, "FILE NOT FOUND", 14) == 0) {
        printf("Error: File not found on server.\n");
        close(sock);
        return;
    }

    receive_file_from_server(sock, filename_ptr);
    close(sock);
}

void create_directory_if_not_exists(const char *dir) {
    struct stat st = {0};
    if (stat(dir, &st) == -1) {
        mkdir(dir, 0700);  
    }
}

void receive_file_from_server(int sock, const char *filename) {
    create_directory_if_not_exists(RECEIVED_FOLDER);

    const char *basename = strrchr(filename, '/');
    if (basename) {
        basename++;
    } else {
        basename = filename; 
    }

    char filepath[BUFFER_SIZE];
    snprintf(filepath, sizeof(filepath), "%s/%s", RECEIVED_FOLDER, basename);

    FILE *file = fopen(filepath, "wb");
    if (!file) {
        perror("Cannot create file");
        return;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;

    while ((bytes_received = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes_received, file);
    }

    if (bytes_received < 0) {
        perror("Error while receiving file");
    } else {
        printf("File received and saved as: %s\n", filepath);
    }

    fclose(file);
}


void show_and_upload_file_to_server() {

    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Error creating socket");
        exit(1);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error connecting to server");
        close(client_socket);
        exit(1);
    }
    DIR *d;
    struct dirent *dir_entry;

    d = opendir(CLIENT_FOLDER);
    if (d) {
        printf("Các file trong thư mục '%s':\n", CLIENT_FOLDER);
        while ((dir_entry = readdir(d)) != NULL) {
            #ifdef DT_REG
                if (dir_entry->d_type == DT_REG) {
                    printf("%s\n", dir_entry->d_name);
                }
            #else
                struct stat file_stat;
                char filepath[BUFFER_SIZE];
                snprintf(filepath, sizeof(filepath), "%s/%s", CLIENT_FOLDER, dir_entry->d_name);
                if (stat(filepath, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
                    printf("%s\n", dir_entry->d_name);
                }
            #endif
        }
        closedir(d);
    } else {
        perror("Không thể mở thư mục");
        return;
    }

    char filename[MAX_FILENAME_LEN + 1];
    printf("\nNhập tên file để upload (tối đa %d kí tự): ", MAX_FILENAME_LEN);
    scanf(SCANF_FILENAME, filename);

    char filepath[BUFFER_SIZE];
    snprintf(filepath, sizeof(filepath), "%s/%s", CLIENT_FOLDER, filename);

    FILE *file = fopen(filepath, "rb");
    if (!file) {
        perror("Không tìm thấy file");
        return;
    }

    char upload_msg[BUFFER_SIZE];
    snprintf(upload_msg, sizeof(upload_msg), "UPLOAD %s", filename);
    if (send(client_socket, upload_msg, strlen(upload_msg), 0) < 0) {
        perror("Lỗi khi gửi thông báo UPLOAD");
        fclose(file);
        return;
    }
    
    printf("Đã gửi lệnh UPLOAD '%s' đến server.\n", filename);

    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (send(client_socket, buffer, bytes_read, 0) < 0) {
            perror("Lỗi khi gửi dữ liệu file");
            fclose(file);
            return;
        }
    }

    fclose(file);

    char end_msg[] = "FILE UPLOAD COMPLETE";
    if (send(client_socket, end_msg, strlen(end_msg), 0) < 0) {
        perror("Lỗi khi gửi thông báo hoàn tất upload");
    } else {
        printf("File đã được upload thành công.\n");
    }
    close(client_socket);
}


void start_client() {
    int choice;

    do {
        printf("\n1. List available files and folder");
        printf("\n2. Request file");
        printf("\n3. Upload file");
        printf("\n4. Exit");
        printf("\nChoose an option: ");
        scanf("%d",&choice);

        switch (choice) {
                case 1:{
                    list_choice();
                    break;}
                case 2: {
                    request_file_from_server();
                    break;}
                case 3: {
                    show_and_upload_file_to_server();
                    break;
                }
                case 4:
                    printf("\nExiting...\n");
                    break;
                default:
                    printf("\nInvalid choice!\n");
                    break;
            }
            
    } while (choice < 4);
}

int main() {
    create_directory_if_not_exists(CLIENT_FOLDER);
    start_client();
    return 0;
}