#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <openssl/sha.h>

#include "node.h"


#define BUF_SIZE 1024
#define K 20

int start_client(char *server_ip, int server_port);
int net_request_id(char *bootstrap_ip, unsigned short int bootstrap_port, unsigned char id[SHA_DIGEST_LENGTH]);
int net_find_node(char *server_ip, unsigned short int server_port, unsigned char *hash, struct node k_closest[K]);
int net_ping(char *server_ip, unsigned short int server_port);
int net_pong(char *server_ip, unsigned short int server_port);
int net_store(char *server_ip, unsigned short int server_port, unsigned char *key, int value);
int net_load(char *server_ip, unsigned short int server_port, unsigned char *key);
int net_insert(char *server_ip, unsigned short int server_port, char *hashkey, int value);
int net_get(char *server_ip, unsigned short int server_port, char *hashkey);