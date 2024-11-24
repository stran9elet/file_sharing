#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include "hashtable.h"
// hash table is an array of linked lists


struct hashtable *create_hashtable(unsigned int capacity) {
    struct hashtable *table = (struct hashtable *) malloc(sizeof(struct hashtable));
    table->capacity = capacity;
    table->size = 0;
    table->arr = (struct pair **) malloc(sizeof(struct pair *) * table->capacity);

    for (unsigned int i = 0; i < table->capacity; i++) {
        table->arr[i] = NULL; // Initialize each slot to NULL
    }
    return table;
}

void delete_hashtable(struct hashtable *table) {
    for (unsigned int i = 0; i < table->capacity; i++) {
        struct pair *cur = table->arr[i];
        struct pair *prev = cur;
        while (cur) {
            cur = cur->next;
            free(prev);
            prev = cur;
        }
    }
    free(table->arr);
    free(table);
}

// Hash function: Sum the bytes of the 20-byte SHA1 key and mod by the capacity, to create a key for the local hashtable
static unsigned int hash(const unsigned char *key, unsigned int capacity) {
    unsigned int hash_value = 0;
    for (size_t i = 0; i < SHA_DIGEST_LENGTH; i++) {
        hash_value += key[i];
    }
    return hash_value % capacity;
}

static void rehash(struct hashtable *table);

void insert(struct hashtable *table, const unsigned char *key, int value) {
    unsigned int index = hash(key, table->capacity);
    struct pair *prev, *cur;
    prev = cur = table->arr[index];
    int found = 0;

    while (cur) {
        if (memcmp(cur->key, key, SHA_DIGEST_LENGTH) == 0) {
            found = 1;
            break;
        }
        prev = cur;
        cur = cur->next;
    }

    // if key already not present, add a new pair
    if (!found) {
        struct pair *n = (struct pair *) malloc(sizeof(struct pair));
        memcpy(n->key, key, SHA_DIGEST_LENGTH);
        n->data = value;
        n->next = NULL;
        if (prev) {
            prev->next = n;
        } else {
            table->arr[index] = n;
        }

        table->size++;

        float load_factor = (float) table->size / table->capacity;
        if (load_factor > LOAD_FACTOR_THRESHOLD)
            rehash(table);
    } else { // update the already present pair
        cur->data = value;
    }
}

static void rehash(struct hashtable *table) {
    struct hashtable *newtable = create_hashtable(table->capacity * 2);

    for (unsigned int i = 0; i < table->capacity; i++) {
        struct pair *cur = table->arr[i];
        while (cur) {
            insert(newtable, cur->key, cur->data);
            cur = cur->next;
        }
    }

    for (unsigned int i = 0; i < table->capacity; i++) {
        struct pair *cur = table->arr[i];
        struct pair *prev = cur;
        while (cur) {
            cur = cur->next;
            free(prev);
            prev = cur;
        }
    }
    free(table->arr);

    table->arr = newtable->arr;
    table->capacity = newtable->capacity;

    free(newtable);
}

int get(struct hashtable *table, const unsigned char *key) {
    unsigned int index = hash(key, table->capacity);
    struct pair *cur = table->arr[index];
    while (cur) {
        if (memcmp(cur->key, key, SHA_DIGEST_LENGTH) == 0) {
            return cur->data;
        }
        cur = cur->next;
    }
    return INT_MAX; // Return the max value of int if key not found
}




// struct hashtable *create_hashtable(unsigned int capacity) {
//     struct hashtable *table = (struct hashtable *) malloc(sizeof(struct hashtable));
//     table->capacity = capacity;
//     table->size = 0;
//     table->arr = (struct pair **) malloc(sizeof(struct pair *) * table->capacity);

//     for (unsigned int i = 0; i < table->capacity; i++) {
//         table->arr[i] = NULL; // Initialize each slot to NULL
//     }
//     return table;
// }

// void delete_hashtable(struct hashtable *table){
//     for(unsigned int i=0; i<table->capacity; i++){
//         struct pair *cur = table->arr[i];
//         struct pair *prev = cur;
//         while (cur){
//             cur = cur->next;
//             free(prev->key);
//             free(prev);
//             prev = cur;
//         }
//     }
//     free(table->arr);
//     free(table);
// }

// // a simple hash function which adds the ascii values of all characters in string, and mods by hashtable capacity
// static unsigned int hash(const char *key, unsigned int capacity) {
//     unsigned int hash_value = 0;
//     while (*key) {
//         hash_value += *key++;
//     }
//     return hash_value % capacity;
// }

// static void rehash(struct hashtable *table);

// void insert(struct hashtable *table, char *key, int value){
//     unsigned int index = hash(key, table->capacity);
//     struct pair *prev, *cur;
//     prev = cur = table->arr[index];
//     int found = 0;
//     while(cur){
//         if (strcmp(cur->key, key) == 0){
//             found = 1;
//             break;
//         }
//         prev = cur;
//         cur = cur->next;
//     }

//     if (!found) {
//         struct pair *n = (struct pair *) malloc(sizeof(struct pair));
//         n->key = strdup(key);;
//         n->data = value;
//         n->next = NULL;
//         if (prev) {
//             prev->next = n;
//         } else {
//             table->arr[index] = n;
//         }

//         table->size++;
        
//         float load_factor = (float) table->size / table->capacity;
//         if (load_factor > LOAD_FACTOR_THRESHOLD)
//             rehash(table);
//     } else {
//         cur->data = value;
//     }
// }

// static void rehash(struct hashtable *table){

//     // create the new hashtable array
//     struct hashtable *newtable = create_hashtable(table->capacity*2);
    
//     for(unsigned int i=0; i<table->capacity; i++){
//         struct pair *cur = table->arr[i];
//         while (cur){
//             insert(newtable, cur->key, cur->data);
//             cur = cur->next;
//         }
//     }

//     // free the previous table's array
//     for(unsigned int i=0; i<table->capacity; i++){
//         struct pair *cur = table->arr[i];
//         struct pair *prev = cur;
//         while (cur){
//             cur = cur->next;
//             free(prev->key);
//             free(prev);
//             prev = cur;
//         }
//     }
//     free(table->arr);

//     // set it to new table's array
//     table->arr = newtable->arr;
//     table->capacity = newtable->capacity;

//     // free the newtable, but don't free its array
//     free(newtable);
// }

// int get(struct hashtable *table, const char *key){
//     unsigned int index = hash(key, table->capacity);
//     struct pair *cur = table->arr[index];
//     int value = 0;
//     while (cur){
//         if (strcmp(cur->key, key) == 0){
//             return cur->data;
//         }
//         cur = cur->next;
//     }
//     return UINT_MAX; // return the max value of unsigned int if key not found
// }


