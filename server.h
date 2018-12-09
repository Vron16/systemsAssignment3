#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <linux/limits.h>
#include <signal.h>
#include <pthread.h>
#include <netdb.h>

// Define our values to read and write from
#define STDIN 0
#define STDOUT 1
#define STDERR 2

//holds the relevant information of each account
typedef struct account {
	int *service; //points to int storing 0 or 1 depending on whether the account is being served or not
	double *balance; //contains the balance currently stored in the account
	char name[256]; //stores the name associated with this account, might need to change to char * for thread implementation
} Account;

//abstraction that holds itself and a pointer to the next
typedef struct node {
	Account *accnt; //stores pointer to a specific account struct
	struct node *next; //stores pointer to the next node in the list
} Node;

// abstraction that holds a pthread_t thread handle and its associated socket fd
// this is needed in case the Server is ever interrupted and needs to abruptly shut down
typedef struct package {
	pthread_t *threadHandle;
	int *socketfd;
} Handler;

#endif