// #include <stdio.h>         
// #include <string.h>
// #include <stdlib.h>
// #include <unistd.h>
// #include <arpa/inet.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <sys/select.h>  

// #include <pthread.h>

// #define BUF_SIZE 1024

// struct ThreadPool {
//     pthread_t *threads;
//     int *clientqueue; // a circular queue of client fds
//     long clientqueue_length;
//     long threadcount;
//     pthread_mutex_t lock;
//     pthread_cond_t is_empty;
//     long head;
//     long tail; // here tail represents index of last element
// };


// void *start_server(void *server_args);
// // takes an array of 3 values as argument- server port, threadcount, clientqueue length