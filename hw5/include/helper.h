#ifndef HELPER_H
#define HELPER_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <poll.h>

#define FORMAT " %30[^ ,\n\t]"

struct website {
	struct website* next;
	struct website* child;
	int country_count;
 	int socketfd;
	float avg_duration;
	float avg_user_per_year;
    int user_count_per_country[10]; // CN
    pthread_t tid;
    char country[3];
    char name[126];
};
typedef struct website website;

static void addWebsite(website** web);
static void recursiveFree(website* web);

static bool countryExists(char* countries[10], char* country);
static int getCountryIndex(char* countries[10], char* country);

#endif