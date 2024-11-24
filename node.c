#include <openssl/sha.h>
#include "node.h"

void intialize_node(struct node *n){
    for (int i=0; i<SHA_DIGEST_LENGTH; i++){
        n->node_id[i] = '\0';
    }

    for (int i=0; i<4; i++){
        n->ip[i] = '\0';
    }

    n->port = 0;
    n->next = NULL;
}

void copy_node(struct node *src, struct node *dest){
    for (int i=0; i<SHA_DIGEST_LENGTH; i++){
        dest->node_id[i] = src->node_id[i];
    }

    for (int i=0; i<4; i++){
        dest->ip[i] = src->ip[i];
    }
    dest->port = src->port;
    dest->next = src->next;
}

void break_links(struct node *arr, int size){
    for (int i=0; i<size; i++){
        arr[i].next = NULL;
    }
}