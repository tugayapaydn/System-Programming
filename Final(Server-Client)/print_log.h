#ifndef PRINT_LOG_H
#define PRINT_LOG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

typedef struct LOGGER {
	int file_log;
} LOGGER;

LOGGER logger; 

int print_log(char* msg);
#endif