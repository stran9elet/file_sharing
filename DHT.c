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
#include "hashtable.h"
// #include "node.h"

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
pthread_mutex_t routing_table_lock = PTHREAD_MUTEX_INITIALIZER;

struct ThreadPool {
    pthread_t *threads;
    int *clientqueue; // a circular queue of client fds
    struct sockaddr_in **clientaddqueue; // will store socket addresses of clients in client queue
    long clientqueue_length;
    long threadcount;
    pthread_mutex_t lock;
    pthread_cond_t is_not_empty;
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


// find the k nearest nodes to a given hash from our own routing table
// return will be an array of atmost k nodes sorted from closest to farthest
// use lock when accessing the routing table
int find_node(unsigned char *hash, struct node k_closest[K]){
    printf("Finding locally k closest nodes in the routing table\n");
    unsigned char *distances[K]; // an array of size k
    // each element is a pointer to an array of unsigned char distances of length SHA_DIGEST_LENGTH
    for (int i = 0; i < K; i++) {
        intialize_node(&k_closest[i]);
        distances[i] = NULL;
    }

    unsigned char *dist;
    struct node myself;
    intialize_node(&myself);
    for (int i=0; i<SHA_DIGEST_LENGTH; i++)
        myself.node_id[i] = id[i]; // not adding the IP address because OS doesn't know self's IP. So the calling node will have to determine that from his own routing table since he called me directly and must've known my address    
    struct node *cur;

    int last_idx = 0;

    // compare your own id as well
    dist = (unsigned char*) malloc(SHA_DIGEST_LENGTH * sizeof(unsigned char));
    xor_distance(hash, id, dist);
    distances[0] = dist;
    copy_node(&myself, &k_closest[0]);

    // check your own routing table to find atmost the k closest nodes you know to the hash
    pthread_mutex_lock(&routing_table_lock);
    for (int i=0; i<ROUTING_TABLE_SIZE; i++){

        if (routing_table[i] == NULL)
            continue;

        // need to get lock to make sure tha when i am reading the routing table, some other thread doosn't modify it
        cur = routing_table[i];


        for (int j=0; j<K && cur != NULL; j++){
            dist = (unsigned char*) malloc(SHA_DIGEST_LENGTH * sizeof(unsigned char));
            xor_distance(hash, cur->node_id, dist);

            int insert_pos = 0;
            for (insert_pos=0; insert_pos <= last_idx; insert_pos++){
                if (cmp_distance(dist, distances[insert_pos]) <= 0)
                    break;
            }

            if (insert_pos == K)
                free(dist);
            else if (insert_pos < K){
                if (last_idx == K-1)
                    free(distances[K-1]);
                else
                    last_idx++;

                for (int l=last_idx; l>insert_pos; l--){
                    distances[l] = distances[l-1];
                    copy_node(&k_closest[l-1], &k_closest[l]);
                }
                distances[insert_pos] = dist;
                copy_node(cur, &k_closest[insert_pos]);
            }

            cur = cur->next;
        }
    }
    pthread_mutex_unlock(&routing_table_lock);


    for (int i=0; i<K; i++){
        if (distances[i] != NULL)
            free(distances[i]);
    }

    break_links(k_closest, K);
    printf("Found\n");
    return last_idx+1;
}



// return 1 if found, else 0
int find_k_closest_worker(unsigned char *hash, struct node k_closest[K]){
    printf("Finding globally k closest nodes\n");
    struct node new_k_closest[K];

    // this DHT assumes nodes are not constantly leaving the network. That's why i assume k_closest[0] node was active and didn't try further nodes
    if (k_closest[0].port == 0)
        return 0; // didn't find any k closest node

    char ip[16]; // Buffer to store the dotted-decimal format string
    sprintf(ip, "%d.%d.%d.%d", k_closest[0].ip[0], k_closest[0].ip[1], k_closest[0].ip[2], k_closest[0].ip[3]);
    net_find_node(ip, k_closest[0].port, hash, new_k_closest);

    // if new k closest same as old k closest, copy new_k_closest to k_closest and return 1
    int global_found = 1;
    for (int i=0; i<K; i++){
        if (!cmp_distance(k_closest[i].node_id, new_k_closest[i].node_id) == 0){
            // different nodes
            global_found = 0;
            break;
        }
    }

    if (global_found)
        return 1;
    else{
        int ret = find_k_closest_worker(hash, new_k_closest);
        if (ret) {
            // found the global k closest nodes
            // copy new k closest array to old k closest
            for (int i=0; i<K; i++){
                k_closest[i] = new_k_closest[i];
            }
            return 1;
        }
    }
    return 0;
}


// recursively call find_node to find the globally closest k nodes to a given hash
// first find the closest node to that hash in your routing table
// then recursively query closest nodes received with find node
void find_k_closest(unsigned char *hash, struct node k_closest_global[K]) {
    find_node(hash, k_closest_global);
    for (int i=0; i<K; i++){
        printf("port: %d\n", k_closest_global[i].port);
    }
    find_k_closest_worker(hash, k_closest_global);
    for (int i=0; i<K; i++){
        printf("port: %d\n", k_closest_global[i].port);
    }
}



void print_hash(unsigned char *hash){
    for (int i=0; i<20; i++){
        printf("%u ", hash[i]);
    }
    printf("\n");
}


void generate_id(unsigned char *id){
    srand(time(NULL));

    while (1){
        // generate a random id, and check if its not taken by another node
        for (int i=0; i<SHA_DIGEST_LENGTH; i++){
            id[i] = rand() & 0xFF;
        }

        printf("Generated id: ");
        print_hash(id);

        // find out which nodes are closest to given id
        // if our id is already a part of the array received, then generate a new id
        int already_exists = 0;
        struct node k_nearest[K];
        for(int i=0; i<K; i++){
            intialize_node(&k_nearest[i]);
        }
        printf("Finding k closest nodes to generate id...\n");
        find_k_closest(id, k_nearest);

        // because k_nearest[0] will always be my own node id
        if (cmp_distance(id, k_nearest[1].node_id) != 0){
            break;
        }

        printf("%d\n", already_exists);
        // sleep(10);
    }
}


int common_prefix_length(unsigned char *hash1, unsigned char *hash2) {
    int common_bits = 0;

    for (int byte_idx = 0; byte_idx < SHA_DIGEST_LENGTH; byte_idx++) {
        unsigned char xor_byte = hash1[byte_idx] ^ hash2[byte_idx]; // XOR to find differing bits

        // Check if all bits in this byte are the same
        if (xor_byte == 0) {
            common_bits += 8; // All 8 bits are the same
        } else {
            // Count common bits in the current byte
            for (int bit = 7; bit >= 0; bit--) {
                if (xor_byte & (1 << bit)) {
                    return common_bits; // Return if a differing bit is found
                }
                common_bits++;
            }
        }
    }

    return common_bits; // Return the total common bits (maximum 160)
}

void update_routing_table(unsigned char *node_id, char *node_ip, unsigned short int node_port){
    // in kademlia, you would first ping the least recently seen node
    // if that node responds, then you discard this new node
    // if it doesn't then you put this new node as the most recently seen node, and discard that old one
    // but i'm just gonna directly put this node as the most recently seen node
    if (cmp_distance(node_id, id) == 0){
        return;
    }

    int common_prefix = common_prefix_length(node_id, id);

    pthread_mutex_lock(&routing_table_lock);
    // check if node id already in bucket
    struct node *cur = routing_table[common_prefix];
    for (int i=0; i<K; i++){
        if (cur == NULL)
            break;
        if (cmp_distance(cur->node_id, node_id) == 0)
            return;
    }

    // shift each node in k bucket 1 step ahead
    struct node *newnode = (struct node *) malloc(sizeof(struct node));
    for (int i=0; i<SHA_DIGEST_LENGTH; i++){
        newnode->node_id[i] = node_id[i];
    }
    sscanf(node_ip, "%d.%d.%d.%d", (int *) &newnode->ip[0], (int *) &newnode->ip[1], (int *) &newnode->ip[2], (int *) &newnode->ip[3]);
    newnode->port = node_port;

    newnode->next = routing_table[common_prefix];
    routing_table[common_prefix] = newnode;

    cur = newnode;
    for (int i=1; i<K; i++){
        if (cur->next == NULL)
            break;
        cur = cur->next;
    }

    if (cur->next != NULL){
        free(cur->next);
        cur->next = NULL;
    }

    pthread_mutex_unlock(&routing_table_lock);

}



// used by a node to first find its k nearest nodes, then ping them so that they add her to their routing tables, and return a response so that i can add them to my routing table.
void announce(char *bootstrap_ip, unsigned short int bootstrap_port){
    struct node k_closest[K];
    net_find_node(bootstrap_ip, bootstrap_port, id, k_closest);

    // this will now populate the k_closest list with the global k closest nodes
    find_k_closest_worker(id, k_closest);


    for (int i=0; i<K; i++){
        if (k_closest[i].port == 0) // to not ping empty node or myself
            continue;
        char ip[16]; // Buffer to store the dotted-decimal format string
        sprintf(ip, "%d.%d.%d.%d", k_closest[0].ip[0], k_closest[0].ip[1], k_closest[0].ip[2], k_closest[0].ip[3]);
        net_ping(ip, k_closest[i].port, id);
    }
}

void store(){
    // find the nearest node ids in my routing table to a given hash, and issue store on them
}


// Use locks when accessing routing table, and other global variables
void* thread_handler(void *pool){
    struct ThreadPool *threadPool = (struct ThreadPool*) pool; 

    while (1){ // this loop will continuously read client from taskqueue and execute
        pthread_mutex_lock(&(threadPool->lock));
        if (threadPool->head == -1 && threadPool->tail == -1) { // queue is empty
            // no data to read- put the thread to sleep
            pthread_cond_wait(&(threadPool->is_not_empty), &(threadPool->lock)); // will put this thread on sleep and unlock the mutex lock for others to use
            pthread_mutex_unlock(&(threadPool->lock));
            continue;
        }

        // dequeue
        int newfd = (threadPool->clientqueue)[threadPool->head];
        struct sockaddr_in *client_addr = (threadPool->clientaddqueue)[threadPool->head];
        threadPool->head++;
        if (threadPool->head == threadPool->tail+1){ // queue got empty
            threadPool->head = -1;
            threadPool->tail = -1;
        }
        threadPool->head = threadPool->head % (threadPool->clientqueue_length);

        pthread_mutex_unlock(&(threadPool->lock));

        // the messages we receive will first contain a message code
        // followed by that client's node id (except for in case of MSG_GET_ID)
        // and then the actual message

        int rbytes;
        int msg_code;
        if ((rbytes = recv(newfd, &msg_code, sizeof(int), 0)) < 0){
            printf("Error while receiving");
            return NULL;
        }

        printf("Received: %d\n", msg_code);

        unsigned char node_id[SHA_DIGEST_LENGTH];

        if (msg_code != MSG_GET_ID){
            // get the node id
            if ((rbytes = recv(newfd, node_id, SHA_DIGEST_LENGTH*sizeof(char), 0)) < 0){
                printf("Error while receiving");
                return NULL;
            }
        }

        switch (msg_code) {
            case MSG_GET_ID:
                generate_id(node_id);
                if (send(newfd, node_id, sizeof(node_id), 0) < 0){
                    printf("Error while sending id");
                }
                break;

            case MSG_FIND_NODE:
                unsigned char hash[SHA_DIGEST_LENGTH];
                int ret = recv(newfd, hash, SHA_DIGEST_LENGTH * sizeof(char), 0);
                if (ret == -1)
                {
                    printf("Failed to receive message\n");
                    return NULL;
                }

                struct node k_closest[K];
                find_node(hash, k_closest);

                for (int i=0; i<K; i++){
                    ret = send(newfd, k_closest[i].node_id, SHA_DIGEST_LENGTH * sizeof(char), 0);
                    if (ret == -1)
                    {
                        printf("Failed to receive message\n");
                        return NULL;
                    }

                    for (int j=0; j<4; j++){
                        ret = send(newfd, &k_closest[i].ip[j], sizeof(char), 0);
                        if (ret == -1)
                        {
                            printf("Failed to receive message\n");
                            return NULL;
                        }
                    }

                    ret = send(newfd, &k_closest[i].port, sizeof(unsigned short int), 0);
                    if (ret == -1)
                    {
                        printf("Failed to receive message\n");
                        return NULL;
                    }
                }
                break;

            case MSG_PING: // just pong back
                net_pong(inet_ntoa(client_addr->sin_addr), client_addr->sin_port, id);
            break;

            case MSG_PONG: // do nothing, routing table will be automatically updated later
            break;

            default:
                char *msg = "error";
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

        update_routing_table(node_id, inet_ntoa(client_addr->sin_addr), client_addr->sin_port);
        free(client_addr);
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
    pthread_cond_init(&(threadPool->is_not_empty), NULL);

    threadPool->clientqueue = (int *) malloc(threadPool->clientqueue_length * sizeof(int));
    threadPool->clientaddqueue = (struct sockaddr_in **) malloc(threadPool->clientqueue_length * sizeof(struct sockaddr_in));

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
            struct sockaddr_in *client_sockaddr = (struct sockaddr_in *) malloc(sizeof(struct sockaddr_in));
            socklen_t client_sockaddr_length = sizeof(client_sockaddr);
            if ((newfd = accept(sockfd, (struct sockaddr*) &client_sockaddr, &client_sockaddr_length)) < 0){
                printf("Error in accepting\n");
            }
            if (threadPool->head == -1 && threadPool->tail == -1){ // queue was empty
                threadPool->head++;
            }
            threadPool->tail = (threadPool->tail+1)%threadPool->clientqueue_length;
            threadPool->clientqueue[threadPool->tail] = newfd;
            threadPool->clientaddqueue[threadPool->tail] = client_sockaddr;
            pthread_cond_signal(&(threadPool->is_not_empty));
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
    printf("Initializeing routing table\n");
    for (int i=0; i<ROUTING_TABLE_SIZE; i++){
        routing_table[i] = NULL;
    }

    if (bootstrap){
        // generate your own id
        printf("Generating ID\n");
        unsigned char tempid[SHA_DIGEST_LENGTH];
        generate_id(tempid);
        for (int i=0; i<SHA_DIGEST_LENGTH; i++){
            id[i] = tempid[i];
        }
    } else {
        // request id from a bootstrap node
        printf("Requesting ID from bootstrap\n");
        net_request_id(bootstrap_ip, bootstrap_port, id);
        announce(bootstrap_ip, bootstrap_port); // ping the k closest nodes about my presence, to update their and my routing tables
    }

    printf("Got Id: ");
    print_hash(id);
    printf("\n");


    pthread_t server_thread;
    // server args- port, threadcount and length of client queue
    int server_args[] = {5000, 10, 10000};
    pthread_create(&server_thread, NULL, start_server, (void *) &server_args);

    // start_client("127.0.0.1", 5000);
    // sleep(1);
    // start_client("127.0.0.1", 5000);
    // sleep(1);
    // start_client("127.0.0.1", 5000);

    pthread_join(server_thread, NULL);
    return 0;
}






// int h(char *data, unsigned char hash[SHA_DIGEST_LENGTH]) {
//     SHA1((unsigned char *)data, strlen(data), hash);
// }