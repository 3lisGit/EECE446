/*
 * peer.c
 * EECE 446 - Program 3
 * Fall 2025
 * Alexander Liu and Elijah Coleman
 * P2P Peer Application with File Download
 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>

#define MAX_FILENAME_LEN 100
#define MAX_FILES 100
#define MAX_BUFFER_SIZE 1200
#define FETCH_BUFFER_SIZE 4096
#define ACTION_JOIN 0
#define ACTION_PUBLISH 1
#define ACTION_SEARCH 2
#define ACTION_FETCH 3

// Global variables
int sockfd = -1;
uint32_t peer_id;

// Function prototypes
int connect_to_registry(const char *registry_host, const char *registry_port);
int send_join(void);
int send_publish(void);
int send_search(const char *filename, uint32_t *peer_id_out, uint32_t *peer_addr_out, uint16_t *peer_port_out);
int send_fetch(const char *filename);
void handle_exit(void);
int get_files_in_directory(char filenames[][MAX_FILENAME_LEN], int *count);
int connect_to_peer(uint32_t peer_addr, uint16_t peer_port);
int fetch_file_from_peer(int peer_sockfd, const char *filename, size_t *bytes_received_out);

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <registry_host> <registry_port> <peer_id>\n", 
                argv[0]);
        return 1;
    }

    const char *registry_host = argv[1];
    const char *registry_port = argv[2];
    peer_id = (uint32_t)atol(argv[3]);

    // Connect to registry
    if (connect_to_registry(registry_host, registry_port) < 0) {
        fprintf(stderr, "Failed to connect to registry\n");
        return 1;
    }

    // Main command loop
    char command[100];
    while (1) {
        printf("Enter a command: ");
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }

        // Remove newline
        command[strcspn(command, "\n")] = 0;

        if (strcmp(command, "JOIN") == 0) {
            if (send_join() < 0) {
                fprintf(stderr, "Failed to send JOIN\n");
            }
        } else if (strcmp(command, "PUBLISH") == 0) {
            if (send_publish() < 0) {
                fprintf(stderr, "Failed to send PUBLISH\n");
            }
        } else if (strcmp(command, "SEARCH") == 0) {
            char filename[MAX_FILENAME_LEN];
            printf("Enter a file name: ");
            if (fgets(filename, sizeof(filename), stdin) == NULL) {
                break;
            }
            filename[strcspn(filename, "\n")] = 0;
            
            uint32_t peer_id_resp, peer_addr;
            uint16_t peer_port;
            if (send_search(filename, &peer_id_resp, &peer_addr, &peer_port) < 0) {
                fprintf(stderr, "Failed to search\n");
            }
        } else if (strcmp(command, "FETCH") == 0) {
            char filename[MAX_FILENAME_LEN];
            printf("Enter a file name: ");
            if (fgets(filename, sizeof(filename), stdin) == NULL) {
                break;
            }
            filename[strcspn(filename, "\n")] = 0;
            
            if (send_fetch(filename) < 0) {
                fprintf(stderr, "Failed to fetch file\n");
            }
        } else if (strcmp(command, "EXIT") == 0) {
            handle_exit();
            break;
        } else {
            fprintf(stderr, "Unknown command: %s\n", command);
        }
    }

    return 0;
}

int connect_to_registry(const char *registry_host, const char *registry_port) {
    struct addrinfo hints, *res, *p;
    int status;

    // Initialize hints structure
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;
    hints.ai_addrlen = 0;
    hints.ai_addr = NULL;
    hints.ai_canonname = NULL;
    hints.ai_next = NULL;

    if ((status = getaddrinfo(registry_host, registry_port, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return -1;
    }

    // Try each address until we successfully connect
    for (p = res; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            sockfd = -1;
            continue;
        }

        break; // Successfully connected
    }

    freeaddrinfo(res);

    if (sockfd == -1) {
        fprintf(stderr, "Failed to connect\n");
        return -1;
    }

    return 0;
}

int send_join(void) {
    uint8_t buffer[5];
    buffer[0] = ACTION_JOIN;
    uint32_t peer_id_network = htonl(peer_id);
    memcpy(&buffer[1], &peer_id_network, 4);

    ssize_t sent = send(sockfd, buffer, 5, 0);
    if (sent < 0) {
        perror("send");
        return -1;
    }
    if (sent != 5) {
        fprintf(stderr, "Partial send in JOIN\n");
        return -1;
    }

    return 0;
}

int get_files_in_directory(char filenames[][MAX_FILENAME_LEN], int *count) {
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char filepath[512];

    dir = opendir("SharedFiles");
    if (dir == NULL) {
        perror("opendir");
        return -1;
    }

    *count = 0;
    while ((entry = readdir(dir)) != NULL && *count < MAX_FILES) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Build full path and check if it's a regular file
        snprintf(filepath, sizeof(filepath), "SharedFiles/%s", entry->d_name);
        if (stat(filepath, &statbuf) == 0 && S_ISREG(statbuf.st_mode)) {
            strncpy(filenames[*count], entry->d_name, MAX_FILENAME_LEN - 1);
            filenames[*count][MAX_FILENAME_LEN - 1] = '\0';
            (*count)++;
        }
    }

    closedir(dir);
    return 0;
}

int send_publish(void) {
    char filenames[MAX_FILES][MAX_FILENAME_LEN];
    int file_count = 0;

    if (get_files_in_directory(filenames, &file_count) < 0) {
        return -1;
    }

    // Build PUBLISH message
    uint8_t buffer[MAX_BUFFER_SIZE];
    int offset = 0;

    buffer[offset++] = ACTION_PUBLISH;

    uint32_t count_network = htonl((uint32_t)file_count);
    memcpy(&buffer[offset], &count_network, 4);
    offset += 4;

    // Add filenames
    for (int i = 0; i < file_count; i++) {
        int len = strlen(filenames[i]);
        memcpy(&buffer[offset], filenames[i], len);
        offset += len;
        buffer[offset++] = '\0'; // NULL terminator
    }

    ssize_t sent = send(sockfd, buffer, offset, 0);
    if (sent < 0) {
        perror("send");
        return -1;
    }
    if (sent != offset) {
        fprintf(stderr, "Partial send in PUBLISH\n");
        return -1;
    }

    return 0;
}

int send_search(const char *filename, uint32_t *peer_id_out, uint32_t *peer_addr_out, uint16_t *peer_port_out) {
    uint8_t buffer[MAX_BUFFER_SIZE];
    int offset = 0;

    buffer[offset++] = ACTION_SEARCH;

    int len = strlen(filename);
    memcpy(&buffer[offset], filename, len);
    offset += len;
    buffer[offset++] = '\0';

    ssize_t sent = send(sockfd, buffer, offset, 0);
    if (sent < 0) {
        perror("send");
        return -1;
    }
    if (sent != offset) {
        fprintf(stderr, "Partial send in SEARCH\n");
        return -1;
    }

    // Receive response
    uint8_t response[10]; // 4 + 4 + 2 = 10 bytes
    ssize_t received = 0;
    while (received < 10) {
        ssize_t r = recv(sockfd, response + received, 10 - received, 0);
        if (r < 0) {
            perror("recv");
            return -1;
        }
        if (r == 0) {
            fprintf(stderr, "Connection closed by registry\n");
            return -1;
        }
        received += r;
    }

    // Parse response
    uint32_t peer_id_resp;
    uint32_t peer_addr;
    uint16_t peer_port;

    memcpy(&peer_id_resp, &response[0], 4);
    memcpy(&peer_addr, &response[4], 4);
    memcpy(&peer_port, &response[8], 2);

    peer_id_resp = ntohl(peer_id_resp);
    peer_port = ntohs(peer_port);

    // Store output parameters
    if (peer_id_out) *peer_id_out = peer_id_resp;
    if (peer_addr_out) *peer_addr_out = peer_addr;
    if (peer_port_out) *peer_port_out = peer_port;

    if (peer_id_resp == 0 && peer_addr == 0 && peer_port == 0) {
        printf("File not indexed by registry\n");
    } else {
        char ip_str[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &peer_addr, ip_str, INET_ADDRSTRLEN) == NULL) {
            perror("inet_ntop");
            return -1;
        }
        printf("File found at\n");
        printf("Peer %u\n", peer_id_resp);
        printf("%s:%u\n", ip_str, peer_port);
    }

    return 0;
}

int connect_to_peer(uint32_t peer_addr, uint16_t peer_port) {
    int peer_sockfd;
    struct sockaddr_in peer_sockaddr;

    // Create socket
    peer_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (peer_sockfd < 0) {
        perror("socket");
        return -1;
    }

    // Setup peer address
    peer_sockaddr.sin_family = AF_INET;
    peer_sockaddr.sin_addr.s_addr = peer_addr; // Already in network byte order
    peer_sockaddr.sin_port = htons(peer_port);

    // Connect to peer
    if (connect(peer_sockfd, (struct sockaddr *)&peer_sockaddr, sizeof(peer_sockaddr)) < 0) {
        perror("connect to peer");
        close(peer_sockfd);
        return -1;
    }

    return peer_sockfd;
}

int fetch_file_from_peer(int peer_sockfd, const char *filename, size_t *bytes_received_out) {
    // Build FETCH request
    uint8_t request[MAX_BUFFER_SIZE];
    int offset = 0;

    request[offset++] = ACTION_FETCH;

    int len = strlen(filename);
    memcpy(&request[offset], filename, len);
    offset += len;
    request[offset++] = '\0';

    // Send FETCH request
    ssize_t sent = send(peer_sockfd, request, offset, 0);
    if (sent < 0) {
        perror("send fetch request");
        return -1;
    }
    if (sent != offset) {
        fprintf(stderr, "Partial send in FETCH request\n");
        return -1;
    }

    // Receive response code (1 byte)
    uint8_t response_code;
    ssize_t r = recv(peer_sockfd, &response_code, 1, 0);
    if (r < 0) {
        perror("recv response code");
        return -1;
    }
    if (r == 0) {
        fprintf(stderr, "Connection closed by peer\n");
        return -1;
    }

    // Check response code
    if (response_code != 0) {
        fprintf(stderr, "Peer returned error code: %u\n", response_code);
        return -1;
    }

    // Open file for writing
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        perror("fopen");
        return -1;
    }

    // Receive file data
    uint8_t buffer[FETCH_BUFFER_SIZE];
    size_t total_received = 0;
    
    while (1) {
        r = recv(peer_sockfd, buffer, FETCH_BUFFER_SIZE, 0);
        if (r < 0) {
            perror("recv file data");
            fclose(file);
            return -1;
        }
        if (r == 0) {
            // Connection closed, file transfer complete
            break;
        }

        // Write to file
        size_t written = fwrite(buffer, 1, r, file);
        if (written != (size_t)r) {
            perror("fwrite");
            fclose(file);
            return -1;
        }

        total_received += r;
    }

    fclose(file);
    
    // Store the total bytes received
    if (bytes_received_out) {
        *bytes_received_out = total_received;
    }

    return 0;
}

int send_fetch(const char *filename) {
    uint32_t peer_id_resp, peer_addr;
    uint16_t peer_port;

    // First, search for the file
    if (send_search(filename, &peer_id_resp, &peer_addr, &peer_port) < 0) {
        return -1;
    }

    // Check if file was found
    if (peer_id_resp == 0 && peer_addr == 0 && peer_port == 0) {
        // File not found (already printed by send_search)
        return -1;
    }

    // Connect to the peer
    int peer_sockfd = connect_to_peer(peer_addr, peer_port);
    if (peer_sockfd < 0) {
        fprintf(stderr, "Failed to connect to peer\n");
        return -1;
    }

    // Fetch the file
    size_t bytes_received = 0;
    if (fetch_file_from_peer(peer_sockfd, filename, &bytes_received) < 0) {
        fprintf(stderr, "Failed to fetch file from peer\n");
        close(peer_sockfd);
        return -1;
    }

    // Close peer connection
    close(peer_sockfd);

    // Print success message with file size
    printf("File '%s' successfully downloaded (%zu bytes)\n", filename, bytes_received);

    return 0;
}

void handle_exit(void) {
    if (sockfd >= 0) {
        close(sockfd);
    }
}