#include "Graph.h"

/*Creates a new graph with number of "numV" vertices*/
Graph* create_graph(int numV) {
	Graph* graph = (Graph*)malloc(sizeof(Graph));
	if (graph == NULL) {
		perror("Failed to initialize graph!");
		return NULL;
	}
	graph->numV = numV;
	graph->vertices = (EdgeList*)malloc(numV * sizeof(EdgeList));
	if (graph->vertices == NULL) {
		perror("Failed to initialize graph!");
		return NULL;
	}
	return graph;
}

/*Inserts a new edge to a specific graph*/
int insert_edge(Graph* graph, int src, int dest) {
	//Check if source or destination is out of range
	if (src >= graph->numV || dest >= graph->numV) {
		return -1;
	} else {
		Edge* newEdge = (Edge*)malloc(sizeof(Edge));
		newEdge->dest = dest;
		//Insert the edge
		newEdge->next = graph->vertices[src].head;
		graph->vertices[src].head = newEdge;
		return 0;
	}
}

char* path(const Graph* graph, int src, int dest) {
	if (graph == NULL || src < 0 || dest < 0 || src >= graph->numV || dest >= graph->numV) {
		return NULL;
	}
	int* parent = BFS(graph, src);
	Stack* s = create_stack();
	
	int prev = dest;
//	Destination should be pushed before loop but in order to create 
//	a nice ordered path string, it will not be pushed
	while (parent[prev] != -1) {
//	Push the previous neighbour to the stack
		push(s, parent[prev]);
//	Set prev to the previous neighbour
		prev = parent[prev];
	}
	if (s->size <= 0) {
		return NULL;
	}

	char numb[20] = {'\0'};
	char* path = (char*)malloc(s->size * 20);
	strcpy(path, "");

	Node_s* i = pop(s);
	while (i != NULL) {
		if (snprintf(numb, 19, "%d",i->data) < 0) {
			return NULL;
		}
		strcat(path, numb);
		strcat(path, "->");
		i = pop(s);
	}
	if (snprintf(numb, 19, "%d",dest) < 0) {
		return NULL;
	}
	strcat(path, numb);
	free(parent);
	destroy_stack(&s);
	return path;
}

int* BFS(const Graph* graph, int start) {
    int numV;
	if (graph == NULL || start < 0 || start >= (numV = graph->numV)) {
		return NULL;
	}
    Queue* q;
    char* identified;
    int* parent;
    int data_size = sizeof(int);
//  Creating Queue
    q = create_queue(data_size);
//  Identification array holds visited node data
    if ((identified = (char*)malloc(numV * sizeof(char))) == NULL
    		|| (parent = (int*)malloc(numV * sizeof(int))) == NULL) {
        perror("BreadFirstSearch Failure!");
        return NULL;
    }
    memset(identified, 'F', numV);
    for (int i = 0; i < numV; ++i) {
    	parent[i] = -1;
    }
    
//  Start from the source "start" vertex
    enqueue(q, &start, data_size);
    identified[start] = 'T';
    
    while (!isEmpty(q)) {
        int* cur = (int*)dequeue(q);
        Edge* e = graph->vertices[*cur].head;
        while (e != NULL) {
            int neighbor = e->dest;
//			If neighbor has not been identified
            if (identified[neighbor] == 'F') {
//				Mark it identified
                identified[neighbor] = 'T';
//				Place it into the Queue
                enqueue(q, &neighbor, data_size);
//				Insert the edge into the array
                parent[neighbor] = *cur;
            }
            e = e->next;
        }    
        free(cur);
    }
    free(identified);
    destroy_queue(&q);
    return parent;
}

Graph* loadGraphFromFile(char* file) {
	int fd;
	if((fd = open(file, O_RDONLY, PERM)) == -1) {
		perror("Failed to open input file");
		return NULL;
	}

	char* keyword = "Nodes:";
	char buffw[100];
	char buffc[2] = {'\0'};
	char numv[20];
	int i = 0;
	int j = 0;

	while ((read(fd, buffc, 1) != -1)) {
		if ((buffc[0] == ' ') || (buffc[0] == '\n')) {
			buffw[i] = '\0';
			//printf("Next key: %s\n", buffw);
			if (strcmp(buffw, keyword) == 0) {
				j = 0;
				while (read(fd, buffc, 1) != -1 && buffc[0] != ' ') {
					numv[j++] = buffc[0];
				}
				while (read(fd, buffc, 1) != -1 && buffc[0] != '\n') {}
				break;
			} else {
				i = 0;
			}
		} else {
			buffw[i++] = buffc[0];
		}
	}
	while (read(fd, buffc, 1) != -1 && buffc[0] == '#') {
		while(read(fd, buffc, 1) && buffc[0] != '\n'){}
	}
	lseek(fd, -1, SEEK_CUR);
	//Warning! : buffc[0] holds the source value of the first input (FromNodeId)
	int numV = strtol(numv, NULL, 10);

	Graph* g = create_graph(numV);
	int src, dest;
	int byte_read;
	j = 0;
	do {
		//Read source vertex
		i = 0;
		while(((byte_read = read(fd, buffc, 1)) > 0) && buffc[0] != 9) {
			buffw[i++] = buffc[0];
		}		
		if (byte_read == 0)
			break;
		buffw[i] = '\0';
		src = strtol(buffw, NULL, 10);

		//Read destination vertex
		i = 0;
		while(((byte_read = read(fd, buffc, 1)) > 0) && (buffc[0] >= 48 && buffc[0] <= 57)) {
			buffw[i++] = buffc[0];
		}
		while (buffc[0] != '\n') {
			byte_read = read(fd, buffc, 1);
		}
		buffw[i] = '\0';
		dest = strtol(buffw, NULL, 10);
		//printf("%d %d\n", src, dest);
		//Insert the edge to the graph
		if (insert_edge(g, src, dest) == -1) {
			printf("Failed to insert an edge(out of graph range source: %d, dest: %d)\n", src, dest);
		}
	} while (byte_read > 0);

	close(fd);
	return g;
} 

int destroy_graph(Graph** graph) {
	if ((*graph) == NULL) {
		return -1;
	}
	Edge* temp;
	for (int i = 0; i < (*graph)->numV; ++i) {
		EdgeList list = (*graph)->vertices[i];

		while(list.head != NULL) {
			temp = list.head;
			list.head = list.head->next;
			free(temp);
		}
	}
	free((*graph)->vertices);
	free(*graph);
	graph = NULL;
	return 0;
}