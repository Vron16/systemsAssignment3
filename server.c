// Michael Vinciguerra and Varun Ravichandran
// Assignment 3
// Multi-Threaded Bank System
// server.c
//*****************************************************************
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
#include <semaphore.h>
#include <sys/time.h>

#include "server.h"

//Global Variables
Node *head; //stores the head of the linked list
Node *current; //stores the current node that has a NULL account name and will be initialized on create, mutex0 will be used to lock around any changes to the LinkedList
int *totalAccounts; //stores the total number of accounts currently in the bank, mutex1
Handler **handles; // stores all opened thread handlesand socket file descriptors, mutex2
int *numSessions; // stores the total number of sessions currently in use, mutex3
int *bufferSize; // a dynamically sized buffer used for resizing of handles array, mutex4
pthread_mutex_t mutex0, mutex1, mutex2, mutex3, mutex4; // mutexes for our data, see the variables above
sem_t binarySem; // binary semaphore that will be used for the SIGALARM

void sigAlarmHandler(int signal){
	sem_wait(&binarySem); //wait until other threads finish up
	// Print diagnostic output
	//outputs all current account's information
	char *greeting = "SERVER DIAGNOSTIC INFORMATION\n";
	write(STDOUT,greeting,sizeof(char)*strlen(greeting));
	char outputBuf[500];
	bzero(outputBuf,500);
	outputBuf[0] = '\0';
	Node *curr = (Node *)malloc(sizeof(Node));
	curr = head;
	int i;
	for (i = 0; i < *totalAccounts; i++) {
		Account *acc = curr->accnt;
		char *inServiceStatus;
		if (*(acc->service)){
			inServiceStatus = "IN SERVICE";
		} else {
			inServiceStatus = "NOT IN SERVICE";
		}
		sprintf(outputBuf, "Account name : %s\tAccount balance: %f\tAccount Service Status: %s\n", *(acc->balance), acc->name, inServiceStatus);
		write(STDOUT, outputBuf, sizeof(char)*strlen(outputBuf));
		curr = curr->next;	
	}
	sem_post(&binarySem); // unlock the semaphore
}


/*******************
	BORDER WHERE YOU SPAWN A NEW THREAD TO HANDLE EVERY INDIVIDUAL ACCOUNT
********************/
void * clientServiceThread(void *param){
	//allocating memory for a new account. Will not be added into the overall list of accounts with default values unless thread receives a create message from client
	Account *account = NULL;
	int clientEnd = 0;
	int acceptsockfd = *((int *)param);
	char buf[256];
	
	while (clientEnd == 0) {
		bzero(buf, 256); //clears out the buffer
		int error = 0;
		if (read(acceptsockfd, buf, 4) < 0) { //attempting to read in request from the client. For now server just says what it's going to do
			char *errorMessage = "Server unable to accept the client's request to connect at the specified port. Aborting program.\n";
			write(acceptsockfd, errorMessage, sizeof(char)*strlen(errorMessage));
			pthread_exit(NULL);	
		}
		int i;
		char numBytes[4];
		for (i = 0; i < 4; i++) {
			if(buf[i] == ':') {
				numBytes[i] = '\0';
				break;
			}
			else {
				numBytes[i] = buf[i];
			}	
		}
		memset(buf, 0, sizeof(char)*256);
		int messageLength = atoi(numBytes);
		if (read(acceptsockfd, buf, messageLength) < 0) { //attempting to read in request from the client.
			char *errorMessage = "Server unable to accept the client's request to connect at the specified port. Aborting program.\n";
			write(acceptsockfd, errorMessage, sizeof(char)*strlen(errorMessage));
			pthread_exit(NULL);	
		}
		//write(STDOUT, buf, sizeof(char)*strlen(buf));
		char *commandInfo = (char *)malloc(sizeof(char)*strlen(buf)-1);
		memset(commandInfo, 0, sizeof(char)*strlen(buf)-1);
		if (strcmp(buf, "quit\n") == 0) {
			if (account != NULL){ // we need to free an in service account if it exists
				char accountName[256]; //since we have to reset our local copy to null, must store the account name first before going through global LL
				memset(accountName, 0, sizeof(char)*256);
				strcpy(accountName, account->name);
				account = NULL; //update locally in the thread
				pthread_mutex_lock(&mutex0);
				Node *curr = (Node *)malloc(sizeof(Node));
				curr = head;
				int i;
				for (i = 0; i < *totalAccounts; i++) {
					Account *acct = curr->accnt;
					if (strcmp(acct->name, accountName) == 0) {
						sem_wait(&binarySem); // block CS due to actual changes
						*(acct->service) = 0;
						sem_post(&binarySem); // we are done servicing the account
						break;
					}
					curr = curr->next;
				}
			}
			
			write(acceptsockfd, "quit", sizeof(char)*4);
			close(acceptsockfd); //closes connection with this particular client connection
			clientEnd = 1; // client has closed, time to join back
		}
		else if (strcmp(buf, "query\n") == 0) {
			//if query is the first commmand, then account in thread has null name. If name isn't null, check that it's service flag is set to active, otherwise write error
			if ((account == NULL) || (*(account->service) == 0)) {
				char *errorMessage = "The account being queried does not have an active service session. Please run serve first to establish a service session.\n"; 
				write(acceptsockfd, errorMessage, sizeof(char)*strlen(errorMessage));
				error = 1;
				continue;
			}
			else { //valid request
				double balance = *(account->balance);
				char *name = account->name;
				char floatBuf[1000];
				bzero(floatBuf, 1000); //clear it out
				sprintf(floatBuf, "The balance in account %s is: %f\n", name, balance);
				write(acceptsockfd, floatBuf, sizeof(char)*strlen(floatBuf));
				continue;
			}

		}
		else if (strcmp(buf, "end\n") == 0) {
			//check if there is a valid service request on the local account struct stored in the thread
			if ((account == NULL) || (*(account->service) == 0)) {
				char *errorMessage = "The account you are trying to end a service session for does not have an active service session. Please run serve first to establish a service session.\n"; 
				write(acceptsockfd, errorMessage, sizeof(char)*strlen(errorMessage));
				error = 1;
				continue;
			}
			else {
				char accountName[256]; //since we have to reset our local copy to null, must store the account name first before going through global LL
				memset(accountName, 0, sizeof(char)*256);
				strcpy(accountName, account->name);
				account = NULL; //update locally in the thread
				pthread_mutex_lock(&mutex0);
				Node *curr = (Node *)malloc(sizeof(Node));
				curr = head;
				int i;
				for (i = 0; i < *totalAccounts; i++) {
					Account *acct = curr->accnt;
					if (strcmp(acct->name, accountName) == 0) {
						sem_wait(&binarySem); // block CS to make update
						*(acct->service) = 0;
						sem_post(&binarySem); // re-open once update is made
						break;
					}
					curr = curr->next;
				}
				pthread_mutex_unlock(&mutex0);
				char *successMessage = "The service session for the account has been ended.\n";
				write(acceptsockfd, successMessage, sizeof(char)*strlen(successMessage));
				continue;
			}
		}
		else { //create, serve, deposit, and withdraw are all indicated by the first char of buf
			memcpy(commandInfo, &buf[1], sizeof(char)*(strlen(buf)-1)); //gets bytes 1-end of buffer into commandInfo
			//write(STDOUT, commandInfo, sizeof(char)*strlen(commandInfo));
			pthread_mutex_lock(&mutex0);
			Node *curr = (Node *)malloc(sizeof(Node));
			curr = head;
			int i;
			switch(buf[0]) {
				case 'c': ; //create
					int repeat = 0;
					//loop through the global list of accounts and see if any account has the same name as the one provided
					for (i = 0; i < *totalAccounts; i++) {
						Account *acct = curr->accnt;
						/*if (curr->accnt->name == NULL) {
							//last node in LL contains an account with null name, it's where the new account will be added
							break;
						}*/
						if (strcmp(acct->name, commandInfo) == 0) {
							char *errorMessage = "Please choose a different name, there's already an account with the name you entered.\n";
							write(acceptsockfd, errorMessage, sizeof(char)*strlen(errorMessage));
							repeat = 1;
							break;
						}
						curr = curr->next;
					}
					if (repeat == 1) 
						break;
					//no accounts have same name, so we can proceed with account creation!
					sem_wait(&binarySem); // block on operation before SIGALARM goes off
					current->accnt = (Account *)malloc(sizeof(Account)); //malloc'ing space in the GLOBAL database for a new account
					memset(current->accnt->name, 0, sizeof(char)*256); //clearing the previous null data
					strcpy(current->accnt->name, commandInfo); //setting the new name for the current account
					current->accnt->balance = (double *)malloc(sizeof(double)); //malloc'ing space in the GLOBAL database's new account for its balance
					current->accnt->service = (int *)malloc(sizeof(int)); //malloc'ing space in the GLOBAL database's new account for its service flag
					*(current->accnt->balance) = 0.0; //balance is default at 0.0
					*(current->accnt->service) = 0; //initially no service connection to this account exists
					//updating the local account pointer by simply malloc'ing space for a pointer and setting it to point to the newly created global account pointer 
					current->next = (Node *)malloc(sizeof(Node *));
					current = current->next; //last node of LL (current) gets contents of account copied over into it and move onto next node in LL for next addition
					pthread_mutex_lock(&mutex1);
					*totalAccounts = *totalAccounts + 1;
					pthread_mutex_unlock(&mutex1);
					sem_post(&binarySem); // end block so SIGALARM can occur if needed
					char *successMessage = "The account has been created successfully.\n";
					write(acceptsockfd, successMessage, sizeof(char)*strlen(successMessage));
					break;
				case 's': ; //serve
					//memset(account->name, 0, sizeof(char)*256); //clearing the previous null data
					if (account != NULL) {
						char *errorMessage = "A service connection could not be opened to the requested account because another service connection is active.\n";
						write(acceptsockfd, errorMessage, sizeof(char)*strlen(errorMessage));
					}
					int found = 0;
					for (i = 0; i < *totalAccounts; i++) {
						Account *acct = curr->accnt;
						if (strcmp(acct->name, commandInfo) == 0) {
							if (*(acct->service) == 1){
								break;
							}
							account = (Account *)malloc(sizeof(Account));
							sem_wait(&binarySem); // prevent SIGALARM before update occurs
							*(acct->service) = 1;
							sem_post(&binarySem); // unlock semaphore for SIGALARM and other threads
							account = acct;
							found = 1;
							break;
						}
						curr = curr->next;
					}
					if (found) {
						char *successMessage = "A service connection has been opened to the requested account.\n";
						write(acceptsockfd, successMessage, sizeof(char)*strlen(successMessage));
					}
					else {
						char *errorMessage = "A service connection could not be opened to the requested account because it does not exist.\n";
						write(acceptsockfd, errorMessage, sizeof(char)*strlen(errorMessage));
					}
					break;
				case 'd': //deposit
					if ((account == NULL) || (*(account->service) == 0)) {
						char *errorMessage = "The account you are trying to deposit money into does not have an active service session. Please run serve first to establish a service session.\n"; 
						write(acceptsockfd, errorMessage, sizeof(char)*strlen(errorMessage));
					}
					else {
						sem_wait(&binarySem); // prevent SIGALARM from going off
						*(account->balance) = *(account->balance) + atof(commandInfo); //updating deposit locally
						int i;
						for (i = 0; i < *totalAccounts; i++) {
							Account *acct = curr->accnt;
							if (strcmp(acct->name, account->name) == 0) {
								*(acct->balance) = *(account->balance); //the value stored in the global balance is updated with the value now stored in the local balance
								break;
							}
							curr = curr->next;
						}
						char *successMessage = "The specified amount was deposited successfully into the account.\n";
						write(acceptsockfd, successMessage, sizeof(char)*strlen(successMessage));	
						sem_post(&binarySem); // unlock the semaphore for SIGALARM and other threads	
					}
					break;
				case 'w': //withdraw
					if ((account == NULL) || (*(account->service) == 0)) {
						char *errorMessage = "The account you are trying to withdraw money from does not have an active service session. Please run serve first to establish a service session.\n"; 
						write(acceptsockfd, errorMessage, sizeof(char)*strlen(errorMessage));
					}
					else {
						if (atof(commandInfo) > *(account->balance)) {
							//all error messages must be prefaced by sending the length of the error message being sent
							char *errorMessage = "You cannot withdraw this amount because it is more than what your account currently has in it.\n";
							write(acceptsockfd, errorMessage, sizeof(char)*strlen(errorMessage));
						}
						else {
							sem_wait(&binarySem); // prevent SIGALARM from occuring
							*(account->balance) = *(account->balance) - atof(commandInfo);
							int i;
							for (i = 0; i < *totalAccounts; i++) {
								Account *acct = curr->accnt;
								if (strcmp(acct->name, account->name) == 0) {
									*(acct->balance) = *(account->balance); //the value stored in the global balance is updated with the value now stored in the local balance
									break;
								}
								curr = curr->next;
							}
							/*curr = head;
							while (curr != NULL) {
								Account *acct = curr->accnt;
								if (strcmp(acct->name, account->name) == 0) {
									memcpy(acct, account, sizeof(Account *));
									break;
								}
								curr = curr->next;
							}*/
							char *successMessage = "The requested amount was successfully withdrawn from the account currently being served.\n";
							write(acceptsockfd, successMessage, sizeof(char)*strlen(successMessage));
							sem_post(&binarySem); // unlock semaphore for SIGALARM and other threads
						}
					}
					break;
				default: ;
					char *errorMessage = "This shouldn't happen.\n";
					write(acceptsockfd, errorMessage, sizeof(char)*strlen(errorMessage));
					break;
			}
			pthread_mutex_unlock(&mutex0);
		}
	}

	pthread_exit(NULL); // we have reached this point if the client has shut down
}

void * sessionAcceptorThread(void *param){
	int sockfd = *((int *)param);
	char *message; //the message to be sent from server to client
	struct sockaddr_in clientAddress;

	while(1){
		listen(sockfd, 5); //listening for client connections to the port
		int clientLength = sizeof(clientAddress);
		int acceptsockfd = accept(sockfd, (struct sockaddr *)&clientAddress, &clientLength); //blocks the process up until the connection with the client has been established
		if (acceptsockfd < 0) {
			char *errorMessage = "Server unable to accept the client's request to connect at the specified port. Aborting program.\n";
			writeFatalError(errorMessage);
			pthread_exit(NULL);	
		}
		message = "Server has successfully accepted the connection request from the client.\n";
		write(acceptsockfd, message, sizeof(char)*strlen(message)); //confirmation to client that server has accepted client request
		write(STDOUT, message, sizeof(char)*strlen(message)); //not sure if server also has to announce acceptance confirmation directly to its STDOUT, so I'm doing both

		// Set up the new thread
		int *newParam = (int *)malloc(sizeof(int));
		*newParam = acceptsockfd;

		pthread_t *threadHandle = (pthread_t *)malloc(sizeof(pthread_t));
		pthread_attr_t threadAttr;
		if (pthread_attr_init(&threadAttr) != 0) {
			writeFatalError("Thread attributes not initialized properly. Exiting program.");
			pthread_exit(NULL);
		}
	
		void * (*fnPtr)(void *) = clientServiceThread;

		// Update the relevant global variables
		pthread_mutex_lock(&mutex2);
		pthread_mutex_lock(&mutex3);
		if (*numSessions == *bufferSize){ // resize needed on the buffer
			pthread_mutex_lock(&mutex4);
			*bufferSize = *bufferSize * 4;
			Handler temp[*bufferSize];
			memcpy((void *)temp, (void *)(*handles), sizeof(handles));
			*handles = temp;
			pthread_mutex_unlock(&mutex4);
		}

		handles[*numSessions] = (Handler *)malloc(sizeof(Handler));
		handles[*numSessions]->threadHandle = (pthread_t *)malloc(sizeof(pthread_t)); // storing for later use for pthread_join
		handles[*numSessions]->threadHandle = threadHandle;
		handles[*numSessions]->socketfd = (int *)malloc(sizeof(int)); // storing for later use in case of shutdown
		handles[*numSessions]->socketfd = newParam;

		*numSessions = *numSessions + 1;
		pthread_mutex_unlock(&mutex3);
		pthread_mutex_unlock(&mutex2);
		pthread_create(threadHandle, &threadAttr, fnPtr, (void *)newParam);		
	}

	pthread_exit(NULL);
}

int main (int argc, char **argv) {
	//Command line input handling
	if (argc != 2) {
		char *errorMessage = "Missing command line arguments. Aborting program.\n";
		writeFatalError(errorMessage);
		return -1;
	}

	// Set up signal handlers and timer
	struct itimerval myTimer;
	myTimer.it_value.tv_sec = 15; // set timer to 15 seconds
	myTimer.it_value.tv_usec = 0.2; // set uncertainty in timer value
	myTimer.it_interval = myTimer.it_value;
	if(setitimer(ITIMER_REAL, &myTimer, NULL) == -1){
		char *errorMessage = "Couldn't set up itimer.  Aborting server.\n";
		writeFatalError(errorMessage);
		return -1;
	}
	signal(SIGALRM, sigAlarmHandler);

	int port = atoi(argv[1]);
	int* sockfd = (int *)malloc(sizeof(int));
	char buf[256];
	struct sockaddr_in serverAddress;

	//initializing memory for the global data structure of account nodes
	head = (Node *)malloc(sizeof(Node));
	current = (Node *)malloc(sizeof(Node));
	current = head;
	totalAccounts = (int *)malloc(sizeof(int));
	*totalAccounts = 0;
	numSessions = (int *)malloc(sizeof(int)); // keep track of all sessions (active and ended)
	*numSessions = 0;
	bufferSize = (int *)malloc(sizeof(int));
	*bufferSize = 10; // start off with a max of 10 sessions
	handles = (Handler **)malloc(sizeof(Handler *)*(*bufferSize)); // initialize the table that we will use later

	*sockfd = socket(AF_INET, SOCK_STREAM, 0); //creates a socket connection and generates a file descriptor for it
	if (*sockfd == -1) {
		char *errorMessage = "Server unable to create socket. Aborting program.\n";
		writeFatalError(errorMessage);
		return -1;	
	} 

	bzero((char *)&serverAddress, sizeof(serverAddress)); //zeros everything in server address struct
	serverAddress.sin_family = AF_INET; //establishes TCP as family of connection
	serverAddress.sin_port = htons(port); //sets serverAddress struct's port info to the user-specified port
	serverAddress.sin_addr.s_addr = INADDR_ANY; //sets server's address to the IP address of the current machine it's running on

	if (bind(*sockfd, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) { //attempt to weld the socket to the specified port
		char *errorMessage = "Server unable to bind socket to the specified port. Aborting program.\n";
		writeFatalError(errorMessage);
		return -1;	
	}

	// Set up the mutexes
	if (pthread_mutex_init(&mutex0, NULL) != 0) {
		writeFatalError("Mutex 0 could not be properly initialized.\n");
		return -1;
	}
	if (pthread_mutex_init(&mutex1, NULL) != 0) {
		writeFatalError("Mutex 1 could not be properly initialized.\n");	
		return -1;
	}
	if (pthread_mutex_init(&mutex2, NULL) != 0){
		writeFatalError("Mutex 2 could not be properly initialized.\n");
		return -1;
	}
	if (pthread_mutex_init(&mutex3, NULL) != 0){
		writeFatalError("Mutex 3 could not be properly initialized.\n");
		return -1;
	}
	if (pthread_mutex_init(&mutex4, NULL) != 0){
		writeFatalError("Mutex 4 could not be properly initialized.\n");
		return -1;
	}
	if(sem_init(&binarySem, 0, 1) != 0){
		writeFatalError("Semaphore could not be properly initialized.\n");
		return -1;
	}

	//TODO: spawn the a new thread that will listen and spawn new threads as necessary
	pthread_t *threadHandle = (pthread_t *)malloc(sizeof(pthread_t));
	pthread_attr_t threadAttr;
	if (pthread_attr_init(&threadAttr) != 0) {
		writeFatalError("Thread attributes not initialized properly. Exiting program.");
		return -1;
	}
	
	pthread_create(threadHandle, &threadAttr, sessionAcceptorThread, (void *)sockfd);
	pthread_join(*threadHandle, NULL); // wait for sessionAcceptorThread to end
	
	return 0;
}
