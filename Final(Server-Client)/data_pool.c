#include "data_pool.h"

//Functions 

static void* threadFunc(void* arg) {
	pthread_track* th = (pthread_track*)arg;
	safe_queue** q = th->pack->avbl_queue;
	char buffer[1024] = {'\0'};
	char tokens[1024] = {'\0'};
	int src = -1, dest = -1;

	while(1) {
//		Block common available thread queue and enqueue itself to it.		
		if (pthread_mutex_lock(&((*th->pack->avbl_queue)->q_lock)) != 0) {
			print_log("Failed to make a thread ready for next processes");
			return NULL;
		}
		if (enqueue((*q)->q, &th->id, sizeof(int)) == -1) {
			print_log("Failed to make a thread ready for next processes");
			return NULL;
		} 
		if (pthread_cond_signal(&((*th->pack->avbl_queue)->cond_state)) != 0) {
			print_log("Failed to make a thread ready for next processes");
			return NULL;
		}
		if (pthread_mutex_unlock(&((*th->pack->avbl_queue)->q_lock)) != 0) {
			print_log("Failed to make a thread ready for next processes");
			return NULL;
		}
//		////////////////////////////////////////////////////////////////////////////
		
//		Start thread functionality
		if (pthread_mutex_lock(&(th->th_lock)) != 0) {
			print_log("A thread has failed to wait for next processes");
			return NULL;
		}
//			Wait for a signal to keep continue
			while (th->avail == TRUE && *(th->pack->exit) == FALSE) {
//				Print a waiting connection info to the terminal
				if(snprintf(buffer, 1024, "Thread #%d: waiting for connection", th->id) < 0) {
					perror("Failed to print an information to the log file");
					return NULL;
				}
				print_log(buffer), memset(buffer, '\0', strlen(buffer));
				if (pthread_cond_wait(&(th->avbl_state), &(th->th_lock)) != 0) {
					print_log("A thread has failed to wait for next processes");
					return NULL;
				}
			}
//		If exit signal has caught, exit flag will be set to TRUE
		if (*(th->pack->exit) == TRUE) {
			if	(pthread_mutex_unlock(&(th->th_lock)) != 0) {
				print_log("A thread has failed to exit");
				return NULL;
			}
			q = NULL;
			th = NULL;
			return NULL;
		} else {
//			If exit signal has not been caught and main thread make this thread busy, it means we got a job to do.
			th->avail = TRUE;
			if ((read(th->pack->clientfd, tokens, 1024)) < 0) {
				print_log("A thread has failed read client request");
				return NULL;
			}
			tokenize(tokens, &src, &dest);
				if(snprintf(buffer, 1024, "Thread #%d: searching database to find a path from %d to %d", th->id, src, dest) < 0) {
					perror("Failed to print an information to the log file");
					return NULL;
				}
				print_log(buffer), memset(buffer, '\0', strlen(buffer));
//			Searching a path in database
			if	(pthread_mutex_lock(&((*th->pack->cache)->c_lock)) != 0) {
				print_log("A thread has failed to process client request");
				return NULL;
			}
			char* p = search((*th->pack->cache)->cache, src, dest);
			if	(pthread_mutex_unlock(&((*th->pack->cache)->c_lock)) != 0) {
				print_log("A thread has failed to process client request");
				return NULL;
			}
//			If no path found in database
			if (p == NULL) {
				if(snprintf(buffer, 1024, "Thread #%d: no path in database, calculating %d->%d", th->id, src, dest) < 0) {
					perror("Failed to print an information to the log file");
					return NULL;
				}
				print_log(buffer), memset(buffer, '\0', strlen(buffer));
//				Calculate path
				p = path((*th->pack->graph), src, dest);
				if (p == NULL) {
//					If there is no path print failure message
					if(snprintf(buffer, 1024, "Thread #%d: path not possible from node %d to %d", th->id, src, dest) < 0) {
						perror("Failed to print an information to the log file");
						return NULL;
					}
					print_log(buffer), memset(buffer, '\0', strlen(buffer));
					if (write(th->pack->clientfd, "NO PATH", 8) < 0) {
						print_log("A thread has failed to write to the client");
						return NULL;
					}	
				} else {
//					If there is a path, print success message and insert path to the database
					if(snprintf(buffer, 1024, "Thread #%d: path calculated: %s", th->id, p) < 0) {
						perror("Failed to print an information to the log file");
						return NULL;
					}
					print_log(buffer), memset(buffer, '\0', strlen(buffer));
					if(snprintf(buffer, 1024, "Thread #%d: responding to the client and adding path to database", th->id) < 0) {
						perror("Failed to print an information to the log file");
						return NULL;
					}
					print_log(buffer), memset(buffer, '\0', strlen(buffer));
					if (write(th->pack->clientfd, p, strlen(p)) < 0) {
						print_log("A thread has failed to write to the client");
						return NULL;
					} 
					insert((*th->pack->cache)->cache, src, dest, p);
				}
			} else {
//				If a path found in database print success message and send it to the client
				if(snprintf(buffer, 1024, "Thread #%d: path found in database: %s", th->id, p) < 0) {
					perror("Failed to print an information to the log file");
					return NULL;
				}
				print_log(buffer), memset(buffer, '\0', strlen(buffer));
				if (write(th->pack->clientfd, p, strlen(p)) < 0) {
					perror("Failed to write to the client");
					return NULL;
				} 
			}
			if (pthread_mutex_unlock(&(th->th_lock)) != 0) {
				print_log("A thread has failed to be ready for next processes");
				return NULL;
			}
		}
		src = -1, dest = -1;
	}
	return NULL;
}

void tokenize(char* str, int* src, int* dest) {
	int i = 0;
	char* numb = strtok(str, " ");
	while (numb != NULL) {
		if (i++ == 0) 
			*src = atoi(numb);
		else
			*dest = atoi(numb);
		numb = strtok(NULL, " ");
	}
}

data_pool* create_data_pool(int init_size) {
	data_pool* newPool;
	if ((newPool = (data_pool*)malloc(sizeof(data_pool))) == NULL) {
		return NULL;
	}
	if ((newPool->avbl_queue = (safe_queue*)malloc(sizeof(safe_queue))) == NULL) {
		free(newPool);
		return NULL;
	}
	if ((newPool->avbl_queue->q = create_queue(sizeof(int))) == NULL)  {
		free(newPool->avbl_queue);
		free(newPool);
		return NULL;
	}
	if (pthread_cond_init(&newPool->avbl_queue->cond_state, NULL) != 0 || pthread_mutex_init(&newPool->avbl_queue->q_lock, NULL) != 0) {
		destroy_pool(&newPool);
		return NULL;
	}
	if ((newPool->cache = (safe_cache*)malloc(sizeof(safe_cache))) == NULL)  {
		free(newPool->avbl_queue);
		free(newPool);
		return NULL;
	}
	if ((newPool->cache->cache = create_CacheStruct()) == NULL)  {
		free(newPool->avbl_queue);
		free(newPool);
		return NULL;
	}
	if (pthread_mutex_init(&newPool->cache->c_lock, NULL) != 0) {
		//destroy_pool(&newPool);
		return NULL;
	}


	newPool->head = NULL;
	newPool->last = NULL;
	newPool->total_size = 0;
	newPool->total_used = 0;
	newPool->unique_thread_identification = 0;
	newPool->exit = FALSE;
	
	if (enlarge(&newPool, init_size) == -1) {
		destroy_pool(&newPool);
		return NULL;
	}
	return newPool;
}

data_node* create_data_node(int data_size, data_pool* dp) {
	data_node* newNode;
	if ((newNode = (data_node*)malloc(sizeof(data_node))) == NULL) {
		return NULL;
	}
	newNode->size = data_size;
	newNode->no_avail = data_size;
	newNode->next = NULL;

	if((newNode->threads = (pthread_track*)malloc(sizeof(pthread_track) * data_size)) == NULL) {
		free(newNode);
		return NULL;
	}
	for (int i = 0; i < data_size; ++i) {
		newNode->threads[i].id = dp->unique_thread_identification++;
		newNode->threads[i].avail = TRUE;
		if ((newNode->threads[i].pack = (pthread_package*)malloc(sizeof(pthread_package))) == NULL) {
			free(newNode->threads);
			free(newNode);
			return NULL;
		}
		newNode->threads[i].pack->avbl_queue = &(dp->avbl_queue);
		newNode->threads[i].pack->cache = &(dp->cache);
		newNode->threads[i].pack->graph = &(dp->graph);
		newNode->threads[i].pack->exit = &(dp->exit);
	}
	for (int i = 0; i < data_size; ++i) {
		if (pthread_cond_init(&newNode->threads[i].avbl_state, NULL) != 0 || pthread_mutex_init(&newNode->threads[i].th_lock, NULL) != 0
				|| pthread_create(&newNode->threads[i].thread, NULL, threadFunc, (void*)&newNode->threads[i]) != 0) {
			return NULL;
		}
	}
	return newNode;
}

pthread_track* get_thread(data_pool* dp, int id) {
	if (dp == NULL)
		return NULL;
	
	data_node* temp = dp->head;
	while (temp != NULL) {
		for (int i = 0; i < temp->size; ++i) {
			if (temp->threads[i].id == id) {
				return &(temp->threads[i]);
			}
		}
		temp = temp->next;
	}
	return NULL;
}

int enlarge(data_pool** dp, int size) {
	if (dp == NULL)
		return -1;

//	Create a new node
	data_node* new;
	if ((new = create_data_node(size, (*dp))) == NULL) {
		return -1;
	}
//	Add a new node to the end of the structure list
	if ((*dp)->last != NULL) {
		(*dp)->last->next = new;
	}
	(*dp)->last = new;
	if ((*dp)->head == NULL) {
		(*dp)->head = (*dp)->last;
	}
	(*dp)->total_size += size;
	return 0;
}

int destroy_pool(data_pool** dp) {
	if ((*dp) == NULL)
		return -1;

	(*dp)->exit = TRUE;
// 	Main joins all threads.	
	data_node* itr = (*dp)->head;
	while (itr != NULL) {
		for (int i = 0; i < itr->size; ++i) {
			if ((pthread_mutex_lock(&(itr->threads[i].th_lock)) != 0) || (pthread_cond_signal(&(itr->threads[i].avbl_state)) != 0)
					|| (pthread_mutex_unlock(&(itr->threads[i].th_lock)) != 0)) {
				print_log("Failed to exit a thread");
			} else {
				if (pthread_join(itr->threads[i].thread, NULL) != 0) {
					print_log("Failed to exit a thread");
				}
			}
		}
		itr = itr->next;
	}
	data_node* temp;
	while ((*dp)->head != NULL) {
		temp = (*dp)->head;
		(*dp)->head = (*dp)->head->next;
		for (int i = 0; i < temp->size; ++i) {
//			Destroy mutexes and condition variables
			if (pthread_cond_destroy(&temp->threads[i].avbl_state) != 0) {
				print_log("ERROR: Failed to destroy a condition variable");
			}
			if (pthread_mutex_destroy(&temp->threads[i].th_lock) != 0) {
				print_log("ERROR: Failed to destroy a mutex");
			}
			temp->threads[i].pack->avbl_queue = NULL;
			temp->threads[i].pack->cache = NULL;
			temp->threads[i].pack->graph = NULL;
			temp->threads[i].pack->exit = NULL;
			free(temp->threads[i].pack);
		}
		free(temp->threads);
		free(temp);
	}
	(*dp)->last = NULL;
	if (destroy_graph(&((*dp)->graph)) == -1) {
		print_log("Failed to free resources that are allocated for graph");
	}
	destroy_CacheStruct(&((*dp)->cache->cache));
	destroy_queue(&((*dp)->avbl_queue->q));
	if (pthread_mutex_destroy(&((*dp)->cache->c_lock)) != 0) {
		print_log("Failed to destroy mutex that is used for cache");
	}
	if (pthread_cond_destroy(&((*dp)->avbl_queue->cond_state)) != 0 || pthread_mutex_destroy(&((*dp)->avbl_queue->q_lock)) != 0) {
		print_log("Failed to destroy mutexes that are used for thread pool");
	}
	free((*dp)->avbl_queue);
	free((*dp)->cache);
	free((*dp));
	dp = NULL;
	return 0;
}