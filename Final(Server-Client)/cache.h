#ifndef CACHE_H
#define CACHE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//The size that HashTable capacity will be set at the first initialization.
#define INITIAL_HASHTABLE_SIZE  32

//Value node of the HashTable
typedef struct CacheNode {
    int key;    //src
    int dest;   
    char* path;
} CacheNode;

//HashTable implementation of Caching
typedef struct CacheStruct {
    size_t capacity;    //Capacity of the HashTable
    size_t size;        //Number of used place in the table
    CacheNode** data;   //Array of (key,value) data
} CacheStruct;

CacheNode* create_CacheNode(int key, int dest, char* path);
CacheStruct* create_CacheStruct();
int hashCode(CacheStruct* st, int key);
int insert(CacheStruct* st, int key, int dest, char* path);
char* search(CacheStruct* st, int key, int dest);
int expand(CacheStruct* st);
void destroy_CacheStruct(CacheStruct** st);
#endif