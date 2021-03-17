#ifndef GRAPH_H
#define GRAPH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "queue.h"
#include "stack.h"

#define PERM (S_IRUSR)

/*A graph implementation by using adjacency list approach.*/

//Edge representation
typedef struct Edge {
	int dest;
	struct Edge* next;
} Edge;

//Linked List representation of the edges
typedef struct EdgeList {
	Edge* head;
} EdgeList;

//Graph representation
typedef struct Graph {
	int numV;
	struct EdgeList* vertices;
} Graph;

Graph* loadGraphFromFile(char* file);
Graph* create_graph(int numV);
int insert_edge(Graph* graph, int src, int dest);
char* path(const Graph* graph, int src, int dest);
int* BFS(const Graph* graph, int start);
int destroy_graph(Graph** graph);
#endif