// =====================================
// Mini Project 1 Submission
// Group Details:
// Member 1 Name: Kavya Rai
// Member 1 Roll number: 23CS10031
// =====================================

#include "ksocket.h"

int shmid;
int lock[MAX_SOCKETS];
socket_entry* SM;
struct sembuf pop1, vop1;

// Global variables
int currseq = 1;
struct sockaddr_in* destination;

int main(int argc,char* argv[]){
    // Display startup message
    printf("\033[1;34m");
    printf("The USER 2 is starting...\n");
    printf("\033[0m");

    // Validate command-line arguments
    if(argc < 6){
        printf("\033[1;31m");
        printf("The usage is:\n<cmd> <Source IP> <Source Port> <Remote IP> <Remote Port> <outputfile.txt>\n");
        printf("\033[0m");
        exit(EXIT_FAILURE);
    }
    
    // Create a custom socket
    int kfd = k_socket(AF_INET,SOCK_KTP,0);
    
    if(kfd == -1){
        perror("[*]ERROR: There is error in making socket.\n");
        exit(EXIT_FAILURE);
    }

    // Initialize source and remote addresses
    struct sockaddr_in source_addr,remote_addr;

    bzero(&source_addr,sizeof(source_addr));
    bzero(&remote_addr,sizeof(source_addr));

    inet_aton(argv[1],&source_addr.sin_addr);
    inet_aton(argv[3],&remote_addr.sin_addr);

    source_addr.sin_family = remote_addr.sin_family = AF_INET;
    
    source_addr.sin_port = htons(atoi(argv[2]));
    remote_addr.sin_port = htons(atoi(argv[4]));

    // Bind socket to the source address
    if(k_bind(kfd,&source_addr,&remote_addr) < 0){
        perror("[*]ERROR: Cannot Bind the port.\n");
        exit(EXIT_FAILURE);
    }

    // Open the output file for writing (create if not exists, truncate if exists)
    int fd = open(argv[5], O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fd == -1) {
        perror("[*]ERROR: Cannot open file\n");
    }

    char buf[MESSAGE_SIZE];

    // Indicate that User 2 is ready to receive the file
    printf("\033[1;34m");
    printf("The User 2 starts accepting file.\n");
    printf("\033[0m");

    struct sockaddr_in cliaddr; socklen_t clilen;

    while(1) {
        bzero(buf, MESSAGE_SIZE);
        while(k_recvfrom(kfd, buf, MESSAGE_SIZE, 0, &cliaddr, &clilen) < 0) usleep(1000);
        printf("\033[1;34m");
        printf("Received Packet.\nData: %s\n\n",buf);
        printf("\033[0m");
        if (strcmp(buf, "END") == 0) break;
        if (write(fd, buf, strlen(buf)) == -1) {
            perror("[*]ERROR: Cannot write into the file.\n");
            exit(EXIT_FAILURE);
        }
    }
    
    k_close(kfd);
}