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
#include <errno.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include "ksocket.h"

// =====================================
// Mini Project 1 Submission
// Group Details:
// Member 1 Name: Devanshu Agrawal
// Member 1 Roll number: 22CS30066
// =====================================


// Shared resources
int shmid;
socket_entry* SM;

// Locks
int lock[MAX_SOCKETS];
struct sembuf pop1, vop1;

// Max function
int max(int a, int b) {
    return (a > b) ? a : b;
}

// Signal Handler
void signalhandler(int signo){
    for(int i=0; i<MAX_SOCKETS; i++) {
        if(SM[i].sockid > 0) close(SM[i].sockid);
    }
    shmdt(SM);
    shmctl(shmid, IPC_RMID, 0);
    for(int i=0; i<MAX_SOCKETS; i++){
        semctl(lock[i], 0, IPC_RMID, 0);
    }
    exit(0);
}

// Function to check resend timeout (scales with retransmit count)
int check_resend_timeout(struct timeval * last_send, int count) {
    struct timeval curr;
    gettimeofday(&curr, NULL);
    long udiff = (curr.tv_sec - last_send->tv_sec) * 1000000L + (curr.tv_usec - last_send->tv_usec);
    if (udiff >= (long)T * 1000000L * count) {
        return 1;
    } else return 0;
}

// Function to check send timeout (T seconds)
int check_send_timeout(struct timeval * last_send) {
    struct timeval curr;
    gettimeofday(&curr, NULL);
    long udiff = (curr.tv_sec - last_send->tv_sec) * 1000000L + (curr.tv_usec - last_send->tv_usec);
    if (udiff >= (long)T * 1000000L) {
        return 1;
    } else return 0;
}

// Function to generate packet out of a message buffer
void buf_to_packet(char* buf,data_packet* packet){
    packet->is_ack = buf[0] - '0';
    
    char seq_number[4];
    seq_number[0] = buf[1];
    seq_number[1] = buf[2];
    seq_number[2] = buf[3];
    seq_number[3] = '\0';

    packet->seq = atoi(seq_number);

    if(packet->is_ack == 0){
        strcpy(packet->data, &buf[4]);
    }else if(packet->is_ack == 1){
        packet->rwnd = (buf[4]-'0')*10+(buf[5]-'0');
    }
}

// Function to generate message buffer out of a packet
void packet_to_buf(data_packet* packet, char* buf){
    buf[0] = '0' + packet->is_ack;
    buf[1] = (packet->seq/100) + '0';
    buf[2] = (packet->seq%100)/10 + '0';
    buf[3] = (packet->seq%10) + '0';

    if(packet->is_ack){
        buf[4] = '0'+(packet->rwnd)/10;
        buf[5] = '0'+(packet->rwnd)%10;
    }
    else{
        strcpy(&buf[4],packet->data);
    }
}

// Receiver Thread
void *r_thread(void *args){
    fflush(stdout);
    printf("\033[1;32m");
    printf("*********R THREAD STARTED*********\n");
    printf("\033[0m");
    fflush(stdout);
    fd_set fds;
    while(1) {

        // Select call initalization
        FD_ZERO(&fds);
        int maxfd = 0;
        for(int i=0; i<MAX_SOCKETS; i++) {
            if (SM[i].isAlloc == 2 && SM[i].listen) {
                FD_SET(SM[i].sockid, &fds);
                maxfd = max(maxfd, SM[i].sockid);
            }
        }
        
        // Timeout set to T/2 seconds as per spec
        struct timeval timeout;
        timeout.tv_sec = T / 2;
        timeout.tv_usec = 0;

        // Select Call
        int activity = select(maxfd+1, &fds, NULL, NULL, &timeout);

        if(activity == 0){
            for(int i=0; i<MAX_SOCKETS; i++){
                P(lock[i]);

                // Checks for a new Sockets to listens
                if(SM[i].isAlloc == 2) SM[i].listen = 1;
                else SM[i].listen = 0;

                // send an false acknowledgement with new size
                if(SM[i].rwnd.window_size != 0 && SM[i].Flag == NOSPACE){

                    // send an false acknowledgement with new size
                    data_packet* packet = (data_packet* ) malloc(sizeof(data_packet));
                    packet->is_ack = 1;
                    packet->seq = (SM[i].rwnd.last_message % 256) + ((SM[i].rwnd.last_message % 256) == 0);
                    packet->rwnd = SM[i].rwnd.window_size;

                    char databuf[520];
                    packet_to_buf(packet,databuf);

                    sendto(SM[i].sockid, databuf, strlen(databuf)+1, 0, (struct sockaddr *) &SM[i].dest, sizeof(SM[i].dest));

                    SM[i].Flag = SPACE;

                }
                V(lock[i]);
            }
        }
        for(int i=0; i<MAX_SOCKETS; i++){
            P(lock[i]);
            // Check if got a message
            if (SM[i].isAlloc == 2 && FD_ISSET(SM[i].sockid, &fds)) {
                fflush(stdout);
                printf("\033[1;32m");
                printf("[R THREAD] Socket %d has received a message.\n",i);
                printf("\033[0m");
                fflush(stdout);
                
                // Make the receive call
                char buff[520];
                bzero(buff, sizeof(buff));

                struct sockaddr_in client;
                socklen_t clilen = sizeof(client);

                recvfrom(SM[i].sockid, buff, 520, 0, (struct sockaddr*)&client, &clilen);

                // if((SM[i].dest.sin_addr.s_addr != client.sin_addr.s_addr) || (SM[i].dest.sin_port != client.sin_port)){
                //     fflush(stdout);
                //     printf("\033[1;31m");
                //     printf("[R THREAD] Dest does not match.\n");
                //     printf("\033[0m");
                //     fflush(stdout);
                //     V(lock[i]);
                //     continue;
                // }

                data_packet* packet = (data_packet*) malloc (sizeof(data_packet));
                data_packet* response = (data_packet*) malloc (sizeof(data_packet));
                
                // Generate packet structure from buff
                buf_to_packet(buff,packet);

                // Prompt for receiving a message
                fflush(stdout);
                printf("\033[1;32m");
                if(packet->is_ack == 0) printf("[R THREAD] Received Message.\n Seq: %d\n",packet->seq);
                if(packet->is_ack == 1) printf("[R THREAD] Received Acknowledgement.\n Seq: %d\nRwnd: %d\n",packet->seq,packet->rwnd);
                printf("\033[0m");
                fflush(stdout);

                // Making the drop decision
                if (!dropMessage(P_arg)) {
                    if (packet->is_ack == 0) {

                        int front = SM[i].rwnd.last_message;
                        int end = (SM[i].rwnd.last_message+SM[i].rwnd.window_size-1)%256;
                        if (!end) end++;

                        // Check calc for old message - Old message always means out of window
                        int old_start = SM[i].rwnd.last_message-21;
                        if (old_start < 1) old_start += 255;

                        // Check message is part of th current window or not
                        int curr_wind = 0;
                        if ((end > front) && (packet->seq >= front && packet->seq <= end)) curr_wind = 1;
                        else if ((end < front) && (packet->seq>=front || packet->seq<=end)) curr_wind = 1;

                        // Check if it is part of the old window
                        int old = 0;
                        if ((old_start < front) && (packet->seq >= old_start && packet->seq < front)) old = 1;
                        else if ((old_start > front) && (packet->seq >= old_start || packet->seq < front)) old = 1; 

                        // If part of the old window, we send a duplicate acknowledgement
                        if (old) {   
                            bzero(&buff[4], sizeof(buff)-4);
                            response->is_ack = 1;
                            response->seq = packet->seq;
                            response->rwnd = SM[i].rwnd.window_size;

                            packet_to_buf(response,buff);
                            sendto(SM[i].sockid, buff, strlen(buff)+1, 0, (struct sockaddr *) &SM[i].dest, sizeof(SM[i].dest));
                        } 
                        // We check if we take it in
                        // First we handle an IN ORDER MESSAGE
                        else if ((SM[i].rwnd.window_size) && ((SM[i].rwnd.last_message == packet->seq-1) || (packet->seq == 1 && SM[i].rwnd.last_message==255))) {    // New Message which is expected                            
                            int index = (SM[i].rwnd.sliding_end + 1)%MAX_WINDOW_SIZE;
                            strcpy(SM[i].receive_buffer[index], &buff[4]);

                            // Loop to send acknowledgement of the message in the receiver buffer
                            do {
                                SM[i].rwnd.sliding_end = (SM[i].rwnd.sliding_end + 1) % MAX_WINDOW_SIZE;
                                SM[i].rwnd.seq_map[index] = 0;
                                SM[i].rwnd.window_size--;

                                if(SM[i].rwnd.window_size == 0){
                                    SM[i].Flag = NOSPACE;
                                }

                                // Constructing message
                                bzero(buff, sizeof(buff));
                                response->is_ack = 1;
                                int seq = (SM[i].rwnd.last_message+1)%256;
                                if (!seq) seq++;

                                response->seq = seq;
                                response->rwnd = SM[i].rwnd.window_size;
                                SM[i].rwnd.last_message = seq;

                                packet_to_buf(response,buff);
                                fflush(stdout);
                                printf("\033[1;32m");
                                printf("Sending Acknowledgement - %s - %d\n", buff, SM[i].rwnd.window_size);
                                printf("\033[0m");
                                fflush(stdout);
                                sendto(SM[i].sockid, buff, strlen(buff)+1, 0, (struct sockaddr *) &SM[i].dest, sizeof(SM[i].dest));
                                index = (SM[i].rwnd.sliding_end+1)%MAX_WINDOW_SIZE;

                            } while(SM[i].rwnd.seq_map[index] == ((packet->seq+1)%256)+(packet->seq == 255));
                        } 
                        // Handle OUT OF ORDER messages
                        else if (curr_wind) {   
                            // Just place it.
                            if (packet->seq < (SM[i].rwnd.last_message)) packet->seq += 255;
                            int index = (packet->seq - (SM[i].rwnd.last_message) + SM[i].rwnd.sliding_end) % MAX_WINDOW_SIZE;
                            if (SM[i].rwnd.seq_map[index] == 0) {
                                strcpy(SM[i].receive_buffer[index], &buff[4]);
                                SM[i].rwnd.seq_map[index] = packet->seq;
                                if (packet->seq > 255) SM[i].rwnd.seq_map[index] -= 255;
                            }
                        }
                    }
                    // Handle an acknowlegment
                    else {    
                        SM[i].timeout_tries = 0;
                        fflush(stdout);
                        printf("\033[1;32m");
                        printf("Reseting timeout tries for socket %d\n",i);
                        printf("\033[0m");
                        fflush(stdout);

                        // Check if the acknowledgement is part of the current window
                        int curr_window = 0;
                        int front = SM[i].swnd.seq;
                        int end = SM[i].swnd.seq + 9 - SM[i].swnd.window_size;
                        end %= 256;
                        if (end == 0) end++;
                        if (((end >= front) && (packet->seq >= front && packet->seq <= end)) || ((end < front) && (packet->seq <= end || packet->seq >= front))) curr_window = 1;

                        // If it current window we have received the acknowledgement for the all the messages that are uptil that.
                        if (curr_window) {
                            int count = 1, x = front;
                            while(x != front) {
                                x++;
                                count++;
                            }
                            x++;
                            SM[i].swnd.receive_size = packet->rwnd;
                            if (SM[i].swnd.receive_size == 0) SM[i].swnd.retransmit_nospace = 1;
                            else SM[i].swnd.retransmit_nospace = 0;

                            while(count--) {
                                SM[i].swnd.seq++;
                                SM[i].swnd.seq%=256;
                                if (SM[i].swnd.seq == 0) SM[i].swnd.seq++;
                                SM[i].swnd.sliding_start++;
                                SM[i].swnd.sliding_start %= 10;

                                bzero(&SM[i].send_buffer[SM[i].swnd.sliding_start], sizeof(SM[i].send_buffer[0]));
                                SM[i].swnd.unack[SM[i].swnd.sliding_start] = 0;
                                SM[i].swnd.window_size++;
                            }
                            
                        } 
                        else if ((packet->seq == SM[i].swnd.seq-1) || (packet->seq == 255 && (SM[i].swnd.seq == 0))) {

                            // else if it is in order than kust remove one.
                            SM[i].swnd.receive_size = packet->rwnd;
                            if (SM[i].swnd.receive_size == 0) SM[i].swnd.retransmit_nospace++;
                            else  SM[i].swnd.retransmit_nospace = 0;
                        }
                    }
                } 
                else {
                    fflush(stdout);
                    printf("\033[1;31m");
                    printf("[R THREAD] Package was dropped.\n");
                    printf("\033[0m");
                    fflush(stdout);
                }
            }
            V(lock[i]);
        }
    }
}

void *s_thread(void *args){
    fflush(stdout);
    printf("\033[1;34m");
    printf("*********S THREAD STARTED*********\n");
    printf("\033[0m");
    fflush(stdout);
    while(1) {
        usleep(TIMEOUT_HALF);
        for(int i=0; i<MAX_SOCKETS; i++) {
            P(lock[i]);
            data_packet* packet = (data_packet*) malloc (sizeof(data_packet));
            // if(SM[i].timeout_tries == MAX_RETRIES){
            //     // if max timeout has been reached don't do send
            //     fflush(stdout);
            //     printf("\033[1;31m");
            //     printf("[S THREAD] MAX RETRIES REACHED.\n");
            //     printf("\033[0m");
            //     fflush(stdout);
            // }
            if (SM[i].isAlloc == 2 && ((SM[i].swnd.sliding_end != SM[i].swnd.sliding_start) || (SM[i].swnd.window_size == 0)) && check_send_timeout(&SM[i].last_send)){
                
                gettimeofday(&SM[i].last_send, NULL);

                // if space if available start sending the messages.
                if (SM[i].swnd.receive_size) {
                    SM[i].timeout_tries++;
                    char databuf[520];
                    bzero(databuf, 520);

                    packet->seq = SM[i].swnd.seq;

                    int start = SM[i].swnd.sliding_start;
                    int end = SM[i].swnd.sliding_end;
                    int c = (SM[i].swnd.window_size == 0);
                    int max_count = SM[i].swnd.receive_size;
                    while(((start != end) || c) && SM[i].swnd.unack[(start+1)%10] && max_count) {
                        c = 0;
                        max_count--;
                        start = (start + 1)%10;
                        
                        bzero(databuf, 520);
                        packet->is_ack = 0;
                        strcpy(packet->data, SM[i].send_buffer[start]);

                        packet_to_buf(packet,databuf);

                        SM[i].total_transmissions++;
                        sendto(SM[i].sockid, databuf, strlen(databuf), 0, (struct sockaddr *) &SM[i].dest, sizeof(SM[i].dest));

                        packet->seq++;
                        packet->seq%=256;
                        if (packet->seq == 0) packet->seq++;
                    }
                } 
                else if (check_resend_timeout(&SM[i].last_send, SM[i].swnd.retransmit_nospace)){
                    // Ping the receiver in case of NOSPACE Hang up.

                    SM[i].timeout_tries++;
                    char databuf[520];
                    bzero(databuf, 520);

                    packet->seq = SM[i].swnd.seq - 1;
                    if (packet->seq == 0) packet->seq = 255;
                    packet->is_ack = 0;

                    packet_to_buf(packet,databuf);
                    fflush(stdout);

                    sendto(SM[i].sockid, databuf, strlen(databuf), 0, (struct sockaddr *) &SM[i].dest, sizeof(SM[i].dest));
                }
            }
            V(lock[i]);
        }
    }
}

void *garbage_collector(void *args){

    fflush(stdout);
    printf("\033[1;37m");
    printf("************Garbage collector thread started.************\n");
    printf("\033[0m");
    fflush(stdout);
    
    while(1){
        fflush(stdout);
        printf("\033[1;37m");
        printf("************Garbage collector Checks.************\n");
        printf("\033[0m");
        fflush(stdout);
        for (int i = 0; i < MAX_SOCKETS; i++)
        {
            P(lock[i]);
            if (SM[i].isAlloc != 0)
            {
                int status = kill(SM[i].process_id, 0);

                fflush(stdout);
                printf("\033[1;37m");
                printf("[GC THREAD] Process id -> %d\n", SM[i].process_id);
                printf("[GC THREAD] STATUS, sock entry-> %d %d\n", status, i);
                printf("\033[0m");
                fflush(stdout);
                if (status != 0)
                {
                    if(SM[i].sockid > 0){
                        close(SM[i].sockid);
                    }
                    
                    // Re Initalization
                    SM[i].isAlloc = 0;
                    SM[i].process_id = -1;

                    SM[i].sockid = 0;
                    SM[i].sour.sin_family = 0;
                        
                    SM[i].swnd.window_size = MAX_WINDOW_SIZE;
                    SM[i].swnd.receive_size = MAX_WINDOW_SIZE;
                    SM[i].swnd.sliding_start = MAX_WINDOW_SIZE - 1;
                    SM[i].swnd.sliding_end = MAX_WINDOW_SIZE - 1;
                    SM[i].swnd.seq = 1;

                    SM[i].rwnd.last_message = 0;
                    SM[i].rwnd.sliding_start = MAX_WINDOW_SIZE - 1;
                    SM[i].rwnd.sliding_end = MAX_WINDOW_SIZE - 1;
                    SM[i].rwnd.window_size = MAX_WINDOW_SIZE;
                    SM[i].swnd.retransmit_nospace = 0;

                    for(int i=0; i<MAX_WINDOW_SIZE; i++) {
                        SM[i].swnd.unack[i] = 0;
                        SM[i].rwnd.seq_map[i] = 0;
                        bzero(&SM[i].send_buffer[i], sizeof(SM[i].send_buffer[i]));
                        bzero(&SM[i].receive_buffer[i], sizeof(SM[i].receive_buffer[i]));
                    }

                    gettimeofday(&SM[i].last_send, NULL);
                    SM[i].total_messages = 0;
                    SM[i].total_transmissions = 0;
                    SM[i].listen = 0;
                    SM[i].Flag = SPACE;
                    SM[i].timeout_tries = 0;
                    
                    fflush(stdout);
                    printf("\033[1;37m");
                    printf("[GC THREAD] Socket entry %d cleared\n", i);
                    printf("\033[0m");
                    fflush(stdout);
                }
            }
            V(lock[i]);
        }
        sleep(50);
    }
}

int main(int argc, char* argv[]){
    srand(time(NULL));
    signal(SIGINT,signalhandler);

    fflush(stdout);
    printf("\033[1;36m");
    printf("************Main thread started.************\n");
    printf("\033[0m");
    fflush(stdout);

    shmid = shmget(SM_KEY, MAX_SOCKETS * sizeof(socket_entry), 0777 | IPC_CREAT);
    if(shmid == -1){

        fflush(stdout);
        printf("\033[1;31m");  // Keep error messages in red
        printf("[ACCESS ERROR]: Shared memory could not be accessed\n");
        printf("\033[0m");
        fflush(stdout);

        exit(0);
    }
    
    SM = (socket_entry*) shmat(shmid, 0, 0);
    for (int i = 0; i < MAX_SOCKETS; i++)
    {
        lock[i] = semget(LOCK_KEY(i), 1, 0777 | IPC_CREAT);
        semctl(lock[i],0,SETVAL,1);
        if(lock[i] == -1){
            fflush(stdout);
            printf("\033[1;31m");  // Keep error messages in red
            printf("[ACCESS ERROR]: Lock No %d could not be accessed... terminating fetch for others\n", i+1);
            printf("\033[0m");
            fflush(stdout);
            exit(0);
        }
    }
    pop1.sem_num = vop1.sem_num = 0;
    pop1.sem_flg = vop1.sem_flg = 0;
    pop1.sem_op = -1;
    vop1.sem_op = 1;

    fflush(stdout);
    printf("\033[1;36m");
    printf("************Initalization************\n");
    printf("\033[0m");
    fflush(stdout);
    
    for(int i = 0; i < MAX_SOCKETS; i++){
        SM[i].isAlloc = 0;
        SM[i].process_id = -1;

        SM[i].sockid = 0;
        SM[i].sour.sin_family = 0;
                
        SM[i].swnd.window_size = MAX_WINDOW_SIZE;
        SM[i].swnd.receive_size = MAX_WINDOW_SIZE;
        SM[i].swnd.sliding_start = MAX_WINDOW_SIZE - 1;
        SM[i].swnd.sliding_end = MAX_WINDOW_SIZE - 1;
        SM[i].swnd.seq = 1;

        SM[i].rwnd.last_message = 0;
        SM[i].rwnd.sliding_start = MAX_WINDOW_SIZE - 1;
        SM[i].rwnd.sliding_end = MAX_WINDOW_SIZE - 1;
        SM[i].rwnd.window_size = MAX_WINDOW_SIZE;

        for(int i=0; i<MAX_WINDOW_SIZE; i++) {
            SM[i].swnd.unack[i] = 0;
            SM[i].rwnd.seq_map[i] = 0;
            bzero(&SM[i].send_buffer[i], sizeof(SM[i].send_buffer[i]));
            bzero(&SM[i].receive_buffer[i], sizeof(SM[i].receive_buffer[i]));
        }

        gettimeofday(&SM[i].last_send, NULL);
        SM[i].total_messages = 0;
        SM[i].total_transmissions = 0;
        SM[i].listen = 0;
        SM[i].Flag = SPACE;
        SM[i].timeout_tries = 0;
    }

    pthread_t r_thread_id, s_thread_id, gc_thread_id;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_create(&r_thread_id, &attr, r_thread, NULL);
    pthread_create(&s_thread_id, &attr, s_thread, NULL);
    pthread_create(&gc_thread_id, &attr, garbage_collector, NULL);

    pthread_join(r_thread_id, NULL);
    pthread_join(s_thread_id, NULL);
    pthread_join(gc_thread_id, NULL);

    fflush(stdout);
    printf("\033[1;36m");
    printf("Main Thread entered creation mode.....\n");
    printf("\033[0m");
    fflush(stdout);

    while(1){
        for (int i = 0; i< MAX_SOCKETS; i++){
            P(lock[i]);
            if(SM[i].isAlloc == 1 && SM[i].sockid == 0){
                fflush(stdout);
                printf("\033[1;36m");
                printf("[MAIN THREAD][REQUEST] Received a CREATION request at socket %d from process %d\n",i,SM[i].process_id);
                printf("\033[0m");
                fflush(stdout);
                
                SM[i].sockid = socket(AF_INET,SOCK_DGRAM,0);

                if(SM[i].sockid == -1){
                    fflush(stdout);
                    printf("\033[1;31m");  // Keep error messages in red
                    printf("[MAIN THREAD][ERROR] SOCKET INIT: socket request by process %d for socket %d could not be satisfied.\n",SM[i].process_id,i);
                    printf("\033[0m");
                    fflush(stdout);
                }
            }
            if(SM[i].isAlloc == 1 && SM[i].sour.sin_family != 0){
                fflush(stdout);
                printf("\033[1;36m");
                printf("[MAIN THREAD][REQUEST] Received a BINDING request at socket %d from process %d\n",i,SM[i].process_id);
                printf("\033[0m");
                fflush(stdout);

                if((bind(SM[i].sockid, (struct sockaddr *) &SM[i].sour, sizeof(SM[i].sour))) == -1){
                    fflush(stdout);
                    printf("\033[1;31m");  // Keep error messages in red
                    printf("[MAIN THREAD][ERROR] BINDING: binding request by process %d for socket %d could not be satisfied.\n",SM[i].process_id,i);
                    printf("Error: %s\n",strerror(errno));
                    printf("\033[0m");
                    fflush(stdout);
                    SM[i].isAlloc = 4; 
                }
                else{
                    fflush(stdout);
                    printf("\033[1;36m");
                    printf("[MAIN THREAD][SUCCESS] BINDING: binding request by process %d for socket %d satisfied.\n",SM[i].process_id,i);
                    printf("\033[0m");
                    fflush(stdout);
                    SM[i].isAlloc = 2;
                }
            }
            if (SM[i].isAlloc == 3 && SM[i].listen == 0)
            {  
                fflush(stdout);
                printf("\033[1;36m");
                printf("[MAIN THREAD][REQUEST] Received a CLOSING request at socket %d from process %d\n",i,SM[i].process_id);
                printf("\033[0m");
                fflush(stdout);

                if(SM[i].sockid > 0){
                    close(SM[i].sockid);
                }

                SM[i].isAlloc = 0;
                SM[i].process_id = -1;
                SM[i].sockid = 0;
                SM[i].sour.sin_family = 0;
                SM[i].swnd.window_size = MAX_WINDOW_SIZE;
                SM[i].swnd.receive_size = MAX_WINDOW_SIZE;
                SM[i].swnd.sliding_start = MAX_WINDOW_SIZE - 1;
                SM[i].swnd.sliding_end = MAX_WINDOW_SIZE - 1;
                SM[i].swnd.seq = 1;
                SM[i].rwnd.last_message = 0;
                SM[i].rwnd.sliding_start = MAX_WINDOW_SIZE - 1;
                SM[i].rwnd.sliding_end = MAX_WINDOW_SIZE - 1;
                SM[i].rwnd.window_size = MAX_WINDOW_SIZE;
                SM[i].timeout_tries = 0;
                
                for(int i=0; i<MAX_WINDOW_SIZE; i++) {
                    SM[i].swnd.unack[i] = 0;
                    SM[i].rwnd.seq_map[i] = 0;
                    bzero(&SM[i].send_buffer[i], sizeof(SM[i].send_buffer[i]));
                    bzero(&SM[i].receive_buffer[i], sizeof(SM[i].receive_buffer[i]));
                }

                gettimeofday(&SM[i].last_send, NULL);
                SM[i].total_messages = 0;
                SM[i].total_transmissions = 0;
                SM[i].listen = 0;
                SM[i].Flag = SPACE;
                
                fflush(stdout);
                printf("\033[1;36m");
                printf("[MAIN THREAD] Socket entry closed %d cleared\n", i);
                printf("\033[0m");
                fflush(stdout);
            }
            V(lock[i]);
        }
    }
}