#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <time.h>

#define PERMS (S_IRUSR | S_IWUSR)
#define NO_PLATES 3

const char* nkitchen = "kitchen#99999";
const char* ncounter = "counter#99999";
const char* nstudent = "student#99999";
const char* nprftGuy = "prftGuy#99999";
const char* nstdTble = "nstdTbl#99999";

static void commandlineError() {
	char* arg1 = 	"The commandline arguments must include following commands [-NMTSL]:\n"
					"-N: Number of cooks\n"
					"-M: Number of students\n"
					"-T: Number of tables\n"
					"-S: Size of the counter\n"
					"-L: Number of times that each student eats food\n"
					"The constraints are as follows:\n"
					"M > N > 2 | S > 3 | M > T >= 1 | L >= 3\n";

	if (write(STDERR_FILENO, arg1, strlen(arg1)) == -1) {
		perror("Commandline Error: Failed to print error message");
	}

	exit(EXIT_FAILURE);
}

static void sig_handler(int sig) {
	switch (sig) {
		case SIGINT:
			shm_unlink(nkitchen);
			shm_unlink(ncounter);
			shm_unlink(nstudent);
			shm_unlink(nprftGuy);
			shm_unlink(nstdTble);
			exit(EXIT_SUCCESS);
	}
}

typedef struct {
	sem_t full;    	//Number of foods
	sem_t empty;  	//Number of empty spaces
	sem_t let;		//Used to block processes
	sem_t delivery; //Number of products that will be supplied
	sem_t supp;		//Used to block supplier
	sem_t wait_plate;	//Wait for plate to be delivered if required
	int waiting_plate;	//If this is set, supplier will notify cooks that a plate is delivered.
	int supp_done;		//A flag to know if supplier is done
	int order;			//Order to take plates from kitchen(Can be changed by a cook)
	int count[NO_PLATES]; 	//Counter of food: 0 - Soup | 1 - Course | 2 - Desert
	int count_total;	  	//Total counter of food
	int toBeDelivered[NO_PLATES];	//Number of food of each type that must be delivered to the kitchen
} st_kitchen;

typedef struct {
	sem_t full;		//Number of foods
	sem_t empty;	//Number of empty spaces
	sem_t let;		//Used to block processes
	sem_t P;		//Used to wait for soup
	sem_t C;		//Used to wait for course
	sem_t D;		//Used to wait for desert
	int count[NO_PLATES];	//Counter of food: 0 - Soup | 1 - Course | 2 - Desert
	int count_total;		//Total counter of food
	int student_count;		//Number of students at counter
}st_counter;

typedef struct {
	sem_t let;		//Used to block processes
	sem_t table;	//Used to wait for a table
	int count_table;	//Number of tables available
	char* tables;		//Shared memory to know which tables are available
}st_student;

typedef struct {
	sem_t tray;		//Let a student know at least one plate of each type is available
	sem_t wait;		//Wait for a student to wake perfectGuy up
}st_perfectGuy;

void simulation(const int M, const int N, const int S, const int T, const int L, const char* suppBag);
st_kitchen* initializeKitchen(int size_kitchen, int size_delivery);
st_counter* initializeCounter(int size_counter);
st_student* initializeStudent(int size_table);
st_perfectGuy* initializePerfectGuy();
void destroy_kitchen(st_kitchen* kitchen);
void destroy_counter(st_counter* counter);
void destroy_student(st_student* student);
void destroy_prftguy(st_perfectGuy* prftGuy);
void destroy_all(st_kitchen* kitchen, st_counter* counter, st_student* student, st_perfectGuy* prftGuy);
void* shmalloc(const char* name, int prot, int size);
char* readSuppliersBag(int size, char* filePath);

int main (int argc, char* argv[]) {
	/*
	 * Simulation of student mess hall of a university.
	 * Commandline arguments:
	 * N : Number of cooks
	 * M : Number of students
	 * T : Number of tables
	 * S : Size of the counter
	 * K : Size of the kitchen // K = 2LM+1
	 * L : Number of times that each student eats food
	 * Every plate of food can be of three types: 
	 * Soup(P), main course(C) or desert(D).
	 */
	struct sigaction sa;
	memset (&sa, 0, sizeof(sa));
	sa.sa_handler = sig_handler;
	if (sigaction(SIGINT, &sa, NULL) == -1) {
	    perror("Failed to set SIGINT handler");
	  	return -1;
	}

	int N = -1, M = -1, T = -1, S = -1, L = -1;
	
	char* filePath = NULL;
	int cmd = 0;
	while ((cmd = getopt(argc, argv, ":N:M:T:S:L:F:")) != -1) {
		switch (cmd) {
			case 'N':
				N = strtol(optarg, NULL, 10);
				break;
			case 'M':
				M = strtol(optarg, NULL, 10);
				break;
			case 'T':
				T = strtol(optarg, NULL, 10);
				break;
			case 'S':
				S = strtol(optarg, NULL, 10);
				break;
			case 'F':
				if ((filePath = (char*)malloc(strlen(optarg)))) {
					strcpy(filePath, optarg);
				} else {
					printf("Allocation Fault(Input file)\n");
					exit(0);
				}
				break;
			case 'L':
				L = strtol(optarg, NULL, 10);
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

	if (!(M > N && N > 2 && S > 3 && M > T && T >= 1 && L >= 3 && filePath != NULL)) {
		free(filePath);
		commandlineError();
	}

	char* supp;
	if ((supp = readSuppliersBag(L*M, filePath)) == NULL) {
		free(filePath);
		exit(0);
	}
	simulation(M,N,S,T,L, supp);
	free(supp);
	free(filePath);
	return 0;
}

void simulation(const int M, const int N, const int S, const int T, const int L, const char* suppBag) {
	int K = (2*L*M) + 1;
	int size_delivery = NO_PLATES * L * M;
	
	st_kitchen* kitchen = initializeKitchen(K, size_delivery);
	st_counter* counter = initializeCounter(S);
	st_student* student = initializeStudent(T);
	st_perfectGuy* perfectGuy = initializePerfectGuy();

	if (fork() == 0) {
		/************************************************************************
			perfectGuy is used to deliver foods to the students. It takes
			one type of each plate and lets a student know that food is ready.
		***********************************************************************/
		//Unmap non-required sources
		if (munmap(kitchen, sizeof(st_kitchen)) == -1 || munmap(student->tables, sizeof(student->tables)) == -1
				||munmap(student, sizeof(st_student)) == -1) {
			perror("Failed to munmap shared sources in perfectGuy");
		}
		for (int j = 0; j < L*M; ++j) {
			sem_wait(&perfectGuy->wait);	//Will be awakened by a student
			sem_wait(&counter->P);			//Wait for each
			sem_wait(&counter->C);			// 	type of plates
			sem_wait(&counter->D);
			
			sem_wait(&counter->let);		//Blocks counter to change variables
			counter->count[0]--;			//	This block will be unlocked by a student.
			sem_post(&counter->empty);
			counter->count[1]--;
			sem_post(&counter->empty);
			counter->count[2]--;
			sem_post(&counter->empty);
			counter->count_total -= 3;
			sem_post(&perfectGuy->tray);	//Notify a student that food is ready
		}

		//Unmap the rest sources
		if (munmap(counter, sizeof(st_counter)) == -1 || munmap(perfectGuy, sizeof(st_perfectGuy)) == -1) {
			perror("Failed to munmap shared sources in perfectGuy");
		}
		exit(0);
	}

	for (int i = 0; i < M; i++) {
		if (fork() == 0) {
			/***********************************************************************
				This is student process. Each student waits for perfectGuy to
				know if one plate for each type of food is ready. Then a student
				takes 3 plates and sits at a table to eat the food.
			***********************************************************************/
			if (munmap(kitchen, sizeof(st_kitchen)) == -1) {
				perror("Failed to munmap shared sources in student");
			}
			for (int j = 0; j < L; ++j) {
				printf("Student %d is going to the counter(round %d) - # of students at counter: %d and counter items P:%d, C:%d, D:%d = %d\n"
						, i, j+1, counter->student_count, counter->count[0], counter->count[1], counter->count[2], counter->count_total);
					counter->student_count++;

				sem_post(&perfectGuy->wait);	//Wake up perfect guy.
				sem_wait(&perfectGuy->tray);	//Wait for 3 plates of each type

				printf("Student %d got food and is going to get a table (round %d) - # of empty tables: %d\n", i, j+1, student->count_table);
					counter->student_count--;

				sem_post(&counter->let);	//Let counter know the calculation is done
				sem_wait(&student->table);	//Wait for a table
				sem_wait(&student->let);	//Tables are shared spaces. So students should sit at a table
											//in an order. Otherwise they may sit at a same table.
				int z = 0;
				while(student->tables[z] != 'E'){	//Searches an 'E'mpty table
					z++;
				}
				student->tables[z] = 'S';	//Student 'S'its at the table
					student->count_table--;		

				sem_post(&student->let);	//Let other students select a table
				printf("Student %d sat at table %d to eat (round %d) - # empty tables: %d\n", i, z, j+1, student->count_table);

				sem_wait(&student->let);   //Before leaving the table a student should notify other students
				student->tables[z] = 'E';  //that the table is going to be empty.
					student->count_table++;

				if (j+1 != L)
					printf("Student %d left table %d to eat again (round %d) - empty tables: %d\n", i, z, j+1, student->count_table);
				sem_post(&student->table);
				sem_post(&student->let);
			}
			printf("Student %d is done eating L = %d times - going home - GOODBYE!!!\n", i, L);
			if (munmap(student->tables, sizeof(student->tables)) == -1 || munmap(student, sizeof(st_student)) == -1 
					|| munmap(counter, sizeof(st_counter)) == -1 || munmap(perfectGuy, sizeof(perfectGuy)) == -1) {
				perror("Failed to munmap shared sources in student");
			}
			exit(0);
		}
	}

	char del[10];
	for (int i = 0; i < N; i++) {	
		if(fork() == 0) {
			/************************************************************************
				This is cook process. Each cook takes a plate from the kitchen(if
				there is one) and deliveres it to the counter.
			***********************************************************************/
			if (munmap(student->tables, sizeof(student->tables)) == -1 || munmap(student, sizeof(st_student)) == -1 
					|| munmap(perfectGuy, sizeof(st_perfectGuy)) == -1) {
				perror("Failed to munmap shared sources in cook");
			}
			while (!sem_trywait(&kitchen->delivery)) {	//While there are plates to be delivered to the counter
				sem_wait(&kitchen->full);	//Wait if the kitchen is empty
				sem_wait(&kitchen->let);	//Block other cooks
				
				sem_wait(&kitchen->supp);	//Block supplier before making calculations
				printf("Cook %d is going to the kitchen to wait for/get a plate - kitchen items P:%d, C:%d, D:%d = %d\n",
							i, kitchen->count[0], kitchen->count[1], kitchen->count[2], kitchen->count_total);
				
				//If a cook comes to the kitchen but there is no food, then cook must choose a way to go on.
				//If supplier is already done and there is no food at the kitchen, there is something went wrong.
				if (kitchen->supp_done == 1 && kitchen->count_total == 0) {
					printf("There is no food to be delivered to the kitchen. All cooks must have been done until now.\n");
					sem_post(&kitchen->supp);
					destroy_all(kitchen, counter, student, perfectGuy);
					exit(0);
				}

				/***********************************************************************************
					If there is no enough space in the counter, cook should choose the next plate
					specificly. If a type of plate is missing in the counter, then this type of a
					plate should be delivered to the counter. 
				************************************************************************************/
				while (kitchen->count[kitchen->order] == 0) {
					if (kitchen->toBeDelivered[kitchen->order] == 0) {	//If all foods of a type are delivered, pass to the next type of plate.
						kitchen->order = (kitchen->order + 1) % NO_PLATES;
					} else if ((S - counter->count_total) < NO_PLATES) {  //If there is not enough space and a missing plate on the counter
						kitchen->waiting_plate = kitchen->order;		  //wait supplier to delivered that food to the kitchen.
						sem_post(&kitchen->supp);
						sem_wait(&kitchen->wait_plate);
						sem_wait(&kitchen->supp);
						break;
					} else {
						kitchen->order = (kitchen->order + 1) % NO_PLATES;
					}	
				}
				
				//Taking the food from the kitchen.
				switch (kitchen->order) {
					case 0:
						kitchen->count[0]--; //Soup
						strcpy(del, "Soup");
						break;
					case 1:
						kitchen->count[1]--; //Course
						strcpy(del, "Course");
						break;
					case 2:
						kitchen->count[2]--; //Desert
						strcpy(del, "Desert");
						break;
					default:
						printf("There is no such food in menu.\n");
						exit(0);
				}
				kitchen->order = (kitchen->order + 1) % NO_PLATES;
				kitchen->count_total--;

				sem_post(&kitchen->supp);
				printf("Cook %d is going to the counter to deliver: %s - counter items P:%d, C:%d, D:%d = %d\n", 
							i, del, counter->count[0], counter->count[1], counter->count[2], counter->count_total);

				//Delivering the food to the counter
				sem_wait(&counter->empty);	//Wait if the counter is full
				sem_wait(&counter->let);	//Block students to change variables

				if (strcmp(del, "Soup") == 0) {
					sem_post(&counter->P);
					counter->count[0]++;
				} else if (strcmp(del, "Course") == 0) {
					sem_post(&counter->C);
					counter->count[1]++;
				} else if (strcmp(del, "Desert") == 0) {
					sem_post(&counter->D);
					counter->count[2]++;
				} else {
					printf("There is no such food in menu.\n");
					exit(0);	
				}
				counter->count_total++;
				
				if (counter->count_total > S){
					printf("COUNTER LIMIT HAS BEEN CROSSED: %d\n", counter->count_total);
					destroy_all(kitchen, counter, student, perfectGuy);
					exit(0);
				}
	
				printf("Cook %d placed %s on the counter - counter items P:%d, C:%d, D:%d = %d\n", 
							i, del, counter->count[0], counter->count[1], counter->count[2], counter->count_total);
				
				sem_post(&counter->let);
				sem_post(&counter->full);
				sem_post(&kitchen->let);
				sem_post(&kitchen->empty);
			}
			printf("Cook %d finished serving - items at kitchen: %d - going home - GOODBYE!!!\n", i, kitchen->count_total);
			if (munmap(counter, sizeof(st_counter)) == -1 || munmap(kitchen, sizeof(st_kitchen)) == -1) {
				perror("Failed to munmap shared sources in cook");
			}
			exit(0);
		}
	}

	for (int i = 0; i < strlen(suppBag); ++i) {
		/****************************************************************
			This is supplier process. Supplier will deliver randomly
			ordered plates to the kitchen one by one.
		*****************************************************************/
		sem_wait(&kitchen->empty);	//Wait for an empty room in the kitchen
		sem_wait(&kitchen->supp);	//Wait if a cook is making calculations in the kitchen
		
		switch (suppBag[i]) {
			case 'P':
				kitchen->count[0]++; //Soup
				kitchen->toBeDelivered[0]--;
				strcpy(del, "Soup");
				printf("The supplier is going to the kitchen to deliver %s: kitchen items P:%d, C:%d, D:%d = %d\n" 
						, del, kitchen->count[0], kitchen->count[1], kitchen->count[2], kitchen->count_total);
				if (kitchen->waiting_plate == 0) {	//If a cook is waiting for a soup
					kitchen->waiting_plate = -1;	//let cook know that soup has delivered.
					sem_post(&kitchen->wait_plate);
				}
				break;
			case 'C':
				kitchen->count[1]++; //Course
				kitchen->toBeDelivered[1]--;
				strcpy(del, "Course");
				printf("The supplier is going to the kitchen to deliver %s: kitchen items P:%d, C:%d, D:%d = %d\n" 
					, del, kitchen->count[0], kitchen->count[1], kitchen->count[2], kitchen->count_total);
				if (kitchen->waiting_plate == 1) {	//If a cook is waiting for a course
					kitchen->waiting_plate = -1;	//let cook know that course has delivered.
					sem_post(&kitchen->wait_plate);
				}
				break;
			case 'D':
				kitchen->count[2]++; //Desert
				kitchen->toBeDelivered[2]--;
				strcpy(del, "Desert");
				printf("The supplier is going to the kitchen to deliver %s: kitchen items P:%d, C:%d, D:%d = %d\n" 
					, del, kitchen->count[0], kitchen->count[1], kitchen->count[2], kitchen->count_total);
				if (kitchen->waiting_plate == 2) {	//If a cook is waiting for a desert
					kitchen->waiting_plate = -1;	//let cook know that desert has delivered.
					sem_post(&kitchen->wait_plate);
				}
				break;
			default:
				printf("There is no such food in the menu.\n");
				destroy_all(kitchen, counter, student, perfectGuy);
				exit(0);
		}

		kitchen->count_total++;		
		printf("The supplier delivered %s - after delivery: kitchen items P:%d, C:%d, D:%d = %d\n" 
					, del, kitchen->count[0], kitchen->count[1], kitchen->count[2], kitchen->count_total);
		
		if (i+1 == strlen(suppBag)) {
			printf("The supplier finished supplying - GOODBYE!\n");
			kitchen->supp_done = 1;
		}
		sem_post(&kitchen->supp);	
		sem_post(&kitchen->full);
	}

	while (wait(NULL) > 0) {
	//	Wait for all child processes to terminate.
	}
	destroy_all(kitchen, counter, student, perfectGuy);
}


void* shmalloc(const char* name, int prot, int size) {
	/**********************************************************
		This function returns allocated shared memory space
		with given size and prot values.
	**********************************************************/
	int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL ,PERMS);
		if (fd == -1) {
			perror("Failed to create shared memory");
			return NULL;
		}
		if (ftruncate(fd, size) == -1) {
			perror("Failed to create shared memory");
			return NULL;
		}

	void* shm = (void*)mmap(NULL, size, prot, MAP_SHARED, fd, 0);
		if (shm == MAP_FAILED){
			perror("Failed to create shared memory");
			return NULL;
		}
		if(close(fd) == -1) {
			perror("Failed to create shared memory");
			return NULL;
		}
	return shm;
}

st_kitchen* initializeKitchen(int size_kitchen, int size_delivery) {
	/*************************************************************
		This function returns allocated shared memory space
		for st_kitchen structer after initializing variables
		with given kitchen size, delivery size and other values.
	*************************************************************/
	st_kitchen* kitchen = (st_kitchen*)shmalloc(nkitchen, PROT_READ | PROT_WRITE, sizeof(st_kitchen));
	kitchen->count_total = 0;
	kitchen->order = 0;
	kitchen->waiting_plate = -1;
	kitchen->supp_done = 0;

	for (int i = 0; i < NO_PLATES; ++i) {
		kitchen->toBeDelivered[i] = (size_delivery / NO_PLATES);
		kitchen->count[i] = 0;
	}
	
 	sem_init(&kitchen->full, 1, 0);
 	sem_init(&kitchen->empty, 1, size_kitchen);
 	sem_init(&kitchen->let, 1, 1);
 	sem_init(&kitchen->delivery, 1, size_delivery);
 	sem_init(&kitchen->wait_plate, 1, 0);
 	sem_init(&kitchen->supp, 1, 1);
 	return kitchen;
}

st_counter* initializeCounter(int size_counter) {
	/**********************************************************
		This function returns allocated shared memory space
		for st_counter structer after initializing variables
		with given counter size and other values.
	**********************************************************/
	st_counter* counter = (st_counter*)shmalloc(ncounter, PROT_READ | PROT_WRITE, sizeof(st_counter));
	counter->count[0] = 0;
	counter->count[1]= 0;
	counter->count[2] = 0;
	counter->count_total = 0;
	counter->student_count = 0;

	sem_init(&counter->full, 1, 0);
 	sem_init(&counter->empty, 1, size_counter);
 	sem_init(&counter->let, 1, 1);

	sem_init(&counter->P, 1, 0);
	sem_init(&counter->C, 1, 0);
	sem_init(&counter->D, 1, 0);

	return counter;
}

st_student* initializeStudent(int size_table) {
	/**********************************************************
		This function returns allocated shared memory space
		for st_student structer after initializing variables
		with given table size and other values.
	**********************************************************/
	st_student* student = (st_student*)shmalloc(nstudent, PROT_READ | PROT_WRITE, sizeof(st_student));
	student->count_table = size_table;
	student->tables = (char*)shmalloc(nstdTble, PROT_READ | PROT_WRITE, sizeof(size_table));
	memset(student->tables, 'E', size_table);
	
	sem_init(&student->let, 1, 1);
	sem_init(&student->table, 1, size_table);

	return student;
}

st_perfectGuy* initializePerfectGuy() {
	/**********************************************************
		This function returns allocated shared memory space
		for st_perfectGuy structer after initializing variables
		with default values.
	**********************************************************/
	st_perfectGuy* perfectGuy = (st_perfectGuy*)shmalloc(nprftGuy, PROT_READ | PROT_WRITE, sizeof(st_perfectGuy));
	sem_init(&perfectGuy->tray, 1, 0);
	sem_init(&perfectGuy->wait, 1, 0);

	return perfectGuy;
}

void destroy_kitchen(st_kitchen* kitchen) {
	/**********************************************************
		This function destroys semaphores, unmaps and unlinks
		shared memory spaces of st_kitchen structure.
	**********************************************************/
	if (sem_destroy(&kitchen->full) == -1 || sem_destroy(&kitchen->empty) == -1 || sem_destroy(&kitchen->let) == -1) {
		perror("sem_destroy: kitchen");
	}
	if (munmap(kitchen, sizeof(st_kitchen)) == -1) {
		perror("munmap : kitchen");
	}
	shm_unlink(nkitchen);
}

void destroy_counter(st_counter* counter) {
	/**********************************************************
		This function destroys semaphores, unmaps and unlinks
		shared memory spaces of st_counter structure.
	**********************************************************/
	if (sem_destroy(&counter->P) == -1 || sem_destroy(&counter->C) == -1 || sem_destroy(&counter->D) == -1
		 || sem_destroy(&counter->full) == -1 || sem_destroy(&counter->empty) == -1 || sem_destroy(&counter->let) == -1) {
		perror("sem_destroy: counter");
	}
	if (munmap(counter, sizeof(st_counter)) == -1) {
		perror("munmap : counter");
	}
	shm_unlink(ncounter);	
}

void destroy_student(st_student* student) {
	/**********************************************************
		This function destroys semaphores, unmaps and unlinks
		shared memory spaces of st_student structure.
	**********************************************************/
	if (sem_destroy(&student->let) == -1 || sem_destroy(&student->table) == -1) {
		perror("sem_destroy: student");
	}
	if (munmap(student->tables, sizeof(student->tables)) == -1 || munmap(student, sizeof(st_student)) == -1) {
		perror("munmap : student");
	}
	shm_unlink(nstdTble);
	shm_unlink(nstudent);
}

void destroy_prftguy(st_perfectGuy* perfectGuy) {
	/**********************************************************
		This function destroys semaphores, unmaps and unlinks
		shared memory spaces of st_perfectGuy structure.
	**********************************************************/
	if (sem_destroy(&perfectGuy->tray) == -1 || sem_destroy(&perfectGuy->wait) == -1) {
		perror("sem_destroy: perfectGuy");
	}
	if (munmap(perfectGuy, sizeof(st_perfectGuy)) == -1) {
		perror("munmap : perfectGuy");
	}
	shm_unlink(nprftGuy);
}


void destroy_all(st_kitchen* kitchen, st_counter* counter, st_student* student, st_perfectGuy* perfectGuy) {
	/**********************************************************
		This function destroys all used structures.
	**********************************************************/
	destroy_kitchen(kitchen);
	destroy_counter(counter);
	destroy_student(student);
	destroy_prftguy(perfectGuy);
}

char* readSuppliersBag(int size, char* filePath) {
	/*********************************************************************
		This function reads input file that is required by the supplier.
		Size: Size of each plate seperately
	*********************************************************************/
	int fd;
	int file_size = size*NO_PLATES;
	int bytes_read = 0;
	char* chr;

	if((fd = open(filePath, O_RDONLY, PERMS)) == -1) {
		perror("Failed to open input file");
		return NULL;
	}
	if ((chr = malloc((sizeof(char)*file_size)+1)) == NULL) {
		perror("Failed to allocate memory\n");
		return NULL;
	}
	if((bytes_read = read(fd, chr, file_size)) == -1) {
		perror("Failed to read from input file");
		free(chr);
		close(fd);
		return NULL;
	}
	if (close(fd) == -1) {
		perror("Failed to close input file");
	}
	if (bytes_read < file_size) {
		printf("There should be %d number of elements in the file\n", file_size);
		free(chr);
		return NULL;
	}

	int P = size;
	int C = size;
	int D = size;
	for (int i = 0; i < file_size; ++i) {
		if (chr[i] == 'P')
			P--;
		else if (chr[i] == 'C')
			C--;
		else if (chr[i] == 'D')
			D--;
		else {
			printf("There are insufficient characters in input file\n");
			free(chr);
			return NULL;
		}
	}
	if (P != 0 || C != 0 || D != 0) {
		printf("There should be %d number of plates for each type of food.\n", size);
		free(chr);
		return NULL;
	}

	return chr;
}
