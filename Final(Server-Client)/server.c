#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include "Graph.h"
#include "data_pool.h"
#include "print_log.h"

#define DOMAIN AF_INET
#define WRITE_PERM (S_IWUSR)

#define ERR_LOG_FILE perror("Failed to write to the log file");

volatile sig_atomic_t FLAG_EXIT = 0;

static void SIGINT_HANDLER(int sig) {
	switch (sig) {
		case SIGINT:
			FLAG_EXIT = 1;
			break;
		default:
			//Do nothing
			break;
	}
}

static void commandlineError() {
	char* arg1 = 	"The commandline arguments must include following commands [-iposx]:\n"
					"-i: The path of the input file\n"
					"-p: PORT number (must be greater then 0)\n"
					"-o: The path of the log file\n"
					"-s: Number of threads in the pool at startup (must be greater then 0)\n"
					"-x: Maximum allowed number of threads (must be greater then 0 and s)\n";

	if (write(STDERR_FILENO, arg1, strlen(arg1)) == -1) {
		perror("Commandline Error: Failed to print error message");
	}

	exit(EXIT_FAILURE);
}

static void* resizer_thread_func(void* arg);
int init_sockaddr(int sockfd, int PORT, struct sockaddr_in* serv_addr);
int init_threads(data_pool** threads, int init_size, int file_log);
int load_graph(Graph** graph, char* file_in, int file_log);
int server(char* file_in, int PORT, int s, int MAX, int file_log);

int main(int argc, char* argv[]) {
	int fd_log;
	int PORT = -1;
	int s = -1;
	int x = -1;
	char* file_in = NULL;
	char* file_log = NULL;

//	Read commandline arguments
	int cmd = 0;
	while ((cmd = getopt(argc, argv, ":i:p:o:s:x:")) != -1) {
		switch (cmd) {
			case 'i':
				if ((file_in = strdup(optarg)) == NULL) {
					perror("Failed to start server!");
					exit(1);
				}
				break;
			case 'p':
				PORT = strtol(optarg, NULL, 10);
				break;
			case 'o':
				if ((file_log = strdup(optarg)) == NULL) {
					perror("Failed to start server!");
					exit(1);
				}
				break;
			case 's':
				errno = 0;
				s = strtol(optarg, NULL, 10);
				if (errno == EINVAL || errno == ERANGE) {
					perror("Failed to start server!");
				}
				break;
			case 'x':
				x = strtol(optarg, NULL, 10);
				if (errno == EINVAL || errno == ERANGE) {
					perror("Failed to start server!");
				}
				break;
			case ':':
				fprintf(stderr, "ERROR: No given input for command: -%c\n", optopt);
				commandlineError();
				break;

			case '?':
				fprintf(stderr, "ERROR: Unreleated command: -%c\n", optopt);
				commandlineError();
				break;
			
			default:
				commandlineError();
				break;
		}
	}

//	Check if commandline arguments are sufficient
	if (x < 1 || s < 2 || x < s || PORT < 0 || file_in == NULL || file_log == NULL) {
		if (file_in != NULL) {
			free(file_in);
		}
		if (file_log != NULL) {
			free(file_log);
		}
		commandlineError();
		return -1;	
	}
//	Open log file
	if((fd_log = open(file_log, (O_WRONLY | O_CREAT), WRITE_PERM | PERM)) == -1) {
		perror("Failed to open log file");
		free(file_in);
		free(file_log);
		return -1;
	}
	logger.file_log = fd_log;
	free(file_log);

//	Set signal handler for handling SIGINT
	struct sigaction sh;
	memset (&sh, 0, sizeof(sh));
	sh.sa_handler = SIGINT_HANDLER;
	if (sigaction(SIGINT, &sh, NULL) == -1) {
		char* err = "Failed to create server: SIGINT handler could not set!";
		print_log(err);
		/*if (write(fd_log, err, strlen(err)) == -1) {
			ERR_LOG_FILE
		}*/
	  	free(file_in);
	  	free(file_log);
	  	return -1;
	}
	server(file_in, PORT, s, x, fd_log);
	if (file_in != NULL)
		free(file_in);
	if (file_log != NULL)
		free(file_log);
	close(fd_log);
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
	return 0;
}

int server(char* file_in, int PORT, int s, int MAX, int file_log) {
	data_pool* threads;
	struct sockaddr_in serv_addr;
	int sockfd;
	int clientfd;
	char buffer[1024];
//	Create initial threads
	if ((init_threads(&threads, s, file_log)) == -1) {
		print_log("Failed to create initial threads!");
		if (destroy_pool(&threads) == -1) {
			print_log("Some errors occured while shutting down the server");
		}
		return -1;
	}
//	Loading graph to the server
	if (load_graph(&threads->graph, file_in, file_log) == -1) {
		print_log("Failed to load graph!");
		if (destroy_pool(&threads) == -1) {
			print_log("Some errors occured while shutting down the server");
		}
		return -1;
	}
	free(file_in);
//	Point to the thread pool
	safe_queue** q = &(threads->avbl_queue);

//	Create resizer thread
	resizer_thread resizer;
	if (pthread_cond_init(&resizer.rs_cond, NULL) != 0 || pthread_mutex_init(&resizer.rs_lock, NULL) != 0
			|| pthread_create(&resizer.th, NULL, resizer_thread_func, (void*)&resizer) != 0) {
		print_log("Failed to create resizer thread");
		if (destroy_pool(&threads) == -1) {
			print_log("Some errors occured while shutting down the server");
		}
		return -1;
	}
	resizer.thread_pool = &threads;
	resizer.max_size = MAX;

//	Create connection between server and client
	if ((sockfd = socket(DOMAIN, SOCK_STREAM, 0)) == -1) {
		print_log("Failed to create server!");
		if (destroy_pool(&threads) == -1) {
			print_log("Some errors occured while shutting down the server");
		}
		return -1;
	}
	init_sockaddr(sockfd, PORT, &serv_addr);
	if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(struct sockaddr_in)) == -1) {
		print_log("Failed to bind the server");
		if (destroy_pool(&threads) == -1) {
			print_log("Some errors occured while shutting down the server");
		}
		return -1;
	}
	if (listen(sockfd, MAX) == -1) {
		print_log("Failed to listen from the server");
		if (destroy_pool(&threads) == -1) {
			print_log("Some errors occured while shutting down the server");
		}
		return -1;
	}

//	Start processing clients
	while (1) {
		socklen_t size = sizeof(struct sockaddr_in);
		if ((clientfd = accept(sockfd, (struct sockaddr*)&serv_addr, &size)) == -1) {
			print_log("Termination signal received, waiting for ongoing threads to complete.");
			if (destroy_pool(&threads) == -1) {
				print_log("Some errors occured while shutting down the server");
			}
			return -1;
		}
//		Lock before checking the available threads.
		if (pthread_mutex_lock(&((*q)->q_lock)) != 0) {
			print_log("Failed to lock a mutex");
			if (destroy_pool(&threads) == -1) {
				print_log("Some errors occured while shutting down the server");
			}
			return -1;
		}
//		Thread will wait if the size is 0 and exit signal has not arrived
		while ((*q)->q->size <= 0 && FLAG_EXIT == 0) {
			print_log("No thread is available! Waiting for one.");
			if (pthread_cond_wait(&((*q)->cond_state), &((*q)->q_lock)) != 0) {
				print_log("Server has failed to wait for next requests");
				if (destroy_pool(&threads) == -1) {
					print_log("Some errors occured while shutting down the server");
				}
				return -1;
			}
		}
//		If exit signal arrived and there is no more data to be processed, thread will exit.
		if (FLAG_EXIT == 0) {
//			Lock queue and take one available thread
			int* th_id = (int*)dequeue(threads->avbl_queue->q);
			if (th_id == NULL) {
				print_log("Server has failed to find an available thread");
				if (destroy_pool(&threads) == -1) {
					print_log("Some errors occured while shutting down the server");
				}
				return -1;
			}
			if	(pthread_mutex_unlock(&((*q)->q_lock)) != 0) {
				print_log("Server has failed while getting an available thread");
				return -1;
			}
//			Get thread information
			pthread_track* th = get_thread(threads, (*th_id));
			if (th == NULL) {
				print_log("Server has failed to get an available thread");
				if (destroy_pool(&threads) == -1) {
					print_log("Some errors occured while shutting down the server");
				}
				return -1;
			}
//			Set thread to work the next job
			if (pthread_mutex_lock(&(th->th_lock)) != 0) {
				print_log("Failed to process request");
				if (destroy_pool(&threads) == -1) {
					print_log("Some errors occured while shutting down the server");
				}
				return -1;
			}
			th->avail = FALSE;
			th->pack->clientfd = clientfd;
			if (pthread_cond_signal(&(th->avbl_state)) != 0) {
				print_log("Failed to process request");
				if (destroy_pool(&threads) == -1) {
					print_log("Some errors occured while shutting down the server");
				}
				return -1;
			}	
			if (pthread_mutex_unlock(&(th->th_lock)) != 0) {
				print_log("Failed to process request");
				if (destroy_pool(&threads) == -1) {
					print_log("Some errors occured while shutting down the server");
				}
				return -1;
			}
			float load = (((float)threads->total_size - (float)(*q)->q->size) / (float)threads->total_size) * 100;
			if (snprintf(buffer, 1024, "A connection has been delegated to thread id #%d, system load: %0.1f %%" ,*th_id , load) < 0) {
				perror("Failed to print an information to the log file");
			} else {
				print_log(buffer), memset(buffer, '\0', strlen(buffer));
			}
//			Let resizer thread check the pool before accessing it again
			if (pthread_mutex_lock(&(resizer.rs_lock)) != 0) {
				print_log("Resizer thread has failed");
				if (destroy_pool(&threads) == -1) {
					print_log("Some errors occured while shutting down the server");
				}
				return -1;
			}
			if (pthread_cond_signal(&(resizer.rs_cond)) != 0) {
				print_log("Resizer thread has failed");
				if (destroy_pool(&threads) == -1) {
					print_log("Some errors occured while shutting down the server");
				}
				return -1;
			}	
			if (pthread_mutex_unlock(&(resizer.rs_lock)) != 0) {
				print_log("Resizer thread has failed");
				if (destroy_pool(&threads) == -1) {
					print_log("Some errors occured while shutting down the server");
				}
				return -1;
			}
			th = NULL;
			free(th_id);

		} else {
			print_log("Termination signal received, waiting for ongoing threads to complete.");
			if (pthread_mutex_lock(&(resizer.rs_lock)) != 0 || pthread_cond_signal(&(resizer.rs_cond)) != 0 || pthread_mutex_unlock(&((*q)->q_lock)) != 0) {
				print_log("Resizer thread has failed");
				if (destroy_pool(&threads) == -1) {
					print_log("Some errors occured while shutting down the server");
				}
				return -1;
			}
			if (pthread_join(resizer.th, NULL) != 0) {
				print_log("Failed to exit resizer thread");
			}
			if (destroy_pool(&threads) == -1) {
				print_log("Some errors occured while shutting down the server");
			}
			print_log("All threads have terminated, server shutting down.");
			q = NULL;
			return 0;
		}
	}
	return 0;
}

static void* resizer_thread_func(void* arg) {
	resizer_thread* resizer = (resizer_thread*)arg;
	float load = 0;
	float max_load = 75;
	char buffer[1024] = {'\0'};
	char numb[20] = {'\0'};
	
	while (1) {
		if (pthread_mutex_lock(&resizer->rs_lock) != 0) {
			print_log("Resizer thread has failed");
			FLAG_EXIT = 1;
			return NULL;
		}
//		Thread will wait if the size is 0 and exit signal has not arrived
		load = (((float)(*resizer->thread_pool)->total_size - (float)((*resizer->thread_pool)->avbl_queue->q->size)) / (float)(*resizer->thread_pool)->total_size) * 100;
		while (load < max_load && FLAG_EXIT == 0) {
			load = (((float)(*resizer->thread_pool)->total_size - (float)((*resizer->thread_pool)->avbl_queue->q->size)) / (float)(*resizer->thread_pool)->total_size) * 100;
			if (pthread_cond_wait(&resizer->rs_cond, &resizer->rs_lock) != 0) {
				print_log("Resizer thread has failed");
				FLAG_EXIT = 1;
				return NULL;
			}
		}
		if (FLAG_EXIT == 1 && (*resizer->thread_pool)->total_size >= (resizer->max_size)) {
			return NULL;
		}
		else {
			int size = ((*resizer->thread_pool)->total_size) / 4;
			if (enlarge(resizer->thread_pool, size) == -1) {
				print_log("Failed to enlarge the thread pool");
			} else {
				if (snprintf(numb, 20, "%d", (*resizer->thread_pool)->total_size) < 0) {
					print_log("Failed to create log message (Pool extended)");
				} else {
					strcpy(buffer, "System load %75, pool extended to ");
					strcat(buffer, numb);
					strcat(buffer, " threads");
					print_log(buffer), memset(buffer, '\0', strlen(buffer));
				}
			}
		}
		if (pthread_mutex_unlock(&resizer->rs_lock) != 0) {
			print_log("Resizer thread has failed");
			FLAG_EXIT = 1;
			return NULL;
		}
	}
	return NULL;
}

int load_graph(Graph** graph, char* file_in, int file_log) {
	print_log("Loading graph...");
	clock_t t;
	char* text_load_graph_end = "Graph loaded in ";
	char* text_load_graph_end2 = " seconds with ";
	char* text_load_graph_end3 = " nodes and ";
	char* text_load_graph_end4 = " UNKNOWN! edges.\n";
	char load_time[10] = {'\0'};
	char nodes[20] = {'\0'};
	//char edges[30] = {'\0'};
	
	char* text_load = (char*)malloc(strlen(text_load_graph_end) + strlen(text_load_graph_end2) + strlen(text_load_graph_end3) + strlen(text_load_graph_end4) + 60);
	if (text_load == NULL) {
		return -1;
	}
	t = clock();
//	Load graph from the file
	if(((*graph) = loadGraphFromFile(file_in)) == NULL){
		return -1;
	}
	t = clock() - t;
	double time = ((double) t) / CLOCKS_PER_SEC;
	if ((snprintf(load_time, 9, "%0.1f", time) < 0) || snprintf(nodes, 29, "%d", (*graph)->numV) < 0) {
		print_log("Failed to create log message about graph loading time");
		return -1;
	}
	strcpy(text_load, text_load_graph_end);
	strcat(text_load, load_time);
	strcat(text_load, text_load_graph_end2);
	strcat(text_load, nodes);
	strcat(text_load, text_load_graph_end3);
	strcat(text_load, text_load_graph_end4);
	
	print_log(text_load);
	free(text_load);
	return 0;
}

int init_threads(data_pool** threads, int init_size, int file_log) {
	char* init_th = "A pool of ";
	char* init_th2 = " threads has been created.\n";
	char size[10];
	char* text_th = (char*)malloc(strlen(init_th) + strlen(init_th2) + 10);
	if (text_th == NULL)
		return -1;

	if (snprintf(size, 9, "%d", init_size) < 0) {
		return -1;
	}
	strcpy(text_th, init_th);
	strcat(text_th, size);
	strcat(text_th, init_th2);
	print_log(text_th);
	(*threads) = create_data_pool(init_size); 
	return 0;
}

int init_sockaddr(int sockfd, int PORT, struct sockaddr_in* serv_addr) {
	serv_addr->sin_family = DOMAIN;
	serv_addr->sin_port = htons(PORT);
	serv_addr->sin_addr.s_addr = htonl(INADDR_ANY);
	return 0;
}