#ifndef STACK_H
#define STACK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node_s {
	int data;
	struct Node_s* next;
} Node_s;

typedef struct Stack {
	Node_s* top;
	int size;
} Stack;

Stack* create_stack();
int push(Stack* s, int data);
Node_s* pop(Stack* s);
void destroy_stack(Stack** s);
#endif