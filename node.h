#include <netinet/in.h>

struct node {
    unsigned char node_id[20]; // the hashed node id of 20 bytes
    char ip[4]; // the address corresponding to this node id
    unsigned short port;
    struct node *next;
    // struct node *prev;
};

void intialize_node(struct node *n);
void copy_node(struct node *src, struct node *dest);
void break_links(struct node *arr, int size);