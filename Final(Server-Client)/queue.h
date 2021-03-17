#ifndef QUEUE_H
#define QUEUE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node_q {
	void* data;
	struct Node_q* next;
} Node_q;

typedef struct Queue {
	Node_q* front;	
	Node_q* rear;
	int size;
	size_t data_type_size;
} Queue;

Queue* create_queue(size_t data_type_size);
int enqueue(Queue* q, void* data, size_t size);
void* dequeue(Queue* q);
void display(Queue* q);
Node_q* get_front_data(Queue* q);
int isEmpty(Queue* q);
void destroy_queue(Queue** q);
#endif