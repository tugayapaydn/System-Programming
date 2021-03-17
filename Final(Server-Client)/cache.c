#include "cache.h"

//Creates and initializes a new CacheNode
CacheNode* create_CacheNode(int key, int dest, char* path) {
//  Create a new value node
    CacheNode* new = (CacheNode*)malloc(sizeof(CacheNode));
    if (new == NULL)  //An error set to stderr
        return NULL;

//  Initialize the value(CacheNode) node
    new->key = key;
    new->dest = dest;
    new->path = strdup(path);
    if (new->path == NULL) { //An error set to stderr
        free(new);  //Free allocated resources
        return NULL;
    }
    return new;
}

//Creates and initializes a new CacheStruct
CacheStruct* create_CacheStruct() {
//  Create cache structure    
    CacheStruct* st = (CacheStruct*)malloc(sizeof(CacheStruct));
    if (st == NULL) //An error set to stderr
        return NULL;

//  Initialize cache structure
    st->capacity = INITIAL_HASHTABLE_SIZE;
    st->size = 0;
    st->data = (CacheNode**)calloc(st->capacity, sizeof(CacheNode*));
    if (st->data == NULL) { //An error set to stderr
        free(st);   //Free allocated resources
        return NULL;
    }
    return st;
}

//Returns the hashCode of the given key
int hashCode(CacheStruct* st, int key) {
    return key % st->capacity;
}

//Inserts a new data to the table
int insert(CacheStruct* st, int key, int dest, char* path) {
    if (st == NULL || path == NULL)
        return -1;

//  If %75 of the table is filled, table will be expanded
    if (st->size > ((st->capacity * 3) / 4)) {
        expand(st);
        printf("Expanded: %ld\n" ,st->capacity);
    } 

//  Create a new Node
    CacheNode* newNode = create_CacheNode(key, dest, path);
    if (newNode == NULL)
        return -1;

//  Get the hashCode
    int index = hashCode(st, key);
    while (st->data[index] != NULL) {
//      If element is already in the table, it will not be inserted.
        if (st->data[index]->key == key && st->data[index]->dest == dest) {
            return 0;
        }
        index = (index + 1) % st->capacity;
    }
    st->data[index] = newNode;
    (st->size)++;
    return 0;
}

//Searches if a given (key,dest) pair is in the table
char* search(CacheStruct* st, int key, int dest) {
    if (st == NULL)
        return NULL;

    int index = hashCode(st, key);
    //printf("Searching data hash: %d\n" ,index);
//  Keep looking until a NULL data is found
//  HashTable must not be fully filled
    while (st->data[index] != NULL) {
    //    printf("%s %d %d\n" ,st->data[index]->path, st->data[index]->key, st->data[index]->dest);
        if (st->data[index]->key == key && st->data[index]->dest == dest) {
//          If a data is found, return a pointer to it
            return st->data[index]->path;
        }
        index = (index + 1) % st->capacity;
    }
    return NULL;
}

//If HashTable is filled up to a limit, it will be expanded by using expand() function
int expand(CacheStruct* st) {
    int old_capacity = st->capacity;
    st->capacity *= 2;
//  Create a new data array
    CacheNode** newHash = (CacheNode**)calloc(st->capacity, sizeof(CacheNode*));
    if (newHash == NULL) 
        return -1;

//  Hold old HashTable information and set the new HashTable to the CacheStructure
    CacheNode** oldHash = st->data;
    st->data = newHash;
    st->size = 0;

//  Insert all keys in the old HashTable to the new HashTable 
    int i;
    for (i = 0; i < old_capacity; i++) {
        if (oldHash[i] != NULL) {
            insert(st, oldHash[i]->key, oldHash[i]->dest, oldHash[i]->path);
        }
    }
//  Deallocate old HashTable
    free(oldHash);
    return 0;
}

void destroy_CacheStruct(CacheStruct** st) {
    if ((*st) != NULL && (*st)->data != NULL) {
        size_t i = 0;
        for (i = 0; i < (*st)->capacity; i++) {
            if ((*st)->data[i] != NULL) {
                free((*st)->data[i]->path);
                free((*st)->data[i]);
            }
        }
        free((*st)->data);
        free((*st));
        *st = NULL;
    }
}