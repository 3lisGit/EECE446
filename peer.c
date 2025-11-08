/*
 * peer.c
 * EECE 446 - Program 3
 * Fall 2025
 * Alexander Liu & Elijah Coleman
 * 
 * P2P Peer Application with FETCH capability
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
int send_search(const char *filename);
int send_fetch(const char *filename);
void handle_exit(void);
int get_files_in_directory(char filenames[][MAX_FILENAME_LEN], int *count);

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <registry_host> <registry_port> <peer_id>\n", argv[0]);
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
            
            if (send_search(filename) < 0) {
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
                fprintf(stderr, "Failed to fetch\n");
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
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

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
    char filepath[256];

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

int send_search(const char *filename) {
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

int send_fetch(const char *filename) {
    // Step 1: Send SEARCH to registry to find which peer has the file
    uint8_t search_buffer[MAX_BUFFER_SIZE];
    int offset = 0;

    search_buffer[offset++] = ACTION_SEARCH;
    int len = strlen(filename);
    memcpy(&search_buffer[offset], filename, len);
    offset += len;
    search_buffer[offset++] = '\0';

    ssize_t sent = send(sockfd, search_buffer, offset, 0);
    if (sent < 0) {
        perror("send search in fetch");
        return -1;
    }
    if (sent != offset) {
        fprintf(stderr, "Partial send in SEARCH within FETCH\n");
        return -1;
    }

    // Step 2: Receive SEARCH response from registry
    uint8_t response[10];
    ssize_t received = 0;
    while (received < 10) {
        ssize_t r = recv(sockfd, response + received, 10 - received, 0);
        if (r < 0) {
            perror("recv search response");
            return -1;
        }
        if (r == 0) {
            fprintf(stderr, "Connection closed by registry during FETCH\n");
            return -1;
        }
        received += r;
    }

    // Step 3: Parse peer information
    uint32_t peer_id_resp;
    uint32_t peer_addr;
    uint16_t peer_port;

    memcpy(&peer_id_resp, &response[0], 4);
    memcpy(&peer_addr, &response[4], 4);
    memcpy(&peer_port, &response[8], 2);

    peer_id_resp = ntohl(peer_id_resp);
    peer_port = ntohs(peer_port);

    // Check if file was found
    if (peer_id_resp == 0 && peer_addr == 0 && peer_port == 0) {
        printf("File not indexed by registry\n");
        return -1;
    }

    // Step 4: Connect to the peer that has the file
    char ip_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &peer_addr, ip_str, INET_ADDRSTRLEN) == NULL) {
        perror("inet_ntop");
        return -1;
    }

    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%u", peer_port);

    struct addrinfo hints, *res, *p;
    int status;
    int peer_sockfd = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(ip_str, port_str, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo error for peer: %s\n", gai_strerror(status));
        return -1;
    }

    // Connect to peer
    for (p = res; p != NULL; p = p->ai_next) {
        peer_sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (peer_sockfd == -1) {
            continue;
        }

        if (connect(peer_sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(peer_sockfd);
            peer_sockfd = -1;
            continue;
        }

        break;
    }

    freeaddrinfo(res);

    if (peer_sockfd == -1) {
        fprintf(stderr, "Failed to connect to peer\n");
        return -1;
    }

    // Step 5: Send FETCH request to peer
    uint8_t fetch_buffer[MAX_BUFFER_SIZE];
    offset = 0;

    fetch_buffer[offset++] = ACTION_FETCH;
    len = strlen(filename);
    memcpy(&fetch_buffer[offset], filename, len);
    offset += len;
    fetch_buffer[offset++] = '\0';

    sent = send(peer_sockfd, fetch_buffer, offset, 0);
    if (sent < 0) {
        perror("send fetch request");
        close(peer_sockfd);
        return -1;
    }
    if (sent != offset) {
        fprintf(stderr, "Partial send in FETCH request\n");
        close(peer_sockfd);
        return -1;
    }

    // Step 6: Receive FETCH response (1 byte response code)
    uint8_t response_code;
    ssize_t r = recv(peer_sockfd, &response_code, 1, 0);
    if (r < 0) {
        perror("recv fetch response code");
        close(peer_sockfd);
        return -1;
    }
    if (r == 0) {
        fprintf(stderr, "Peer closed connection before sending response\n");
        close(peer_sockfd);
        return -1;
    }

    // Check response code
    if (response_code != 0) {
        fprintf(stderr, "Peer returned error code: %u\n", response_code);
        close(peer_sockfd);
        return -1;
    }

    // Step 7: Open file for writing
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        perror("fopen");
        close(peer_sockfd);
        return -1;
    }

    // Step 8: Receive file data until peer closes connection
    uint8_t file_buffer[4096];
    size_t total_bytes = 0;

    while (1) {
        r = recv(peer_sockfd, file_buffer, sizeof(file_buffer), 0);
        if (r < 0) {
            perror("recv file data");
            fclose(file);
            close(peer_sockfd);
            return -1;
        }
        if (r == 0) {
            // Peer closed connection - file transfer complete
            break;
        }

        // Write received data to file
        size_t written = fwrite(file_buffer, 1, r, file);
        if (written != (size_t)r) {
            fprintf(stderr, "Failed to write all data to file\n");
            fclose(file);
            close(peer_sockfd);
            return -1;
        }

        total_bytes += written;
    }

    // Step 9: Clean up
    fclose(file);
    close(peer_sockfd);

    printf("File downloaded successfully (%zu bytes)\n", total_bytes);

    return 0;
}

void handle_exit(void) {
    if (sockfd >= 0) {
        close(sockfd);
    }
}
