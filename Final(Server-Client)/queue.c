#include "queue.h"

Queue* create_queue(size_t data_type_size) {
	Queue* q = (Queue*)malloc(sizeof(Queue));
	if (q == NULL) {
		return NULL;
	} else {
		q->front = q->rear = NULL;
		q->size = 0;
		q->data_type_size = data_type_size;
		return q;
	}
}

int isEmpty(Queue* q) {
	return (q->rear == NULL);
}

int enqueue(Queue* q, void* data, size_t size) {
	if (q == NULL)
		return -1;

//	Initialize a new node
	Node_q* newNode = (Node_q*)malloc(sizeof(Node_q));
	if (newNode == NULL)
		return -1;

	newNode->data = malloc(q->data_type_size);
	if (newNode->data == NULL)
		return -1;

	memcpy(newNode->data, data, q->data_type_size);
	newNode->next = NULL;
	
//	Enqueue the new node to the queue
	if (q->rear != NULL) {
		q->rear->next = newNode;
	}
	q->rear = newNode;
	if (q->front == NULL) {
		q->front = q->rear;
	}
	++(q->size);
	return 0;
}

void* dequeue(Queue* q) {
	if (q == NULL || q->front == NULL)
		return NULL;

	void* deqData = malloc(sizeof(q->data_type_size));
	if (deqData == NULL)
		return NULL;

	Node_q* temp = q->front;
	q->front = q->front->next;
	temp->next = NULL;
	if (q->front == NULL) {
		q->rear = NULL;
	}
	memcpy(deqData, temp->data, q->data_type_size);
	free(temp->data);
	free(temp);
	--(q->size);
	return deqData;
}

Node_q* get_front_data(Queue* q) {
	if (q->front != NULL) {
		return q->front;
	} else {
		return NULL;
	}
}

void display(Queue* q) {
	if (q->front != NULL) {
		Node_q* temp = q->front;
		
		do {
			printf("Data: %p\n", temp->data);
			temp = temp->next;
		} while (temp != NULL);
	}
}

void destroy_queue(Queue** q) {
	if (*q != NULL) {
		Node_q* temp;
		if (!isEmpty(*q)) {
			while ((*q)->front != NULL) {
				temp = (*q)->front;
				(*q)->front = (*q)->front->next;
				free(temp);
			}
			(*q)->rear = NULL;
			(*q)->size = 0;
		}
		free(*q);
		*q = NULL;
	}
}