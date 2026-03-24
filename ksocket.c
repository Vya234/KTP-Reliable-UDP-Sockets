// =====================================
// Mini Project 1 Submission
// Member 1 Name: Kavya Rai
// Member 1 Roll number: 23CS10031
// =====================================

#include "ksocket.h" 

// dropMessage: simulates an unreliable link.
// Returns 1 (drop) with probability p, else 0 (keep).
int dropMessage(float p) {
    float val = (float)rand() / RAND_MAX;
    return (val < p);
}

// Semaphores pop and vop
void P(int s){
    struct sembuf pop1;
    pop1.sem_num = 0;
    pop1.sem_flg = 0;
    pop1.sem_op = -1;
    semop(s, &pop1, 1);
} 
void V(int s) {
    struct sembuf vop1;
    vop1.sem_num = 0;
    vop1.sem_flg = 0;
    vop1.sem_op = 1;
    semop(s, &vop1, 1);
}


int k_socket(int domain, int type, int protocol){
    int shmid = shmget(SM_KEY, MAX_SOCKETS * sizeof(socket_entry), 0);
    int lock[MAX_SOCKETS];
    if(shmid == -1){
        printf("\033[1;31m");
        printf("[ACCESS ERROR]: Shared memory could not be accessed\n");
        printf("\033[0m");
        exit(0);
    }
    socket_entry* SM = (socket_entry*) shmat(shmid, 0, 0);

    for (int i = 0; i < MAX_SOCKETS; i++)
    {
        lock[i] = semget(LOCK_KEY(i), 1, 0);
        if(lock[i] == -1){
            printf("\033[1;31m");
            printf("[ACCESS ERROR]: Lock No %d could not be accessed... terminating fetch for others\n", i+1);
            printf("\033[0m");
            exit(0);
        }
    }
    
    if(type != SOCK_KTP){
        perror("[ERROR]K_SOCKET: Cannot decipher socket type. Please use SOCK_KTP.\n");
        exit(EXIT_FAILURE);
    }

    int i;
    for(i = 0; i<MAX_SOCKETS; i++){
        P(lock[i]);
        if(SM[i].isAlloc == 0){
            SM[i].isAlloc = 1;
            SM[i].process_id = getpid();
            V(lock[i]);
            break;
        }
        V(lock[i]);
    }
    
    int kfd = i;
    
    if(kfd == MAX_SOCKETS){
        fprintf(stderr,"[ERROR]K_SOCKET: The Sockets are fully allocated.\n");
        V(lock[i]);
        errno = ENOSOCK;
        shmdt(SM);
        return -1;
    }

    while(SM[kfd].sockid == 0) usleep(1000);

    if(SM[kfd].sockid == -1){
        P(lock[kfd]);
        SM[kfd].isAlloc = 3;
        V(lock[kfd]);
        shmdt(SM);
        return -1;
    }
    shmdt(SM);
    return kfd;
}

int k_bind(int kfd,struct sockaddr_in* source, struct sockaddr_in* dest){
    int shmid = shmget(SM_KEY, MAX_SOCKETS * sizeof(socket_entry), 0);
    int lock[MAX_SOCKETS];
    if(shmid == -1){
        printf("\033[1;31m");
        printf("[ACCESS ERROR]: Shared memory could not be accessed\n");
        printf("\033[0m");
        exit(0);
    }
    socket_entry* SM = (socket_entry*) shmat(shmid, 0, 0);
    for (int i = 0; i < MAX_SOCKETS; i++)
    {
        lock[i] = semget(LOCK_KEY(i), 1, 0);
        if(lock[i] == -1){
            printf("\033[1;31m");
            printf("[ACCESS ERROR]: Lock No %d could not be accessed... terminating fetch for others\n", i+1);
            printf("\033[0m");
            exit(0);
        }
    }

    P(lock[kfd]);
    SM[kfd].sour = (*source);
    SM[kfd].dest = (*dest);
    V(lock[kfd]);
    while(SM[kfd].isAlloc < 2) usleep(1000);
    if(SM[kfd].isAlloc == 4){
        P(lock[kfd]);
        SM[kfd].isAlloc = 3;
        V(lock[kfd]);
        shmdt(SM);
        return -1;
    }
    shmdt(SM);
    return 1;
}

int k_sendto(int kfd,char* buf,int buf_size, int flag,struct sockaddr_in* dest,socklen_t dest_size){
    int shmid = shmget(SM_KEY, MAX_SOCKETS * sizeof(socket_entry), 0);
    int lock[MAX_SOCKETS];
    if(shmid == -1){
        printf("\033[1;31m");
        printf("[ACCESS ERROR]: Shared memory could not be accessed\n");
        printf("\033[0m");
        exit(0);
    }
    socket_entry* SM = (socket_entry*) shmat(shmid, 0, 0);
    for (int i = 0; i < MAX_SOCKETS; i++)
    {
        lock[i] = semget(LOCK_KEY(i), 1, 0);
        if(lock[i] == -1){
            printf("\033[1;31m");
            printf("[ACCESS ERROR]: Lock No %d could not be accessed... terminating fetch for others\n", i+1);
            printf("\033[0m");
            exit(0);
        }
    }

    P(lock[kfd]);

    if (SM[kfd].dest.sin_addr.s_addr != dest->sin_addr.s_addr || SM[kfd].dest.sin_port != dest->sin_port) {
        V(lock[kfd]);
        shmdt(SM);
        errno = ENOTBOUND;
        return -1;
    }
    // Check for space
    if (SM[kfd].swnd.window_size == 0) {
        V(lock[kfd]);
        shmdt(SM);

        errno = ENOSPACE;

        return -1;
    }

    // more than 512, return error for now
    if (strlen(buf) > MESSAGE_SIZE) {
        V(lock[kfd]);
        shmdt(SM);
        return -1;
    }


    int index = (SM[kfd].swnd.sliding_end + 1)%MAX_WINDOW_SIZE;
    SM[kfd].swnd.sliding_end = index;
    bzero(&SM[kfd].send_buffer[index], MESSAGE_SIZE);

    SM[kfd].total_messages++;

    strcpy(SM[kfd].send_buffer[index], buf);
    SM[kfd].swnd.window_size--;
    SM[kfd].swnd.unack[index] = 1;
    V(lock[kfd]);
    shmdt(SM);
    return strlen(buf);
    
}

int k_recvfrom(int kfd,  char* buf,int buf_size,int flag,struct sockaddr_in * dest, socklen_t* dest_size){
    int shmid = shmget(SM_KEY, MAX_SOCKETS * sizeof(socket_entry), 0);
    int lock[MAX_SOCKETS];
    if(shmid == -1){
        printf("\033[1;31m");
        printf("[ACCESS ERROR]: Shared memory could not be accessed\n");
        printf("\033[0m");
        exit(0);
    }

    socket_entry* SM = (socket_entry*) shmat(shmid, 0, 0);
    for (int i = 0; i < MAX_SOCKETS; i++)
    {
        lock[i] = semget(LOCK_KEY(i), 1, 0);
        if(lock[i] == -1){
            printf("\033[1;31m");
            printf("[ACCESS ERROR]: Lock No %d could not be accessed... terminating fetch for others\n", i+1);
            printf("\033[0m");
            exit(0);
        }
    }

    P(lock[kfd]);
    if (SM[kfd].rwnd.window_size == MAX_WINDOW_SIZE) {
        V(lock[kfd]);
        shmdt(SM);

        errno = ENOMESSAGE;

        return -1;
    }
    
    int index = (SM[kfd].rwnd.sliding_start + 1)%MAX_WINDOW_SIZE;
    SM[kfd].rwnd.sliding_start = index;
    SM[kfd].rwnd.window_size++;
    
    strcpy(buf, SM[kfd].receive_buffer[index]);
    bzero(SM[kfd].receive_buffer[index], MESSAGE_SIZE);
    V(lock[kfd]);

    shmdt(SM);
    return strlen(buf);
}

int k_close(int kfd){
    int shmid = shmget(SM_KEY, MAX_SOCKETS * sizeof(socket_entry), 0);
    int lock[MAX_SOCKETS];
    if(shmid == -1){
        printf("\033[1;31m");
        printf("[ACCESS ERROR]: Shared memory could not be accessed\n");
        printf("\033[0m");
        exit(0);
    }
    socket_entry* SM = (socket_entry*) shmat(shmid, 0, 0);
    for (int i = 0; i < MAX_SOCKETS; i++)
    {
        lock[i] = semget(LOCK_KEY(i), 1, 0);
        if(lock[i] == -1){
            printf("\033[1;31m");
            printf("[ACCESS ERROR]: Lock No %d could not be accessed... terminating fetch for others\n", i+1);
            printf("\033[0m");
            exit(0);
        }
    }

    while(SM[kfd].swnd.window_size != 10 && SM[kfd].timeout_tries < MAX_RETRIES) usleep(1000);

    P(lock[kfd]);
    if(SM[kfd].total_messages > 0)
        printf("Total Messages: %d Total Transmissions: %d Ratio: %.2f\n",SM[kfd].total_messages,SM[kfd].total_transmissions,(1.00 * SM[kfd].total_transmissions ) / SM[kfd].total_messages);
    SM[kfd].isAlloc = 3;
    V(lock[kfd]);
    shmdt(SM);
    return 1;
}