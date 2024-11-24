// this is a sample client program which can use the DHT server, assuming you have multiple server nodes running, and you know the ip address of atleast one of them
#include <stdio.h>
#include "client_helper.h"

int main(){
    char *server_ip = "127.0.0.1";
    unsigned short int server_port = 30000;

    char *key = "example key";
    int value = 5;

    net_insert(server_ip, server_port, key, value);
    value = net_get(server_ip, server_port, key);
    printf("%d\n", value);
}