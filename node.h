#include <netinet/in.h>

struct node {
    unsigned char node_id; // the hashed node id
    struct sockaddr_in *node_addr; // the address corresponding to this node id
    struct node *next;
    // struct node *prev;
};