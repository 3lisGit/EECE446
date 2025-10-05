/*
 * EECE 446 - Program 2: Peer-to-Peer Introduction
 * Fall 2025
 * 
 * Group Members:
 * - Alexander Liu
 * - Elijah Coleman
 * 
 * Description: P2P peer application that communicates with a registry
 * to JOIN, PUBLISH files, and SEARCH for files.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <dirent.h>

/* Protocol action codes */
#define ACTION_JOIN    0
#define ACTION_PUBLISH 1
#define ACTION_SEARCH  2

/* Buffer sizes */
#define MAX_FILENAME_LEN 100
#define MAX_PUBLISH_SIZE 1200
#define COMMAND_BUFFER_SIZE 256

/* Function prototypes */
int connect_to_registry(const char *hostname, const char *port);
void handle_join(int sockfd, uint32_t peer_id);
void handle_publish(int sockfd);
void handle_search(int sockfd);
void handle_exit(int sockfd);

int main(int argc, char *argv[]) {
    /* Check command line arguments */
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <registry_host> <registry_port> <peer_id>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *registry_host = argv[1];
    const char *registry_port = argv[2];
    uint32_t peer_id = (uint32_t)atoi(argv[3]);

    /* Connect to registry */
    int sockfd = connect_to_registry(registry_host, registry_port);
    if (sockfd < 0) {
        fprintf(stderr, "Failed to connect to registry\n");
        exit(EXIT_FAILURE);
    }

    /* Main command loop */
    char command[COMMAND_BUFFER_SIZE];
    while (1) {
        printf("Enter a command: ");
        fflush(stdout);

        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }

        /* Remove newline */
        command[strcspn(command, "\n")] = '\0';

        /* Process commands */
        if (strcmp(command, "JOIN") == 0) {
            handle_join(sockfd, peer_id);
        } 
        else if (strcmp(command, "PUBLISH") == 0) {
            handle_publish(sockfd);
        } 
        else if (strcmp(command, "SEARCH") == 0) {
            handle_search(sockfd);
        } 
        else if (strcmp(command, "EXIT") == 0) {
            handle_exit(sockfd);
            break;
        } 
        else {
            fprintf(stderr, "Unknown command: %s\n", command);
        }
    }

    return 0;
}

/**
 * Connect to the registry server
 * Returns socket file descriptor on success, -1 on failure
 */
int connect_to_registry(const char *hostname, const char *port) {
    struct addrinfo hints, *res, *p;
    int sockfd;
    
    /* Initialize hints structure to zero */
    memset(&hints, 0, sizeof(hints));  // This memset is OK - it's initializing a struct
    hints.ai_family = AF_INET;         // Use IPv4
    hints.ai_socktype = SOCK_STREAM;   // TCP socket

    /* Initialize hints structure */
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    /* Resolve hostname */
    int status = getaddrinfo(hostname, port, &hints, &res);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return -1;
    }

    /* Try each address until successful connection */
    for (p = res; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd < 0) {
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0) {
            break; /* Success */
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

/**
 * Handle JOIN command
 * Sends JOIN request to registry
 */
void handle_join(int sockfd, uint32_t peer_id) {
    uint8_t buffer[5];
    
    /* Build JOIN message */
    buffer[0] = ACTION_JOIN;
    uint32_t peer_id_network = htonl(peer_id);
    memcpy(&buffer[1], &peer_id_network, sizeof(peer_id_network));

    /* Send JOIN request in single send call */
    ssize_t sent = send(sockfd, buffer, sizeof(buffer), 0);
    if (sent < 0) {
        perror("send failed");
        return;
    }

    /* Handle partial send if needed */
    if (sent < sizeof(buffer)) {
        /* TODO: Handle partial send */
    }

    /* No response expected for JOIN */
}

/**
 * Handle PUBLISH command
 * Reads files from SharedFiles directory and sends to registry
 */
void handle_publish(int sockfd) {
    DIR *dir;
    struct dirent *entry;
    uint8_t buffer[MAX_PUBLISH_SIZE];
    uint32_t file_count = 0;
    size_t offset = 5; /* Skip action byte and count field initially */

    /* Open SharedFiles directory */
    dir = opendir("SharedFiles");
    if (dir == NULL) {
        perror("opendir failed");
        return;
    }

    /* Read directory and build file list */
    while ((entry = readdir(dir)) != NULL) {
        /* Skip directories */
        if (entry->d_type == DT_DIR) {
            continue;
        }

        /* Check buffer space */
        size_t filename_len = strlen(entry->d_name) + 1; /* +1 for NULL */
        if (offset + filename_len > MAX_PUBLISH_SIZE) {
            fprintf(stderr, "PUBLISH buffer full\n");
            break;
        }

        /* Copy filename with NULL terminator */
        strcpy((char *)&buffer[offset], entry->d_name);
        offset += filename_len;
        file_count++;
    }

    closedir(dir);

    /* Build PUBLISH message header */
    buffer[0] = ACTION_PUBLISH;
    uint32_t count_network = htonl(file_count);
    memcpy(&buffer[1], &count_network, sizeof(count_network));

    /* Send PUBLISH request in single send call */
    ssize_t sent = send(sockfd, buffer, offset, 0);
    if (sent < 0) {
        perror("send failed");
        return;
    }

    /* Handle partial send if needed */
    if ((size_t)sent < offset) {
        /* TODO: Handle partial send */
    }

    /* No response expected for PUBLISH */
}

/**
 * Handle SEARCH command
 * Prompts for filename, sends request, and prints response
 */
void handle_search(int sockfd) {
    char filename[MAX_FILENAME_LEN];
    uint8_t send_buffer[1 + MAX_FILENAME_LEN];
    uint8_t recv_buffer[10]; /* 4 + 4 + 2 bytes */

    /* Prompt for filename */
    printf("Enter a file name: ");
    fflush(stdout);

    if (fgets(filename, sizeof(filename), stdin) == NULL) {
        return;
    }

    /* Remove newline */
    filename[strcspn(filename, "\n")] = '\0';

    /* Build SEARCH request */
    send_buffer[0] = ACTION_SEARCH;
    strcpy((char *)&send_buffer[1], filename);
    size_t msg_len = 1 + strlen(filename) + 1; /* action + filename + NULL */

    /* Send SEARCH request */
    ssize_t sent = send(sockfd, send_buffer, msg_len, 0);
    if (sent < 0) {
        perror("send failed");
        return;
    }

    /* Receive SEARCH response */
    ssize_t received = recv(sockfd, recv_buffer, sizeof(recv_buffer), 0);
    if (received < 0) {
        perror("recv failed");
        return;
    }

    if (received != 10) {
        fprintf(stderr, "Invalid response size: %zd\n", received);
        return;
    }

    /* Parse response */
    uint32_t peer_id_network, peer_ip_network;
    uint16_t peer_port_network;

    memcpy(&peer_id_network, &recv_buffer[0], 4);
    memcpy(&peer_ip_network, &recv_buffer[4], 4);
    memcpy(&peer_port_network, &recv_buffer[8], 2);

    uint32_t peer_id = ntohl(peer_id_network);
    uint16_t peer_port = ntohs(peer_port_network);

    /* Check if file was found (all zeros means not found) */
    if (peer_id == 0 && peer_ip_network == 0 && peer_port == 0) {
        printf("File not indexed by registry\n");
    } else {
        /* Convert IP address to string */
        char ip_str[INET_ADDRSTRLEN];
        struct in_addr addr;
        addr.s_addr = peer_ip_network;
        
        if (inet_ntop(AF_INET, &addr, ip_str, INET_ADDRSTRLEN) == NULL) {
            perror("inet_ntop failed");
            return;
        }

        /* Print result */
        printf("File found at\n");
        printf("Peer %u\n", peer_id);
        printf("%s:%u\n", ip_str, peer_port);
    }
}

/**
 * Handle EXIT command
 * Closes socket and exits program
 */
void handle_exit(int sockfd) {
    close(sockfd);
}