// Michael Vinciguerra and Varun Ravichandran
// Assignment 3
// Multi-Threaded Bank System
// client.c
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

#include "client.h"

char *accountInService; // String representing the account name currently in service, will be passed back by the server upon success of operations
int *serverRunning; // mutex0
pthread_mutex_t mutex0;

// Thread used exclusively to send commands to server
// Must include standard user error checking that the server shouldn't have to deal with
void *commandInputThread(void *param){
	// Set up error checking
	int sockfd = *((int *)param);
	accountInService = NULL; // no account should be in service in the beginning

	char buf[256]; //buffer for server communication
	char temp[256]; //buffer for user communication

	while(*serverRunning) {
		int incorrectInput = 0; 
		char *prompt = "Welcome to Mike and Varun's Bank! Please enter one of the following commands:\ncreate - to create a new account\nserve - to open a new service session with an existing account\ndeposit - to add money to an account you have a service session with\nwithdraw - to extract money from an account you have a service session with\nquery - to return the current account balance from an account you have a service session with\nend - to end an existing service session\nquit - to exit this window entirely.\n";
		write(STDOUT, prompt, sizeof(char)*strlen(prompt));
		bzero(temp, 256);
		char *message;
		//temp[0] = '\0';
		if (read(STDIN, temp, sizeof(char)*255) < 0) {
			char *errorMessage = "Unable to read from standard input. Aborting program.\n";
			writeFatalError(errorMessage);
			pthread_exit(NULL);
		}
		char temp2[256];
		bzero(temp2, 256);
		strcpy(temp2, temp);
		//parse message and send appropriate command to the server. Eventually post-parsing part will be handled in separate thread
		char *token = strtok(temp, " "); //gets first token from user input that occurs before a space. This should be create/serve/deposit/withdraw/query/end/quit
		char *serverMessage = (char *)malloc(sizeof(char)*256);
		memset(serverMessage, 0, sizeof(char)*256);
		int count = 0;
		while (token != NULL) {
			if (count == 2) { //too many spaces!
				char *errorMessage = "Incorrect input format. Try again.\n";
				write(STDOUT, errorMessage, sizeof(char)*strlen(errorMessage));
				incorrectInput = 1;
				break;
			}
			else {
				if (count == 0) { //there is a space and we are looking at first word, so it should be create, serve, deposit, or withdraw
					if (strcmp(token, "create") == 0) {
						char character[2] = "c";
						strcpy(serverMessage, character);
					}
					else if (strcmp(token, "serve") == 0) {
						char character[2] = "s";
						strcpy(serverMessage, character);
					}
					else if (strcmp(token, "deposit") == 0) {
						char character[2] = "d";
						strcpy(serverMessage, character);
					}
					else if (strcmp(token, "withdraw") == 0) {
						char character[2] = "w";
						strcpy(serverMessage, character);
					}
					else if (strcmp(token, "quit\n") == 0 || strcmp(token, "query\n") == 0 || strcmp(token, "end\n") == 0) {
						strcpy(serverMessage, token);
					}
					else {
						char *errorMessage = "Invalid command. Try again.\n";			
						write(STDOUT, errorMessage, sizeof(char)*strlen(errorMessage));
						incorrectInput = 1;
						break;
					}
				}
				else if (count == 1) { //there is a space and the command was valid, so the rest of it must be the account name
					if (strcmp(token, "\n") == 0) {
						char *errorMessage = "Invalid command. Try again.\n";			
						write(STDOUT, errorMessage, sizeof(char)*strlen(errorMessage));
						incorrectInput = 1;
						break;
					}
					strcat(serverMessage, token); //just concatenate the rest of the string, so now serverMessage = first letter of command + accountName
				}
			}
			count++;
			token = strtok(NULL, " "); //get next token in the string
		}
		/*if (count == 0) { //if the command had no spaces, then it must be query, end, or quit
			if (strcmp(temp2, "query\n") == 0 || strcmp(temp2, "quit\n") == 0 || strcmp(temp2, "end\n") == 0) {
				strcpy(serverMessage, temp);
			}
			else {
				char *errorMessage = "Invalid command. Try again.\n";			
				write(STDOUT, errorMessage, sizeof(char)*strlen(errorMessage));
				incorrectInput = 1;
			}
		}
		else if (count == 1) { //if user passed valid command with a space and then no account name, then it's wrong
			char *errorMessage = "Invalid input. You need to specify the account name to perform the requested action.\n";
			write(STDOUT, errorMessage, sizeof(char)*strlen(errorMessage));
			incorrectInput = 1;
		}*/
		if (incorrectInput == 0) {
			//serverMessage must now be populated with the properly parsed message based on the user input
			// firstMessage should look something like this -> "10:c"
			// The numbers serve to give the size of the message being sent
			// The ':' acts as a separator that the server will look for to parse the input
			// If there are any remaining characters that we need to fill up, we will fill them
			// with the first couple of characters from the serverMessage;
			char firstMessage[4];
			memset(firstMessage,0,sizeof(char)*4);
			char numChars[4]; // should be a max of three digits long
			memset(numChars,0,sizeof(char)*3);
			sprintf(numChars,"%d",strlen(serverMessage));
			strcat(firstMessage,numChars);
			strcat(firstMessage,":"); // acts as separator
			int size = strlen(numChars) + 1; // add one for the separator
			int charCount = 0; // should go only to a max of 1
			while (size < 4){
				char addOn = serverMessage[charCount]; // starting at beginning of string
				firstMessage[size] = addOn;
				size++;
				charCount++;
			}
	
			if (write(sockfd, firstMessage, sizeof(char)*4) < 0) { //passes firstMessage onto the server 
				char *errorMessage = "Unable to write to the server. Aborting program.\n";
				writeFatalError(errorMessage);
				pthread_exit(NULL);
			}
			
			if (write(sockfd, serverMessage, sizeof(char)*strlen(serverMessage)) < 0) { //passes serverMessage onto the server 
				char *errorMessage = "Unable to write to the server. Aborting program.\n";
				writeFatalError(errorMessage);
				pthread_exit(NULL);
			}	
		}

		sleep(2); //throttle user input for 2 seconds
	}
	
	// Assume the output thread has closed the socket at this point
	pthread_exit(NULL);
}

// Thread used exclusively to read diagnostic output from server
void *responseOutputThread(void *param){
	int sockfd = *((int *)param);
	char buf[256]; //buffer for server communication

	while (*serverRunning){
		bzero(buf, 256);
		if (read(sockfd, buf, 255) < 0) {
			char *errorMessage = "Unable to read from the server. Aborting program.\n";
			writeFatalError(errorMessage);	
			pthread_exit(NULL);
		}

		if (strcmp(buf,"quit") == 0){ // received shutdown message from server
			pthread_mutex_lock(&mutex0);
			*serverRunning = 0; // server has shut us down
			pthread_mutex_unlock(&mutex0);
			close(sockfd);
			pthread_exit(NULL); // start closing down everything
		}

		write(STDOUT, buf, sizeof(char)*strlen(buf));
		char newLine = '\n';
		write(STDOUT, &newLine, sizeof(char));
	}
	
	pthread_exit(NULL);
}

int main (int argc, char **argv) {
	//Command line input handling
	if (argc != 3) {
		char *errorMessage = "Missing command line arguments. Aborting program.\n";
		writeFatalError(errorMessage);
		return -1;
	}
	char* machine = argv[1]; //extracts the address of the host machine from user input
	int port = atoi(argv[2]); //extracts necessary port number from the user input

	struct hostent *serverIP;
	int sockfd;
	struct sockaddr_in serverAddress;
	char buf[256]; //buffer for server communication
	
	serverIP = gethostbyname(machine); //creates hostent struct from the provided address if possible
	if (!serverIP) {
		char *errorMessage = "Unable to find a host with the provided address. Aborting program.\n";
		writeFatalError(errorMessage);
		return -1;	
	}
	sockfd = socket(AF_INET, SOCK_STREAM, 0); //creates socket and sockfd stores file descriptor
	if (sockfd == -1) {
		char *errorMessage = "Unable to open a socket. Aborting program.\n";
		writeFatalError(errorMessage);
		return -1;
	}
	bzero((char *)&serverAddress, sizeof(serverAddress)); //zeros all contents in the struct
	serverAddress.sin_port = htons(port); //sets port number in the server address struct
	serverAddress.sin_family = AF_INET; //selects TCP as type of network address
	bcopy((char *)serverIP->h_addr, (char *)&serverAddress.sin_addr.s_addr, serverIP->h_length); //bytewise raw copy from serverIP hostent to relevant part of serverAddress
	while (connect(sockfd, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {
		write(STDOUT, "Attempting to connect to server, please wait...\n", 48);
		sleep(3); //if unable to connect to the server, wait 3 seconds and try again
	}

	bzero(buf, 256);
	if (read(sockfd, buf, 255) < 0) {
		char *errorMessage = "Unable to read from the server. Aborting program.\n";
		writeFatalError(errorMessage);	
		return -1;
	}
	//try to look for server's acceptance message and output that
	write(STDOUT, buf, sizeof(char)*strlen(buf));
	char newLine = '\n';
	write(STDOUT, &newLine, sizeof(char));

	// Setting up parameters to be passed to each thread
	serverRunning = (int *)malloc(sizeof(int));
	*serverRunning = 1;
	int *socketfd = (int *)malloc(sizeof(int));
	*socketfd = sockfd;

	if (pthread_mutex_init(&mutex0, NULL) != 0) {
		writeFatalError("Mutex 0 could not be properly initialized.\n");
		return -1;
	}

	// Now that we know the server is all ready to go, we should set up the two threads that we need to use
	pthread_t *commandThreadHandle = (pthread_t *)malloc(sizeof(pthread_t));
	pthread_attr_t commandThreadAttr;
	if (pthread_attr_init(&commandThreadAttr) != 0) {
		writeFatalError("Thread attributes not initialized properly. Exiting program.\n");
		return -1;
	}

	pthread_create(commandThreadHandle, &commandThreadAttr, commandInputThread, (void *)socketfd);

	pthread_t *outputThreadHandle = (pthread_t *)malloc(sizeof(pthread_t));
	pthread_attr_t outputThreadAttr;
	if (pthread_attr_init(&outputThreadAttr) != 0) {
		writeFatalError("Thread attributes not initialized properly. Exiting program.\n");
		return -1;
	}

	pthread_create(outputThreadHandle, &outputThreadAttr, responseOutputThread, (void *)socketfd);

	pthread_join(*commandThreadHandle, NULL);
	pthread_join(*outputThreadHandle, NULL);

	return 0;
}
