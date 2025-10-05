#include <network.h>
#include <protocol.h>
#include <util.h>

#define ACTION_JOIN    0
#define ACTION_PUBLISH 1
#define ACTION_SEARCH  2

#define JOIN_MSG_SIZE     5
#define SEARCH_RESP_SIZE  10

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
        command[strcspn(command, "\n")] = 0; // Remove newline character

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
