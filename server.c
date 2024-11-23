// // a multi-threaded server implementation
// #include "server.h"

// void* thread_handler(void *pool){
//     struct ThreadPool *threadPool = (struct ThreadPool*) pool; 

//     while (1){ // this loop will continuously read client from taskqueue and execute
//         pthread_mutex_lock(&(threadPool->lock));
//         if (threadPool->head == -1 && threadPool->tail == -1) { // queue is empty
//             // no data to read- put the thread to sleep
//             pthread_cond_wait(&(threadPool->is_empty), &(threadPool->lock)); // will put this thread on sleep and unlock the mutex lock for others to use
//             pthread_mutex_unlock(&(threadPool->lock));
//             continue;
//         }

//         // dequeue
//         int newfd = (threadPool->clientqueue)[threadPool->head];
//         threadPool->head++;
//         if (threadPool->head == threadPool->tail+1){ // queue got empty
//             threadPool->head = -1;
//             threadPool->tail = -1;
//         }
//         threadPool->head = threadPool->head % (threadPool->clientqueue_length);

//         pthread_mutex_unlock(&(threadPool->lock));


//         // parse the message and send the response
//         // char buffer[BUF_SIZE] = {0};
//         int rbytes;
//         int msg_code;
//         if ((rbytes = recv(newfd, msg_code, sizeof(int), 0)) < 0){
//             printf("Error while receiving");
//             return NULL;
//         }

//         printf("Received: %d\n", msg_code);

//         char *msg = "world";
//         if (send(newfd, msg, strlen(msg), 0) < 0){
//             printf("Error while sending");
//             return NULL;
//         }
//         printf("Sent: %s\n", msg);

//         if (close(newfd) < 0){
//             printf("Error in closing newfd"); 
//             return NULL;
//         }
//     }
// }





// void *start_server(void *server_args){

//     int  *args = (int *) server_args;
//     int port = args[0];
//     int threadcount = args[1];
//     int l_clientqueue = args[2];

//     int sockfd;
//     if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
//         printf("Error in creating socket\n");
//         return NULL;
//     }

//     struct sockaddr_in server_sockaddr, client_sockaddr;
//     server_sockaddr.sin_family = AF_INET;
//     server_sockaddr.sin_addr.s_addr = INADDR_ANY;
//     server_sockaddr.sin_port = htons(port);

//     if (bind(sockfd, (struct sockaddr*) &server_sockaddr, sizeof(server_sockaddr)) < 0){
//         printf("Error in binding socket\n");
//         return NULL;
//     }

//     if (listen(sockfd, SOMAXCONN) < 0){
//         printf("Error in listening\n");
//         return NULL;
//     }

//     struct ThreadPool *threadPool = (struct ThreadPool*) malloc(sizeof(struct ThreadPool));
//     threadPool->clientqueue_length = l_clientqueue; // 10000
//     threadPool->head = -1;
//     threadPool->tail = -1;
//     pthread_mutex_init(&(threadPool->lock), NULL);
//     pthread_cond_init(&(threadPool->is_empty), NULL);

//     threadPool->clientqueue = (int *) malloc(threadPool->clientqueue_length * sizeof(int));

//     threadPool->threadcount = threadcount;
//     threadPool->threads = (pthread_t*) malloc(threadPool->threadcount * sizeof(pthread_t));

//     for (int i=0; i<threadPool->threadcount; i++){
//         pthread_create(&(threadPool->threads[i]), NULL, &thread_handler, threadPool);
//     }

//     printf("Server listening on port %d\n", port);

//     fd_set readfds; 
//     while(1) {

//         FD_ZERO(&readfds);       
//         FD_SET(sockfd, &readfds); 

//         // i am using select to check if there is a connection waiting to be accepted
//         // so i only call accept when a connection is actually available
//         // otherwise there may be a case where i accept a connection and there was no space in my client queue in threadpool
//         if (select(sockfd+1, &readfds, NULL, NULL, NULL) < 0){    
//             printf("Error while doing select\n");                 
//         }                                                         

//         pthread_mutex_lock(&(threadPool->lock));
//         // enqueue
//         if ((threadPool->tail+1)%threadPool->clientqueue_length != threadPool->head){ // queue is not full
//             int newfd;
//             socklen_t client_sockaddr_length = sizeof(client_sockaddr);
//             if ((newfd = accept(sockfd, (struct sockaddr*) &server_sockaddr, &client_sockaddr_length)) < 0){
//                 printf("Error in accepting\n");
//             }
//             if (threadPool->head == -1 && threadPool->tail == -1){ // queue was empty
//                 threadPool->head++;
//             }
//             threadPool->tail = (threadPool->tail+1)%threadPool->clientqueue_length;
//             threadPool->clientqueue[threadPool->tail] = newfd;
//             pthread_cond_signal(&(threadPool->is_empty));
//         } // else not enough space in the client queue right now

//         pthread_mutex_unlock(&(threadPool->lock));
//     }

//     for (int i = 0; i < threadPool->threadcount; i++)
//     {
//         if (pthread_join(threadPool->threads[i], NULL) != 0)
//         {
//             printf("Error while joining threads\n");
//         }
//     }

//     free(threadPool->threads);
//     free(threadPool->clientqueue);
//     free(threadPool);
//     threadPool = NULL;

//     close (sockfd);
//     return NULL;
// }
