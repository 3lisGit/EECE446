/*
 * EECE 446 - Program 2: Peer-to-Peer Introduction
 * Fall 2025
 * 
 * Group Members:
 * - [Your Name]
 * - [Partner's Name]
 */

// ============================================
// CORRECT #INCLUDES (macOS and Linux)
// ============================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <dirent.h>

// ============================================
// PROTOCOL DEFINITIONS
// ============================================
#define ACTION_JOIN    0
#define ACTION_PUBLISH 1
#define ACTION_SEARCH  2

#define JOIN_MSG_SIZE     5
#define SEARCH_RESP_SIZE  10

// ============================================
// FUNCTION PROTOTYPES
// ============================================
int connect_to_registry(const char *hostname, const char *port);
int send_join_request(int sockfd, uint32_t peer_id);
int send_publish_request(int sockfd);
int send_search_request(int sockfd);

// ============================================
// CONNECT TO REGISTRY
// ============================================
int connect_to_registry(const char *hostname, const char *port) {
    struct addrinfo hints, *res, *p;
    int sockfd;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(hostname, port, &hints, &res);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return -1;
    }

    for (p = res; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd < 0) {
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }

        close(sockfd);
    }

    freeaddrinfo(res);

    if (p == NULL) {
        fprintf(stderr, "Failed to connect to registry\n");
        return -1;
    }

    return sockfd;
}

// ============================================
// SEND JOIN REQUEST
// ============================================
int send_join_request(int sockfd, uint32_t peer_id) {
    uint8_t buffer[JOIN_MSG_SIZE];
    buffer[0] = ACTION_JOIN;
    uint32_t network_peer_id = htonl(peer_id);
    memcpy(buffer + 1, &network_peer_id, sizeof(network_peer_id)); // FIXED: was net_peer_id

    ssize_t bytes_sent = send(sockfd, buffer, sizeof(buffer), 0);
    if (bytes_sent < 0) {
        perror("send");
        return -1;
    }
    printf("Sent JOIN request for peer ID %u\n", peer_id);
    return 0;
}

// ============================================
// SEND PUBLISH REQUEST
// ============================================
int send_publish_request(int sockfd) {
    DIR *dir = opendir("SharedFiles"); // FIXED: was "shared", should be "SharedFiles"
    if (dir == NULL) { // FIXED: added semicolon
        perror("opendir");
        return -1;
    }

    // First pass: count files and calculate size
    int file_count = 0;
    size_t total_filename_length = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            file_count++;
            total_filename_length += strlen(entry->d_name) + 1; 
        }
    }
    
    // Reset directory for second pass
    rewinddir(dir);
    
    size_t message_size = 1 + 4 + total_filename_length;
    uint8_t *buffer = malloc(message_size);
    if (buffer == NULL) {
        closedir(dir);
        return -1; // FIXED: added semicolon
    }
    
    buffer[0] = ACTION_PUBLISH;
    uint32_t network_count = htonl(file_count); // FIXED: was unit32_t (typo)
    memcpy(&buffer[1], &network_count, sizeof(network_count));
    size_t offset = 5;

    // Second pass: copy filenames
    while ((entry = readdir(dir)) != NULL) { // FIXED: was NUll (typo)
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
        perror("send");
        free(buffer);
        return -1;
    } 

    if ((size_t)bytes_sent != message_size) {
        fprintf(stderr, "Partial send: %zd/%zu bytes\n", bytes_sent, message_size);
        free(buffer);
        return -1;
    }

    printf("Sent PUBLISH request with %d files\n", file_count);
    free(buffer);
    return 0;
}

// ============================================
// SEND SEARCH REQUEST
// ============================================
int send_search_request(int sockfd) {
    char filename[100];
    printf("Enter a file name: ");
    fflush(stdout);
    
    if (fgets(filename, sizeof(filename), stdin) == NULL) {
        return -1;
    }
    
    filename[strcspn(filename, "\n")] = '\0';
    
    // Build SEARCH request
    size_t msg_len = 1 + strlen(filename) + 1;
    uint8_t *buffer = malloc(msg_len);
    if (buffer == NULL) {
        return -1;
    }
    
    buffer[0] = ACTION_SEARCH;
    strcpy((char *)&buffer[1], filename);
    
    // Send request
    ssize_t sent = send(sockfd, buffer, msg_len, 0);
    free(buffer);
    
    if (sent < 0) {
        perror("send");
        return -1;
    }
    
    // Receive response
    uint8_t recv_buffer[SEARCH_RESP_SIZE];
    ssize_t received = recv(sockfd, recv_buffer, SEARCH_RESP_SIZE, 0);
    
    if (received < 0) {
        perror("recv");
        return -1;
    }
    
    if (received != SEARCH_RESP_SIZE) {
        fprintf(stderr, "Invalid response size: %zd\n", received);
        return -1;
    }
    
    // Parse response
    uint32_t peer_id, peer_ip;
    uint16_t peer_port;
    
    memcpy(&peer_id, &recv_buffer[0], 4);
    memcpy(&peer_ip, &recv_buffer[4], 4);
    memcpy(&peer_port, &recv_buffer[8], 2);
    
    peer_id = ntohl(peer_id);
    peer_port = ntohs(peer_port);
    
    // Check if file found
    if (peer_id == 0 && peer_ip == 0 && peer_port == 0) {
        printf("File not indexed by registry\n");
    } else {
        char ip_str[INET_ADDRSTRLEN];
        struct in_addr addr;
        addr.s_addr = peer_ip;
        
        if (inet_ntop(AF_INET, &addr, ip_str, INET_ADDRSTRLEN) == NULL) {
            perror("inet_ntop");
            return -1;
        }
        
        printf("File found at\n");
        printf("Peer %u\n", peer_id);
        printf("%s:%u\n", ip_str, peer_port);
    }
    
    return 0;
}

// ============================================
// MAIN
// ============================================
int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <registry_host> <registry_port> <peer_id>\n", argv[0]);
        return 1;
    }

    char *registry_host = argv[1];
    char *registry_port = argv[2];
    uint32_t peer_id = (uint32_t)atoi(argv[3]);

    int sockfd = connect_to_registry(registry_host, registry_port); // FIXED: syntax
    if (sockfd < 0) {
        fprintf(stderr, "Failed to connect to server\n");
        return 1; // FIXED: was retrrn (typo)
    }

    char command[100];
    while (1) {
        printf("Enter a command: ");
        fflush(stdout);
        
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }
        
        command[strcspn(command, "\n")] = '\0'; 

        if (strcmp(command, "JOIN") == 0) { // Changed to strcmp for exact match
            send_join_request(sockfd, peer_id);
        }
        else if (strcmp(command, "PUBLISH") == 0) {
            send_publish_request(sockfd); // FIXED: removed peer_id parameter
        }
        else if (strcmp(command, "SEARCH") == 0) {
            send_search_request(sockfd); // FIXED: removed peer_id parameter
        }
        else if (strcmp(command, "EXIT") == 0) {
            printf("Exiting...\n");
            break;
        }
        else {
            fprintf(stderr, "Unknown command: %s\n", command);
        }
    }
    
    close(sockfd);
    return 0;
}