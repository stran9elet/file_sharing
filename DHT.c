#include <stdio.h>         
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>  
#include <pthread.h>
#include <limits.h>
#include <openssl/sha.h>

#include "message_codes.h"
#include "client.h"
#include "node.h"

# define K 20
# define ROUTING_TABLE_SIZE SHA_DIGEST_LENGTH*8
# define BUF_SIZE 1024

unsigned short int self_port;
unsigned char id[SHA_DIGEST_LENGTH];

int bootstrap = 0;
// a pre-known bootstrap node in our system
char *bootstrap_ip;
unsigned short int bootstrap_port;;

// routing table will be an array of ll node pointers
// each ll will contain atmost k nodes
// means we remember k nodes from each subtree the current node is not a part of
// that's why it is of size SHA_DIGEST_LENGTH*8, i.e. 160, i.e. the height of the tree
// because there is 1 subtree which differes at path bit 1, one subtree which differs at bit 2, one subtree which differs at bit 3, and so on upto bit 160
struct node *routing_table[ROUTING_TABLE_SIZE];

struct ThreadPool {
    pthread_t *threads;
    int *clientqueue; // a circular queue of client fds
    long clientqueue_length;
    long threadcount;
    pthread_mutex_t lock;
    pthread_cond_t is_empty;
    long head;
    long tail; // here tail represents index of last element
};


// returns 20 byte char array representing xor distance, starting from msb and ending on lsb
void xor_distance(unsigned char *hash1, unsigned char *hash2, unsigned char *dist) {
    for (int i=0; i<SHA_DIGEST_LENGTH; i++) {
        dist[i] = hash1[i] ^ hash2[i];
    }
}

// returns +ve if dist1 > dist2, 0 if dist1 = dist2 and -ve if dist1 < dist2
int cmp_distance(unsigned char *dist1, unsigned char *dist2){
    for (int i=0; i<SHA_DIGEST_LENGTH; i++){
        if (dist1[i] > dist2[i])
            return 1;
        if (dist1[i] < dist2[i])
            return -1;
    }
    return 0;
}

void copy_node(struct node *src, struct node *dest){
    dest->node_addr = src->node_addr;
    dest->node_id = src->node_id;
    dest->next = src->next;
}


// find the k nearest nodes to a given hash from our own routing table
// return will be an array of atmost k node pointers sorted from closest to farthest
// use lock when accessing the routing table
int find_node(unsigned char *hash, struct node *k_closest[K]){

    unsigned char *distances[K]; // an array of size k
    // each element is a pointer to an array of unsigned char distances of length SHA_DIGEST_LENGTH
    for (int i = 0; i < K; i++) {
        k_closest[i] = NULL;
        distances[i] = NULL;
    }

    unsigned char *dist;

    struct node *cur;

    int last_idx = -1;

    // check your own routing table to find atmost the k closest nodes you know to the hash
    for (int i=0; i<ROUTING_TABLE_SIZE; i++){
        cur = routing_table[i]; // the first node is the closest, and the last Kth one is the farthest
        if (cur == NULL)
            continue;

        for (int j=0; j<K && cur != NULL; j++){
            dist = (unsigned char*) malloc(SHA_DIGEST_LENGTH * sizeof(unsigned char));
            xor_distance(hash, cur->node_id, dist);

            int replace_pos = 0;
            for (replace_pos=0; replace_pos <= last_idx; replace_pos++){
                if (cmp_distance(dist, distances[replace_pos]) <= 0)
                    break;
            }

            struct node *curcopy = cur;

            while (replace_pos < K){
                unsigned char *temp = distances[replace_pos];
                struct node *tempnode = k_closest[replace_pos];

                distances[replace_pos] = dist;
                k_closest[replace_pos] = curcopy;

                dist = temp;
                curcopy = tempnode;

                if (replace_pos == last_idx+1){
                    last_idx++;
                    break;
                }

                replace_pos++;
            }

            if (replace_pos >= K)
                free(dist);

            cur = cur->next;

        }
    }

    for (int i=0; i<K; i++){
        if (distances[i] != NULL)
            free(distances[i]);
    }

    return last_idx+1;
}


// recursively call find_node to find the globally closest k nodes to a given hash
void find_k_closest(unsigned char *hash, struct node *k_closest_global[K]){
    struct node *k_closest[K];
    for (int i=0; i<K; i++){
        k_closest[i] = NULL;
        k_closest_global[i] = NULL;
    }

    int max_retries = 100;
    for(int t=0; t<max_retries; t++){
        // connect to each node in the previous k_closest set, and ask them for find_node to return you its local k closest nodes
        find_node(hash, k_closest);

        int global_found = 1;
        for (int i=0; i<K; i++){
            if (!((k_closest[i] == NULL && k_closest_global[i] == NULL) || cmp_distance(k_closest[i], k_closest_global[i]) == 0)){
                global_found = 0;
                break;
            }
        }

        if (global_found)
            break;
    }
}


void generate_id(unsigned char *id){
    srand(time(NULL));

    while (1){
        // generate a random id, and check if its not taken by another node
        for (int i=0; i<SHA_DIGEST_LENGTH; i++){
            id[i] = rand() & 0xFF;
        }

        // find out which nodes are closest to given id
        // if our id is already a part of the array received, then generate a new id
        int already_exists = 0;
        struct node *k_nearest[K];
        find_k_closest(id, k_nearest);
        for (int j=0; j<K; j++){
            unsigned char dist[SHA_DIGEST_LENGTH];
            if (k_nearest[j] != NULL && cmp_distance(id, k_nearest[j]->node_id) == 0){
                already_exists = 1;
                break;
            }
        }

        if (!already_exists)
            break;
    }
}

// Use locks when accessing routing table, and other global variables
void* thread_handler(void *pool){
    struct ThreadPool *threadPool = (struct ThreadPool*) pool; 

    while (1){ // this loop will continuously read client from taskqueue and execute
        pthread_mutex_lock(&(threadPool->lock));
        if (threadPool->head == -1 && threadPool->tail == -1) { // queue is empty
            // no data to read- put the thread to sleep
            pthread_cond_wait(&(threadPool->is_empty), &(threadPool->lock)); // will put this thread on sleep and unlock the mutex lock for others to use
            pthread_mutex_unlock(&(threadPool->lock));
            continue;
        }

        // dequeue
        int newfd = (threadPool->clientqueue)[threadPool->head];
        threadPool->head++;
        if (threadPool->head == threadPool->tail+1){ // queue got empty
            threadPool->head = -1;
            threadPool->tail = -1;
        }
        threadPool->head = threadPool->head % (threadPool->clientqueue_length);

        pthread_mutex_unlock(&(threadPool->lock));


        // parse the message and send the response
        // char buffer[BUF_SIZE] = {0};
        int rbytes;
        int msg_code;
        if ((rbytes = recv(newfd, msg_code, sizeof(int), 0)) < 0){
            printf("Error while receiving");
            return NULL;
        }

        printf("Received: %d\n", msg_code);

        switch (msg_code) {
            case MSG_GET_ID:
                unsigned char id[SHA_DIGEST_LENGTH];
                generate_id(id);
                if (send(newfd, id, sizeof(id), 0) < 0){
                    printf("Error while sending id");
                }
                break;
            default:
                char *msg = "world";
                if (send(newfd, msg, strlen(msg), 0) < 0){
                    printf("Error while sending default message");
                    return NULL;
                }
                break;
        }

        if (close(newfd) < 0){
            printf("Error in closing newfd"); 
            return NULL;
        }
    }
}


void *start_server(void *server_args){

    int  *args = (int *) server_args;
    int port = args[0];
    int threadcount = args[1];
    int l_clientqueue = args[2];

    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        printf("Error in creating socket\n");
        return NULL;
    }

    struct sockaddr_in server_sockaddr, client_sockaddr;
    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_addr.s_addr = INADDR_ANY;
    server_sockaddr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*) &server_sockaddr, sizeof(server_sockaddr)) < 0){
        printf("Error in binding socket\n");
        return NULL;
    }

    if (listen(sockfd, SOMAXCONN) < 0){
        printf("Error in listening\n");
        return NULL;
    }

    struct ThreadPool *threadPool = (struct ThreadPool*) malloc(sizeof(struct ThreadPool));
    threadPool->clientqueue_length = l_clientqueue; // 10000
    threadPool->head = -1;
    threadPool->tail = -1;
    pthread_mutex_init(&(threadPool->lock), NULL);
    pthread_cond_init(&(threadPool->is_empty), NULL);

    threadPool->clientqueue = (int *) malloc(threadPool->clientqueue_length * sizeof(int));

    threadPool->threadcount = threadcount;
    threadPool->threads = (pthread_t*) malloc(threadPool->threadcount * sizeof(pthread_t));

    for (int i=0; i<threadPool->threadcount; i++){
        pthread_create(&(threadPool->threads[i]), NULL, &thread_handler, threadPool);
    }

    printf("Server listening on port %d\n", port);

    fd_set readfds; 
    while(1) {

        FD_ZERO(&readfds);       
        FD_SET(sockfd, &readfds); 

        // i am using select to check if there is a connection waiting to be accepted
        // so i only call accept when a connection is actually available
        // otherwise there may be a case where i accept a connection and there was no space in my client queue in threadpool
        if (select(sockfd+1, &readfds, NULL, NULL, NULL) < 0){    
            printf("Error while doing select\n");                 
        }                                                         

        pthread_mutex_lock(&(threadPool->lock));
        // enqueue
        if ((threadPool->tail+1)%threadPool->clientqueue_length != threadPool->head){ // queue is not full
            int newfd;
            socklen_t client_sockaddr_length = sizeof(client_sockaddr);
            if ((newfd = accept(sockfd, (struct sockaddr*) &server_sockaddr, &client_sockaddr_length)) < 0){
                printf("Error in accepting\n");
            }
            if (threadPool->head == -1 && threadPool->tail == -1){ // queue was empty
                threadPool->head++;
            }
            threadPool->tail = (threadPool->tail+1)%threadPool->clientqueue_length;
            threadPool->clientqueue[threadPool->tail] = newfd;
            pthread_cond_signal(&(threadPool->is_empty));
        } // else not enough space in the client queue right now

        pthread_mutex_unlock(&(threadPool->lock));
    }

    for (int i = 0; i < threadPool->threadcount; i++)
    {
        if (pthread_join(threadPool->threads[i], NULL) != 0)
        {
            printf("Error while joining threads\n");
        }
    }

    free(threadPool->threads);
    free(threadPool->clientqueue);
    free(threadPool);
    threadPool = NULL;

    close (sockfd);
    return NULL;
}










// give any 1 argument to make this node bootstrap
// usage 1-> to make own bootstrap, 2-> to connect to an other bootstrap
// ./DHT bootstrap <self_port>
// ./DHT <bootstrap_ip> <bootstrap_port> <self_port>
int main(int argc, char **argv) {
    // unsigned char hash1[5] = {'a', 'a', 'a', 'a', 'a'};
    // unsigned char hash2[5] = {'a', 'a', 'a', 'b', 'a'};
    // printf("%u\n", xor_distance(hash1, hash2));

    if (argc < 2 || argc > 4) {
        printf("Usage: \n./DHT bootstrap <self_port> \nor \n./DHT <bootstrap_ip> <bootstrap_port> <self_port> \n");
        return -1;
    }

    if (argc == 3 && strcmp(argv[1], "bootstrap") == 0) {
        bootstrap = 1;
        self_port = atoi(argv[2]);
    } else if (argc == 4) {
        bootstrap_ip = argv[1];
        bootstrap_port = atoi(argv[2]);
        self_port = atoi(argv[3]);
    }

    // initialize the routing table
    for (int i=0; i<ROUTING_TABLE_SIZE; i++){
        routing_table[i] = NULL;
    }

    if (bootstrap){
        // generate your own id
        generate_id(id);

    } else {
        // request id from a bootstrap node
        request_id(bootstrap_ip, bootstrap_port, id);
    }

    printf("Got Id: ");
    for (int i=0; i<SHA_DIGEST_LENGTH; i++)
        printf("%c", id[i]);
    printf("\n");


    pthread_t server_thread;
    // sedrvedr args- port, threadcount and length of client queue
    int server_args[] = {5000, 10, 10000};
    pthread_create(&server_thread, NULL, start_server, (void *) &server_args);

    start_client("127.0.0.1", 5000);
    sleep(1);
    start_client("127.0.0.1", 5000);
    sleep(1);
    start_client("127.0.0.1", 5000);

    pthread_join(server_thread, NULL);
    return 0;
}






// int h(char *data, unsigned char hash[SHA_DIGEST_LENGTH]) {
//     SHA1((unsigned char *)data, strlen(data), hash);
// }