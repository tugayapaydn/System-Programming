#include "print_log.h"

int print_log(char* msg) {
	char* buffer = (char*)malloc(strlen(msg) + 50);
	if (buffer == NULL) {
		perror("Failed to print a message to the log file!");
		return -1;
	}
	time_t ttime = time(NULL);
	strcpy(buffer, asctime(localtime(&ttime)));
	strcat(buffer, msg);
	strcat(buffer, "\n");

	if (write(logger.file_log, buffer, strlen(buffer)) < 0) {
		perror("Failed to print a message to the log file!");
		return -1;
	}
	return 0;
}