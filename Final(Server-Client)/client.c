#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define DOMAIN AF_INET

static void commandlineError() {
	char* arg1 = 	"The commandline arguments must include following commands [-apsd]:\n"
					"-a: Ip address of the machine running the server\n"
					"-p: PORT number (must be greater then 0)\n"
					"-s: Source node of the requested path(must be greater then 0)\n"
					"-d: Destination node of the requested path(must be greater then 0)\n";

	if (write(STDERR_FILENO, arg1, strlen(arg1)) == -1) {
		perror("Commandline Error: Failed to print error message");
	}

	exit(EXIT_FAILURE);
}

int client(char* host, int PORT, char* s, char* d);
int init_sockaddr(int sockfd, int PORT, struct sockaddr_in* client_addr);

int main(int argc, char* argv[]) {
	char* host;
	int PORT = -1;
	char* src = NULL;
	char* dest = NULL;

	int cmd = 0;
	while ((cmd = getopt(argc, argv, ":a:p:s:d:")) != -1) {
		switch (cmd) {
			case 'a':
				if ((host = strdup(optarg)) == NULL) {
					perror("Failed to start server!");
					exit(1);
				}
				break;
			case 'p':
				errno = 0;
				PORT = strtol(optarg, NULL, 10);
				if (errno == EINVAL || errno == ERANGE) {
					perror("Failed to start server!");
				}
				break;
			case 's':
				if ((src = strdup(optarg)) == NULL) {
					perror("Failed to start client");
					break;
				}  	
			case 'd':
				if ((dest = strdup(optarg)) == NULL) {
					perror("Failed to start client");
					break;
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

	if (host == NULL || PORT < 0 || src == NULL || dest == NULL) {
		if (host != NULL) {
			free(host);
		}
		commandlineError();
		exit(1);	
	}
	client(host, PORT, src, dest);
}

int client(char* host, int PORT, char* s, char* d) {
	int sockfd;
	struct sockaddr_in client_addr;
	clock_t t;
//	Create a socket
	if ((sockfd = socket(DOMAIN, SOCK_STREAM, 0)) == -1) {
		perror("Failed to create client!");
		return -1;
	}
	init_sockaddr(sockfd, PORT, &client_addr);
	
	printf("Client (%d) connecting to %s:%d\n", sockfd, host, PORT);
	if ((connect(sockfd, (struct sockaddr*)&client_addr, sizeof(struct sockaddr_in))) == -1) {
		perror("Failed to connect to the server");
		return -1;
	}
	printf("Client (%d) connected and requesting a path from node %s to %s\n", sockfd, s, d);

	char* request = (char*)malloc(strlen(s) + strlen(d) + 10);
	strcpy(request, s);
	strcat(request, " ");
	strcat(request, d);
	char buffer[10000];
	t = clock();
	if ((write(sockfd, request, strlen(request))) < 0) {
		perror("Failed to request");
		exit(0);
	}
	t = clock() - t;
	double time = ((double) t) / CLOCKS_PER_SEC;
	if ((read(sockfd, buffer, 10000)) < 0) {
		perror("Failed to request");
		exit(0);
	}

	printf("Server's response to %d: %s, arrived in: %0.1f\n" ,sockfd, buffer, time);
	
	return 0;
}

int init_sockaddr(int sockfd, int PORT, struct sockaddr_in* client_addr) {
	client_addr->sin_family = DOMAIN;
	client_addr->sin_port = htons(PORT);
	client_addr->sin_addr.s_addr = htonl(INADDR_ANY);
	return 0;
}