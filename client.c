// the first thing every node sends is the message code, followed by his own source node id, and after that the actual message body

#include <openssl/sha.h>
#include "client.h"
#include "message_codes.h"
// #include "node.h"

# define K 20
extern unsigned char id[SHA_DIGEST_LENGTH];

// returns the socket fd of this connection
int connect_to_server(char *ip, unsigned short int port){
    int sock;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("Socket creation error\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0)
    {
        printf("Invalid address/ Address not supported\n");
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("Connection Failed\n");
        return -1;
    }

    return sock;
}


int send_credentials(int servfd, int msg_code, unsigned char *my_id){
    int ret = send(servfd, &msg_code, sizeof(int), 0);
    if (ret == -1)
    {
        printf("Failed to send message\n");
        return -1;
    }

    if (msg_code == MSG_GET_ID)
        return 0;

    
}


int net_request_id(char *bootstrap_ip, unsigned short int bootstrap_port, unsigned char id[SHA_DIGEST_LENGTH]){
    int servfd;
    if ((servfd = connect_to_server(bootstrap_ip, bootstrap_port)) < 0){
        printf("could not connect to server %s:%d", bootstrap_ip, bootstrap_port);
        return -1;
    }

    if (send_credentials(servfd, MSG_GET_ID, NULL) < 0){
        printf("Failed to send message credentials\n");
        return -1;
    }
    
    printf("Asking for id from bootstrap node...\n");

    int ret = recv(servfd, id, SHA_DIGEST_LENGTH, 0);
    if (ret == -1)
    {
        printf("Failed to receive message\n");
        return -1;
    }

    return ret;
}

// issue a find node request to a server on your network
int net_find_node(char *server_ip, unsigned short int server_port, unsigned char *hash, struct node k_closest[K]){
    for (int i=0; i<K; i++){
        intialize_node(&k_closest[i]);
    }

    int servfd;
    if ((servfd = connect_to_server(server_ip, server_port)) < 0){
        printf("could not connect to server %s:%d", server_ip, server_port);
        return -1;
    }

    if (send_credentials(servfd, MSG_FIND_NODE, id) < 0){
        printf("Failed to send message credentials\n");
        return -1;
    }

    int ret = send(servfd, hash, SHA_DIGEST_LENGTH * sizeof(char), 0);
    if (ret == -1)
    {
        printf("Failed to send message\n");
        return -1;
    }

    // return message format- 20 byte node id, then 4 byte node ip, then 1 byte node port
    // continue for k times

    for (int i=0; i<K; i++){
        ret = recv(servfd, k_closest[i].node_id, SHA_DIGEST_LENGTH * sizeof(char), 0);
        if (ret == -1)
        {
            printf("Failed to receive message\n");
            return -1;
        }

        for (int j=0; j<4; j++){
            ret = recv(servfd, &k_closest[i].ip[j], sizeof(char), 0);
            if (ret == -1)
            {
                printf("Failed to receive message\n");
                return -1;
            }
        }

        
        ret = recv(servfd, &k_closest[i].port, sizeof(unsigned short int), 0);
        if (ret == -1)
        {
            printf("Failed to receive message\n");
            return -1;
        }

    }

    return 0;

}





























int start_client(char *ip, int port)
{
    int sock, ret;
    struct sockaddr_in serv_addr;
    char *message = "hello";
    char buffer[BUF_SIZE] = {0};

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("Socket creation error\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0)
    {
        printf("Invalid address/ Address not supported\n");
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("Connection Failed\n");
        return -1;
    }

    ret = send(sock, message, strlen(message), 0);
    if (ret == -1)
    {
        printf("Failed to send message\n");
        return -1;
    }
    printf("Sent: %s\n", message);

    ret = recv(sock, buffer, BUF_SIZE, 0);
    if (ret == -1)
    {
        printf("Failed to receive message\n");
        return -1;
    }
    printf("Received: %s\n", buffer);

    close(sock);

    return 0;
}