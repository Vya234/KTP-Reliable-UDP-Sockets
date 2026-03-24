// =====================================
// Mini Project 1 Submission
// Member 1 Name: Kavya Rai
// Member 1 Roll number: 23CS10031
// =====================================
#ifndef KSOCKET_H
#define KSOCKET_H

#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/select.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <pthread.h>
#include <semaphore.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>

#define MAX_SOCKETS 10
#define MAX_WINDOW_SIZE 10
#define T 5
#define TIMEOUT_HALF  (T * 1000000 / 2)
#define MESSAGE_SIZE 512

#define SM_KEY ftok(".", 92)
#define LOCK_KEY(i) ftok(".", 22 + i)

#define SOCK_KTP 1
#define P_arg 0.2

#define SPACE 0
#define NOSPACE 1

#define MAX_RETRIES 10

#define ENOSPACE 101
#define ENOTBOUND 102
#define ENOMESSAGE 103
#define ENOSOCK 104

typedef struct data_packet{
    int is_ack;
    int seq;
    union{
        char data[MESSAGE_SIZE];
        int rwnd;
    };
} data_packet;

// swnd structure
struct swnd {
    int unack[MAX_WINDOW_SIZE];
    int receive_size;
    int window_size;
    int seq;
    int sliding_start;
    int sliding_end;
    int retransmit_nospace;
};

typedef struct swnd swnd_type;

struct rwnd {
    int seq_map[MAX_WINDOW_SIZE];
    int window_size;
    int last_message;
    int sliding_start;
    int sliding_end;
};

typedef struct rwnd rwnd_type;

typedef struct shared_memory_entry{
    // if it is allocated or not 
    // ==0 implies not allocated
    // ==1 is allocated
    // ==2 is allocated and binded
    // ==3 is asked to close
    // ==4 general error state to be3 used for imoplementation
    int isAlloc;

    // Pid of the process that allocated it
    pid_t process_id;

    // sockid  == 0 implies that socket has not been assigned yet
    // == -1 implies failed socket creation
    // > 0 implies that socket has been created properly
    int sockid;

    // tell sour address
    struct sockaddr_in sour;
    struct sockaddr_in dest;

    // recv and sendbuffer
    char send_buffer[MAX_WINDOW_SIZE][MESSAGE_SIZE];    
    char receive_buffer[MAX_WINDOW_SIZE][MESSAGE_SIZE]; 

    swnd_type swnd;
    rwnd_type rwnd;

    struct timeval last_send; 

    int listen;

    int total_messages;
    int total_transmissions;
    int Flag;

    int timeout_tries;
} socket_entry;

void P(int s);
void V(int s);

int k_socket(int domain, int type, int protocol);
int k_bind(int kfd,struct sockaddr_in* source, struct sockaddr_in* dest);
int k_sendto(int kfd,char* buf,int buf_size, int flag,struct sockaddr_in* dest,socklen_t dest_size);
int k_recvfrom(int kfd,  char* buf,int buf_size,int flag,struct sockaddr_in * dest, socklen_t* dest_size);
int k_close(int kfd);

int dropMessage(float p);

void packet_to_buf(data_packet* packet, char* buf);
void buf_to_packet( char* buf,data_packet* packet);

#endif