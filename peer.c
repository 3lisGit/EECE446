#include <network.h>
#include <protocol.h>
#include <util.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>      
#include <string.h>      
#include <arpa/inet.h>   
#include <dirent.h>      
#include <stdlib.h>      
#include <stdio.h> 

#define ACTION_JOIN    0
#define ACTION_PUBLISH 1
#define ACTION_SEARCH  2

#define JOIN_MSG_SIZE     5
#define SEARCH_RESP_SIZE  10

int send_join_request(int sockfd, uint32_t peer_id) {
    uint8_t buffer[JOIN_MSG_SIZE];
    buffer[0] = ACTION_JOIN;
    uint32_t network_peer_id = htonl(peer_id);
    memcpy(buffer + 1, &network_peer_id, sizeof(net_peer_id));

    ssize_t bytes_sent = send(sockfd, buffer, sizeof(buffer), 0);
    if (bytes_sent < 0) {
        perror("send");
        return -1;
    }
    printf("Sent JOIN request for peer ID %u\n", peer_id);
    return 0;
}

int send_publish_request(int sockfd){
    DIR *dir = opendir("shared")
    if (dir == NULL){
        perror("opendir");
        return -1;
    }

    int file_count = 0;
    size_t total_filename_length = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL){
        if (entry->d_type == DT_REG){
            file_count++;
            total_filename_length += strlen(entry->d_name) + 1; 
        }
    }
    size_t message_size = 1 + 4 + total_filename_length;
    uint8_t *buffer = malloc(message_size);
    if (buffer == NULL){
        closedir(dir);
        return -1
    }
    buffer[0] = ACTION_PUBLISH;
    unit32_t network_count = htonl(file_count);
    memcpy(&buffer[1], &network_count, sizeof(network_count));
    size_t offset = 5;

    while ((entry = readdir(dir)) != NUll) {
        if (entry->d_type == DT_REG) {
            char *filename = entry->d_name;
            size_t len = strlen(filename) + 1;
            memcpy(buffer + offset, filename, len);

            offset += len;
        }
    }

    closedir(dir);

    ssize_t bytes_sent = send(sockfd, buffer, message_size, 0);

    if (bytes_sent < 0) {
        free(buffer);
        return -1;
    } 

    if (bytes_sent != message_size) {
        free(buffer);
        return -1;
    }

    free(buffer);

    return 0;
}

// OPTIONAL: Function to count files and total filename length in a directory
int count_files_in_directory(const char *dir_path, int *count, size_t *total_length) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        perror("opendir");
        return -1;
    }

    struct dirent *entry;
    *count = 0;
    *total_length = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) { 
            (*count)++;
            *total_length += strlen(entry->d_name) + 1; 
        }
    }

    closedir(dir);
    return 0;
}

int main(int argc, char *argv[]){

    char *registry_host = argv[1];
    char *registry_port = argv[2];
    uint32_t peer_id = atoi(argv[3]);

    int sockfd =connect_to_registry(registry_host, registry_port);{
        if (sockfd < 0) 
        fprintf(stderr, "Failed to connect to server\n");
        retrrn 1;


    }

    char command[100];
    while (1){
        printf("Enter Command:");
        fgets(command, sizeof(command), stdin);
        command[strcspn(command, "\n")] = 0; 

        if (strncmp(command, "JOIN") == 0 ){
            send_join_request(sockfd, peer_id);
        }
        else if (strncmp(command, "PUBLISH") == 0){
            send_publish_request(sockfd, peer_id);
        }
        else if (strncmp(command, "SEARCH") == 0){
            send_search_request(sockfd, peer_id);
        }
        else if (strncmp(command, "EXIT") == 0){
            printf("Exiting...\n");
            break;
        }
        else {
            printf("Invalid command. Please enter join, publish, search, or exit.\n");
        }
    }
        close(sockfd);
        return 0;
}
