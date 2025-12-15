/*
 * EECE 446 Program 4 - P2P Registry
 * Fall 2025
 * Authors: Alexander Liu, Elijah Coleman
 * 
 * This program implements a P2P registry that tracks peers and their files.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdint.h>

#define MAX_PEERS 5
#define MAX_FILES 10
#define MAX_FILENAME_LEN 101  // 100 chars + null terminator

/* Peer entry structure to track connected peers */
struct peer_entry {
    uint32_t id;                              // Peer ID (host byte order for internal use)
    int socket_descriptor;                    // Socket for this peer
    char files[MAX_FILES][MAX_FILENAME_LEN]; // Files published by peer
    int file_count;                           // Number of files published
    struct sockaddr_in address;               // IP address and port
    int is_active;                            // 1 if slot is in use, 0 if empty
};

/* Global array to track all peers */
struct peer_entry peers[MAX_PEERS];

/* Function prototypes */
int create_listen_socket(const char *port);
int accept_new_peer(int listen_sock);
int handle_join_message(int peer_sock, int peer_index);
int handle_publish_message(int peer_sock, int peer_index);
int handle_search_message(int peer_sock, int peer_index);
void setup_fd_sets(int listen_sock, fd_set *readfds, int *max_fd);
void initialize_peers(void);
int find_peer_by_socket(int sock);
int find_empty_peer_slot(void);
void cleanup_peer(int peer_index);

/* Initialize the peer tracking array */
void initialize_peers(void) {
    int i, j;
    for (i = 0; i < MAX_PEERS; i++) {
        peers[i].id = 0;
        peers[i].socket_descriptor = -1;
        peers[i].file_count = 0;
        peers[i].is_active = 0;
        
        for (j = 0; j < MAX_FILES; j++) {
            peers[i].files[j][0] = '\0';
        }
        
        /* Initialize address structure */
        peers[i].address.sin_family = AF_INET;
        peers[i].address.sin_port = 0;
        peers[i].address.sin_addr.s_addr = 0;
    }
}

/* Find peer index by socket descriptor */
int find_peer_by_socket(int sock) {
    int i;
    for (i = 0; i < MAX_PEERS; i++) {
        if (peers[i].is_active && peers[i].socket_descriptor == sock) {
            return i;
        }
    }
    return -1;
}

/* Find an empty slot in the peer array */
int find_empty_peer_slot(void) {
    int i;
    for (i = 0; i < MAX_PEERS; i++) {
        if (!peers[i].is_active) {
            return i;
        }
    }
    return -1;
}

/* Create and bind the listening socket */
int create_listen_socket(const char *port) {
    struct addrinfo hints, *servinfo, *p;
    int listen_sock;
    int yes = 1;
    int rv;
    
    /* Set up hints for getaddrinfo */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;      // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;  // TCP
    hints.ai_flags = AI_PASSIVE;      // Use my IP
    
    /* Get address info */
    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }
    
    /* Loop through results and bind to the first we can */
    for (p = servinfo; p != NULL; p = p->ai_next) {
        /* Create socket */
        if ((listen_sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }
        
        /* Set socket option to reuse address */
        if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            close(listen_sock);
            freeaddrinfo(servinfo);
            return -1;
        }
        
        /* Bind socket */
        if (bind(listen_sock, p->ai_addr, p->ai_addrlen) == -1) {
            close(listen_sock);
            perror("bind");
            continue;
        }
        
        break;
    }
    
    freeaddrinfo(servinfo);
    
    if (p == NULL) {
        fprintf(stderr, "Failed to bind socket\n");
        return -1;
    }
    
    /* Start listening */
    if (listen(listen_sock, 5) == -1) {
        perror("listen");
        close(listen_sock);
        return -1;
    }
    
    return listen_sock;
}

/* Accept a new peer connection */
int accept_new_peer(int listen_sock) {
    struct sockaddr_in peer_addr;
    socklen_t addr_len = sizeof(peer_addr);
    int new_sock;
    int peer_index;
    
    /* Accept the connection */
    new_sock = accept(listen_sock, (struct sockaddr *)&peer_addr, &addr_len);
    if (new_sock == -1) {
        perror("accept");
        return -1;
    }
    
    /* Find an empty slot for this peer */
    peer_index = find_empty_peer_slot();
    if (peer_index == -1) {
        fprintf(stderr, "No space for new peer\n");
        close(new_sock);
        return -1;
    }
    
    /* Store socket descriptor and mark as active but not yet joined */
    peers[peer_index].socket_descriptor = new_sock;
    peers[peer_index].is_active = 1;
    peers[peer_index].id = 0;  // Will be set when JOIN is received
    
    return new_sock;
}

/* Handle JOIN message from a peer */
int handle_join_message(int peer_sock, int peer_index) {
    uint32_t peer_id_net;
    uint32_t peer_id_host;
    ssize_t bytes_received;
    struct sockaddr_in peer_addr;
    socklen_t addr_len = sizeof(peer_addr);
    
    /* Receive the peer ID (4 bytes in network byte order) */
    bytes_received = recv(peer_sock, &peer_id_net, sizeof(peer_id_net), 0);
    if (bytes_received != sizeof(peer_id_net)) {
        if (bytes_received == 0) {
            fprintf(stderr, "Peer disconnected during JOIN\n");
        } else {
            perror("recv JOIN");
        }
        return -1;
    }
    
    /* Convert to host byte order for internal storage */
    peer_id_host = ntohl(peer_id_net);
    
    /* Store peer ID */
    peers[peer_index].id = peer_id_host;
    
    /* Get peer's address information using getpeername */
    if (getpeername(peer_sock, (struct sockaddr *)&peer_addr, &addr_len) == -1) {
        perror("getpeername");
        return -1;
    }
    
    /* Store the address (IP and port for SEARCH responses) */
    peers[peer_index].address = peer_addr;
    
    /* Print summary message: TEST] JOIN <id> */
    printf("TEST] JOIN %u\n", peer_id_host);
    fflush(stdout);
    
    return 0;
}

/* Handle PUBLISH message from a peer */
int handle_publish_message(int peer_sock, int peer_index) {
    uint32_t file_count_net;
    uint32_t file_count_host;
    ssize_t bytes_received;
    int i;
    char filename[MAX_FILENAME_LEN];
    int filename_len;
    
    /* Receive file count (4 bytes in network byte order) */
    bytes_received = recv(peer_sock, &file_count_net, sizeof(file_count_net), 0);
    if (bytes_received != sizeof(file_count_net)) {
        fprintf(stderr, "recv PUBLISH file count failed, got %zd bytes\n", bytes_received);
        perror("recv PUBLISH file count");
        return -1;
    }
    
    file_count_host = ntohl(file_count_net);
    
    #ifdef DEBUG
    fprintf(stderr, "DEBUG: Received file count: %u (0x%08x)\n", file_count_host, file_count_host);
    #endif
    
    /* Validate file count */
    if (file_count_host > MAX_FILES) {
        fprintf(stderr, "Too many files in PUBLISH: %u\n", file_count_host);
        return -1;
    }
    
    /* Store file count */
    peers[peer_index].file_count = file_count_host;
    
    /* Print start of summary message */
    printf("TEST] PUBLISH %u", file_count_host);
    
    /* Receive each filename (NULL-terminated) */
    for (i = 0; i < (int)file_count_host; i++) {
        /* Read filename one byte at a time until NULL terminator */
        filename_len = 0;
        while (filename_len < MAX_FILENAME_LEN - 1) {
            bytes_received = recv(peer_sock, &filename[filename_len], 1, 0);
            if (bytes_received != 1) {
                if (bytes_received == 0) {
                    fprintf(stderr, "Connection closed while reading filename %d\n", i);
                } else {
                    perror("recv PUBLISH filename byte");
                }
                return -1;
            }
            
            /* Check for NULL terminator */
            if (filename[filename_len] == '\0') {
                break;
            }
            
            filename_len++;
        }
        
        /* Ensure NULL termination */
        filename[filename_len] = '\0';
        
        /* Validate we got a filename */
        if (filename_len == 0) {
            fprintf(stderr, "Empty filename in PUBLISH\n");
            return -1;
        }
        
        /* Check if filename is too long (didn't find NULL within limit) */
        if (filename_len >= MAX_FILENAME_LEN - 1 && filename[filename_len - 1] != '\0') {
            fprintf(stderr, "Filename too long in PUBLISH (> %d chars)\n", MAX_FILENAME_LEN);
            return -1;
        }
        
        /* Store the filename */
        strncpy(peers[peer_index].files[i], filename, MAX_FILENAME_LEN - 1);
        peers[peer_index].files[i][MAX_FILENAME_LEN - 1] = '\0';
        
        /* Print filename in summary */
        printf(" %s", filename);
    }
    
    printf("\n");
    fflush(stdout);
    
    return 0;
}

/* Handle SEARCH message from a peer */
int handle_search_message(int peer_sock, int peer_index) {
    char filename[MAX_FILENAME_LEN];
    ssize_t bytes_received;
    int i, j;
    int found_peer_index = -1;
    char ip_str[INET_ADDRSTRLEN];
    int filename_len = 0;
    
    /* Read filename one byte at a time until NULL terminator */
    while (filename_len < MAX_FILENAME_LEN - 1) {
        bytes_received = recv(peer_sock, &filename[filename_len], 1, 0);
        if (bytes_received != 1) {
            if (bytes_received == 0) {
                fprintf(stderr, "Peer disconnected during SEARCH\n");
            } else {
                perror("recv SEARCH filename byte");
            }
            return -1;
        }
        
        /* Check for NULL terminator */
        if (filename[filename_len] == '\0') {
            break;
        }
        
        filename_len++;
    }
    
    /* Ensure NULL termination */
    filename[filename_len] = '\0';
    
    /* Validate we got a filename */
    if (filename_len == 0) {
        fprintf(stderr, "Empty filename in SEARCH\n");
        return -1;
    }
    
    /* Check if filename is too long */
    if (filename_len >= MAX_FILENAME_LEN - 1 && filename[filename_len - 1] != '\0') {
        fprintf(stderr, "SEARCH filename too long (> %d chars)\n", MAX_FILENAME_LEN);
        return -1;
    }
    
    /* Search through all peers for matching file */
    for (i = 0; i < MAX_PEERS; i++) {
        if (!peers[i].is_active || peers[i].id == 0) {
            continue;  /* Skip inactive or non-joined peers */
        }
        
        for (j = 0; j < peers[i].file_count; j++) {
            if (strcmp(peers[i].files[j], filename) == 0) {
                found_peer_index = i;
                break;
            }
        }
        
        if (found_peer_index != -1) {
            break;  /* Found a match, stop searching */
        }
    }
    
    /* Prepare response data */
    uint32_t response_id;
    uint32_t response_ip;
    uint16_t response_port;
    
    if (found_peer_index != -1) {
        /* File found - use peer's information */
        response_id = peers[found_peer_index].id;
        response_ip = peers[found_peer_index].address.sin_addr.s_addr;
        response_port = peers[found_peer_index].address.sin_port;
        
        /* Convert IP to string for printing */
        if (inet_ntop(AF_INET, &peers[found_peer_index].address.sin_addr, 
                      ip_str, sizeof(ip_str)) == NULL) {
            perror("inet_ntop");
            strcpy(ip_str, "?.?.?.?");
        }
        
        /* Print summary message: TEST] SEARCH <filename> <id> <ip>:<port> */
        printf("TEST] SEARCH %s %u %s:%u\n", 
               filename, response_id, ip_str, ntohs(response_port));
    } else {
        /* File not found - use zeros */
        response_id = 0;
        response_ip = 0;
        response_port = 0;
        
        /* Print summary message with zeros */
        printf("TEST] SEARCH %s 0 0.0.0.0:0\n", filename);
    }
    fflush(stdout);
    
    /* Send response back to requesting peer */
    /* Response format: 4 bytes ID + 4 bytes IP + 2 bytes port (all network byte order) */
    uint32_t id_net = htonl(response_id);
    
    /* Send peer ID */
    if (send(peer_sock, &id_net, sizeof(id_net), 0) == -1) {
        perror("send SEARCH response ID");
        return -1;
    }
    
    /* Send peer IP address (already in network byte order from sin_addr) */
    if (send(peer_sock, &response_ip, sizeof(response_ip), 0) == -1) {
        perror("send SEARCH response IP");
        return -1;
    }
    
    /* Send peer port (already in network byte order from sin_port) */
    if (send(peer_sock, &response_port, sizeof(response_port), 0) == -1) {
        perror("send SEARCH response port");
        return -1;
    }
    
    return 0;
}

/* Set up file descriptor sets for select() */
void setup_fd_sets(int listen_sock, fd_set *readfds, int *max_fd) {
    int i;
    
    /* Clear the set */
    FD_ZERO(readfds);
    
    /* Add listening socket */
    FD_SET(listen_sock, readfds);
    *max_fd = listen_sock;
    
    /* Add all active peer sockets */
    for (i = 0; i < MAX_PEERS; i++) {
        if (peers[i].is_active && peers[i].socket_descriptor != -1) {
            FD_SET(peers[i].socket_descriptor, readfds);
            if (peers[i].socket_descriptor > *max_fd) {
                *max_fd = peers[i].socket_descriptor;
            }
        }
    }
}

/* Clean up a disconnected peer */
void cleanup_peer(int peer_index) {
    int j;
    
    if (peer_index < 0 || peer_index >= MAX_PEERS) {
        return;
    }
    
    /* Close socket if open */
    if (peers[peer_index].socket_descriptor != -1) {
        close(peers[peer_index].socket_descriptor);
    }
    
    /* Reset peer entry */
    peers[peer_index].id = 0;
    peers[peer_index].socket_descriptor = -1;
    peers[peer_index].file_count = 0;
    peers[peer_index].is_active = 0;
    
    for (j = 0; j < MAX_FILES; j++) {
        peers[peer_index].files[j][0] = '\0';
    }
    
    peers[peer_index].address.sin_family = AF_INET;
    peers[peer_index].address.sin_port = 0;
    peers[peer_index].address.sin_addr.s_addr = 0;
}

/* Main function */
int main(int argc, char *argv[]) {
    int listen_sock;
    
    /* Check command line arguments */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }
    
    /* Initialize peer tracking */
    initialize_peers();
    
    /* Create listening socket */
    listen_sock = create_listen_socket(argv[1]);
    if (listen_sock == -1) {
        fprintf(stderr, "Failed to create listening socket\n");
        return 1;
    }
    
    printf("Registry listening on port %s\n", argv[1]);
    
    /* Main event loop */
    while (1) {
        fd_set readfds;
        int max_fd;
        int activity;
        int i;
        
        /* Set up file descriptor sets */
        setup_fd_sets(listen_sock, &readfds, &max_fd);
        
        /* Wait for activity on any socket */
        activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        
        if (activity == -1) {
            perror("select");
            break;
        }
        
        /* Check if there's a new connection on listening socket */
        if (FD_ISSET(listen_sock, &readfds)) {
            accept_new_peer(listen_sock);
        }
        
        /* Check all peer sockets for incoming data */
        for (i = 0; i < MAX_PEERS; i++) {
            if (!peers[i].is_active || peers[i].socket_descriptor == -1) {
                continue;
            }
            
            if (FD_ISSET(peers[i].socket_descriptor, &readfds)) {
                uint8_t msg_type;
                ssize_t bytes_received;
                
                /* Receive message type (1 byte) */
                bytes_received = recv(peers[i].socket_descriptor, &msg_type, 1, 0);
                
                if (bytes_received <= 0) {
                    /* Peer disconnected or error */
                    cleanup_peer(i);
                    continue;
                }
                
                /* Route message based on type */
                switch (msg_type) {
                    case 0:  /* JOIN */
                        if (handle_join_message(peers[i].socket_descriptor, i) == -1) {
                            cleanup_peer(i);
                        }
                        break;
                        
                    case 1:  /* PUBLISH */
                        if (handle_publish_message(peers[i].socket_descriptor, i) == -1) {
                            cleanup_peer(i);
                        }
                        break;
                        
                    case 2:  /* SEARCH */
                        if (handle_search_message(peers[i].socket_descriptor, i) == -1) {
                            cleanup_peer(i);
                        }
                        break;
                        
                    default:
                        fprintf(stderr, "Unknown message type: %u\n", msg_type);
                        cleanup_peer(i);
                        break;
                }
            }
        }
    }
    
    close(listen_sock);
    return 0;
}
