#include "stack.h"

Stack* create_stack() {
	Stack* s = (Stack*)malloc(sizeof(Stack));
	if (s == NULL) {
		return NULL;
	} else {
		s->top = NULL;
		s->size = 0;
		return s;
	}
}

int push(Stack* s, int data) {
	if (s == NULL) {
		return -1;
	} 
	Node_s* n = (Node_s*)malloc(sizeof(Node_s));
	if (n == NULL ) {
		return -1;
	}
	n->data = data;		 
	if (s->top == NULL) {
		n->next = NULL;
		s->top = n; 
	} else {
		n->next = s->top;
		s->top = n;
	}
	++(s->size);
	return 0;
}

Node_s* pop(Stack* s) {
	if (s == NULL) {
		return NULL;
	}
	Node_s* temp;
	if (s->top != NULL) {
		temp = s->top;
		s->top = s->top->next;
		--(s->size);
		return temp;
	} else {
		return NULL;
	}
}

void destroy_stack(Stack** s) {
	if (*s != NULL) {
		Node_s* temp;
		while ((*s)->top != NULL) {
			temp = (*s)->top;
			(*s)->top = (*s)->top->next;
			free(temp);
		}
		free(*s);
		*s = NULL;
	}
}