// #define INIT_CAPACITY 16
// #define LOAD_FACTOR_THRESHOLD 0.75

// struct pair {
//     char *key;
//     int data;
//     struct pair *next;
// };

// struct hashtable {
//     unsigned int size; // no. of elements in hash table
//     unsigned int capacity; // no. of slots in hash table
//     struct pair **arr;
// };

// struct hashtable *create_hashtable(unsigned int capacity);

// void delete_hashtable(struct hashtable *table);

// void insert(struct hashtable *table, char *key, unsigned int value);

// unsigned int get(struct hashtable *table, const char *key); // return UINT_MAX if key not found



#include <stdint.h>
#include <openssl/sha.h>

#define INIT_CAPACITY 16
#define LOAD_FACTOR_THRESHOLD 0.75

struct pair {
    unsigned char key[SHA_DIGEST_LENGTH]; // 20-byte SHA1 hash key
    int data;
    struct pair *next;
};

struct hashtable {
    unsigned int size;       // Number of elements in the hash table
    unsigned int capacity;   // Number of slots in the hash table
    struct pair **arr;       // Array of linked list heads
};

struct hashtable *create_hashtable(unsigned int capacity);

void delete_hashtable(struct hashtable *table);

void insert(struct hashtable *table, const unsigned char *key, int value);

int get(struct hashtable *table, const unsigned char *key); // Return UINT_MAX if key not found
