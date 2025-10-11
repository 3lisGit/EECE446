// peer.c
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <dirent.h>

// Protocol constants
#define ACTION_JOIN    0
#define ACTION_PUBLISH 1
#define ACTION_SEARCH  2

#define JOIN_MSG_SIZE     5
#define SEARCH_RESP_SIZE  10

// Function declarations
int connect_to_registry(const char *host, const char *port);
int send_join_request(int sockfd, uint32_t peer_id);
int send_publish_request(int sockfd);

// Connect to registry
int connect_to_registry(const char *host, const char *port) {
    struct addrinfo hints, *res;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo(host, port, &hints, &res) != 0) {
        perror("getaddrinfo");
        return -1;
    }
    
    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        perror("socket");
        freeaddrinfo(res);
        return -1;
    }
    
    if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        close(sockfd);
        freeaddrinfo(res);
        return -1;
    }
    
    freeaddrinfo(res);
    return sockfd;
}

// Send JOIN request
int send_join_request(int sockfd, uint32_t peer_id) {
    uint8_t buffer[JOIN_MSG_SIZE];
    buffer[0] = ACTION_JOIN;
    uint32_t network_peer_id = htonl(peer_id);
    memcpy(buffer + 1, &network_peer_id, sizeof(network_peer_id));

    ssize_t bytes_sent = send(sockfd, buffer, sizeof(buffer), 0);
    if (bytes_sent < 0) {
        perror("send");
        return -1;
    }
    
    if (bytes_sent != JOIN_MSG_SIZE) {
        fprintf(stderr, "Warning: partial send (%zd of %d bytes)\n", bytes_sent, JOIN_MSG_SIZE);
        return -1;
    }
    
    printf("Sent JOIN request for peer ID %u\n", peer_id);
    return 0;
}

// Send PUBLISH request
int send_publish_request(int sockfd) {
    DIR *dir = opendir("SharedFiles");
    if (dir == NULL) {
        perror("opendir");
        return -1;
    }
    
    // First pass - count files and calculate total message size
    int file_count = 0;
    size_t total_filename_length = 0;
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            file_count++;
            total_filename_length += strlen(entry->d_name) + 1; // +1 for null terminator
        }
    }
    
    // Calculate total message size: [1B Action][4B Count][filenames with nulls]
    size_t message_size = 1 + 4 + total_filename_length;
    
    // Allocate buffer
    uint8_t *buffer = malloc(message_size);
    if (buffer == NULL) {
        perror("malloc");
        closedir(dir);
        return -1;
    }
    
    // Build the message
    buffer[0] = ACTION_PUBLISH;
    uint32_t network_count = htonl(file_count);
    memcpy(&buffer[1], &network_count, sizeof(network_count));
    
    // Reset directory to beginning
    rewinddir(dir);
    
    // Second pass - copy filenames into buffer
    size_t offset = 5; // Start after action(1) + count(4)
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            char *filename = entry->d_name;
            size_t len = strlen(filename) + 1; // +1 for '\0'
            memcpy(&buffer[offset], filename, len);
            offset += len;
        }
    }
    
    closedir(dir);
    
    // Send the entire message
    ssize_t bytes_sent = send(sockfd, buffer, message_size, 0);
    
    if (bytes_sent < 0) {
        perror("send");
        free(buffer);
        return -1;
    }
    
    if (bytes_sent != (ssize_t)message_size) {
        fprintf(stderr, "Warning: partial send (%zd of %zu bytes)\n", bytes_sent, message_size);
        free(buffer);
        return -1;
    }
    
    printf("Sent PUBLISH request with %d file(s)\n", file_count);
    free(buffer);
    return 0;
}

// Main function
int main(int argc, char *argv[]) {
    // Check arguments
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <registry_host> <registry_port> <peer_id>\n", argv[0]);
        return 1;
    }
    
    char *registry_host = argv[1];
    char *registry_port = argv[2];
    uint32_t peer_id = (uint32_t)atoi(argv[3]);
    
    // Connect to registry
    int sockfd = connect_to_registry(registry_host, registry_port);
    if (sockfd < 0) {
        fprintf(stderr, "Failed to connect to registry\n");
        return 1;
    }
    
    printf("Connected to registry\n");
    
    // Command loop
    char command[100];
    while (1) {
        printf("Enter a command: ");
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }
        command[strcspn(command, "\n")] = 0; // Remove newline
        
        if (strcmp(command, "JOIN") == 0) {
            send_join_request(sockfd, peer_id);
        }
        else if (strcmp(command, "PUBLISH") == 0) {
            send_publish_request(sockfd);
        }
        else if (strcmp(command, "SEARCH") == 0) {
        }
        else if (strcmp(command, "EXIT") == 0) {
            printf("Exiting...\n");
            break;
        }
        else {
            printf("Invalid command. Please enter JOIN, PUBLISH, SEARCH, or EXIT.\n");
        }
    }
    
    close(sockfd);
    return 0;
}
