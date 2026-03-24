// =====================================
// Mini Project 1 Submission
// Member 1 Name: Kavya Rai
// Member 1 Roll number: 23CS10031
// =====================================

#include "ksocket.h"

int shmid;
int lock[MAX_SOCKETS];
socket_entry* SM;
struct sembuf pop1, vop1;

int main(int argc, char* argv[]) {
    // Display startup message
    printf("\033[1;34m");
    printf("The USER1 is starting...\n");
    printf("\033[0m");
    
    // Validate command-line arguments
    if (argc < 6) {
        printf("\033[1;31m");
        printf("The usage is:\n<cmd> <Source IP> <Source Port> <Remote IP> <Remote Port> <inputfile.txt>\n");
        printf("\033[0m");
        exit(EXIT_FAILURE);
    }
    
    // Create a custom socket
    int sockfd = k_socket(AF_INET, SOCK_KTP, 0);
    
    if (sockfd == -1) {
        perror("[*]ERROR: There is an error in making socket.\n");
        exit(EXIT_FAILURE);
    }

    // Initialize source and remote addresses
    struct sockaddr_in source_addr, remote_addr;
    
    bzero(&source_addr, sizeof(source_addr));
    bzero(&remote_addr, sizeof(remote_addr));
    
    inet_aton(argv[1], &source_addr.sin_addr);
    inet_aton(argv[3], &remote_addr.sin_addr);
    
    source_addr.sin_family = remote_addr.sin_family = AF_INET;
    source_addr.sin_port = htons(atoi(argv[2]));
    remote_addr.sin_port = htons(atoi(argv[4]));
    
    // Bind socket to the source address
    if (k_bind(sockfd, &source_addr, &remote_addr) < 0) {
        perror("[*]ERROR: Cannot bind the port.\n");
    }

    // Open input file for reading
    int fd = open(argv[5], O_RDONLY);
    
    if (fd == -1) {
        perror("[*]ERROR: Cannot open file\n");
    }
    
    char buf[MESSAGE_SIZE];
    
    printf("\033[1;34m");
    printf("The User1 starts sending file.\n");
    printf("\033[0m");

    while(1){
        bzero(buf, sizeof(buf));
        memset(buf, '\0', sizeof(buf));
        int byteRead = read(fd, buf, sizeof(buf) - 1);
        buf[byteRead] = '\0';
        if (byteRead == -1) {
            perror("[*]ERROR: Reading error in input file.\n");
            exit(EXIT_FAILURE);
        }
        
        if (byteRead == 0) {
            bzero(buf, sizeof(buf));
            sprintf(buf, "END");
            while(k_sendto(sockfd, buf, strlen(buf)+1, 0, &remote_addr, sizeof(remote_addr)) < 0) usleep(1000);
            printf("Reached the end of input file.\n");
            break;
        }
        while(k_sendto(sockfd, buf, strlen(buf)+1, 0, &remote_addr, sizeof(remote_addr)) < 0) usleep(1000);
    }

    k_close(sockfd);
    sleep(3);
    
}
