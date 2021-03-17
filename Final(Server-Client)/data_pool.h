#ifndef DATA_POOL_H
#define DATA_POOL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "queue.h"
#include "Graph.h"
#include "cache.h"
#include "print_log.h"

#define TRUE 'T'
#define FALSE 'F'

//Structure templates

typedef struct safe_cache {
	pthread_mutex_t c_lock;
	CacheStruct* cache;
} safe_cache;

typedef struct safe_queue { //A common synchronized queue structure
	pthread_mutex_t q_lock;
	pthread_cond_t cond_state;
	Queue* q;
} safe_queue;

typedef struct pthread_package {
	char* exit;
	int clientfd;
	safe_queue** avbl_queue;
	safe_cache** cache;
	Graph** graph;
} pthread_package;

//A trackable thread structure
typedef struct pthread_track {
	int id;
	char avail;
	pthread_t thread;
	pthread_mutex_t th_lock;
	pthread_cond_t avbl_state;
	pthread_package* pack;
} pthread_track;

typedef struct data_node {
	int size;
	int no_avail;
	pthread_track* threads;
	struct data_node* next;
} data_node;

typedef struct data_pool {
	char exit;
	int unique_thread_identification; 	// Used for Unique identification of the threads
	int total_size;
	int total_used;
	data_node* head;
	data_node* last;
	Graph* graph;
	safe_queue* avbl_queue;
	safe_cache* cache;
} data_pool;

typedef struct resizer_thread {
	pthread_t th;
	data_pool** thread_pool;
	pthread_mutex_t rs_lock;
	pthread_cond_t rs_cond;
	int max_size;
} resizer_thread;

data_pool* create_data_pool(int init_size);
data_node* create_data_node(int data_size, data_pool* dp);
pthread_track* get_thread(data_pool* dp, int id);
int enlarge(data_pool** dp, int size);
int destroy_pool(data_pool** dp);
void tokenize(char* str, int* src, int* dest);
#endif 