#include <openssl/sha.h>
#include "client.h"
#include "message_codes.h"

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



int request_id(char *bootstrap_ip, unsigned short int bootstrap_port, unsigned char id[SHA_DIGEST_LENGTH]){
    int servfd;
    if ((servfd = connect_to_server(bootstrap_ip, bootstrap_port)) < 0){
        printf("could not connect to server %s:%d");
        return -1;
    }
    
    int msg_code = MSG_GET_ID;
    int ret = send(servfd, &msg_code, sizeof(int), 0);
    if (ret == -1)
    {
        printf("Failed to send message\n");
        return -1;
    }
    printf("Asking for id from bootstrap node...\n");

    ret = recv(servfd, id, SHA_DIGEST_LENGTH, 0);
    if (ret == -1)
    {
        printf("Failed to receive message\n");
        return -1;
    }

    return ret;
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