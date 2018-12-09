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

#include "server.h"

//Global Variables
Node *head; //stores the head of the linked list
Node *current; //stores the current node that has a NULL account name and will be initialized on create
int totalAccounts; //stores the total number of accounts currently in the bank
double bal;
int serv;
int main (int argc, char **argv) {
	//Comamnd line input handling
	if (argc != 2) {
		char *errorMessage = "Missing command line arguments. Aborting program.\n";
		writeFatalError(errorMessage);
		return -1;
	}
	int port = atoi(argv[1]);
	int sockfd, acceptsockfd, clientLength;
	char buf[256];
	struct sockaddr_in serverAddress, clientAddress;
	char *message; //the message to be sent from server to client

	//initializing memory for the global data structure of account nodes
	head = (Node *)malloc(sizeof(Node));
	current = (Node *)malloc(sizeof(Node));
	current = head;
	totalAccounts = 0;

	sockfd = socket(AF_INET, SOCK_STREAM, 0); //creates a socket connection and generates a file descriptor for it
	if (sockfd == -1) {
		char *errorMessage = "Server unable to create socket. Aborting program.\n";
		writeFatalError(errorMessage);
		return -1;	
	} 
	bzero((char *)&serverAddress, sizeof(serverAddress)); //zeros everything in server address struct
	serverAddress.sin_family = AF_INET; //establishes TCP as family of connection
	serverAddress.sin_port = htons(port); //sets serverAddress struct's port info to the user-specified port
	serverAddress.sin_addr.s_addr = INADDR_ANY; //sets server's address to the IP address of the current machine it's running on

	if (bind(sockfd, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) { //attempt to weld the socket to the specified port
		char *errorMessage = "Server unable to bind socket to the specified port. Aborting program.\n";
		writeFatalError(errorMessage);
		return -1;	
	}
	//TODO: spawn the a new thread that will listen and spawn new threads as necessary
	listen(sockfd, 5); //listening for client connections to the port
	clientLength = sizeof(clientAddress);
	acceptsockfd = accept(sockfd, (struct sockaddr *)&clientAddress, &clientLength); //blocks the process up until the connection with the client has been established
	if (acceptsockfd < 0) {
		char *errorMessage = "Server unable to accept the client's request to connect at the specified port. Aborting program.\n";
		writeFatalError(errorMessage);
		return -1;	
	}
	message = "Server has successfully accepted the connection request from the client.\n";
	write(acceptsockfd, message, sizeof(char)*strlen(message)); //confirmation to client that server has accepted client request
	write(STDOUT, message, sizeof(char)*strlen(message)); //not sure if server also has to announce acceptance confirmation directly to its STDOUT, so I'm doing both
	
	/*******************
	BORDER WHERE YOU SPAWN A NEW THREAD TO HANDLE EVERY INDIVIDUAL ACCOUNT
	********************/

	//allocating memory for a new account. Will not be added into the overall list of accounts with default values unless thread receives a create message from client
	Account *account = (Account *)malloc(sizeof(Account));
	//memset(account, 0, sizeof(account));
	//account->name = NULL; //identifier, we can do null check on name to see whether there is a local account stored in this thread
	memset(account->name, 0, sizeof(char)*strlen(account->name));
	strcpy(account->name, " "); //temporary identifier to know whether the name is uninitialized
	while (1) {
		bzero(buf, 256); //clears out the buffer
		int error = 0;
		if (read(acceptsockfd, buf, 4) < 0) { //attempting to read in request from the client. For now server just says what it's going to do
			char *errorMessage = "Server unable to accept the client's request to connect at the specified port. Aborting program.\n";
			write(acceptsockfd, errorMessage, sizeof(char)*strlen(errorMessage));
			return -1;	
		}
		int i;
		char numBytes[4];
		for (i = 0; i < 4; i++) {
			if (foundChars == 0) {
				if(buf[i] == ':') {
					numBytes[i] = '\0';
					break;
				}
				else {
					numBytes[i] = buf[i];
				}	
			}
		}
		memset(buf, 0, sizeof(char)*strlen(buf));
		int messageLength = atoi(numBytes);
		if (read(acceptsockfd, buf, messageLength) < 0) { //attempting to read in request from the client.
			char *errorMessage = "Server unable to accept the client's request to connect at the specified port. Aborting program.\n";
			write(acceptsockfd, errorMessage, sizeof(char)*strlen(errorMessage));
			return -1;	
		}
		write(STDOUT, buf, sizeof(char)*strlen(buf));
		char *commandInfo = (char *)malloc(sizeof(char)*strlen(buf)-1);
		memset(commandInfo, 0, sizeof(char)*strlen(buf)-1);
		if (strcmp(buf, "quit\n") == 0) {
			write(acceptsockfd, "quit", sizeof(char)*4);
			close(acceptsockfd); //closes connection with this particular client connection
		}
		else if (strcmp(buf, "query\n") == 0) {
			//if query is the first commmand, then account in thread has null name. If name isn't null, check that it's service flag is set to active, otherwise write error
			if ((account->name == NULL) || (*(account->service) == 0)) {
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
			if ((account->name == NULL) || (*(account->service) == 0)) {
				char *errorMessage = "The account you are trying to end a service session for does not have an active service session. Please run serve first to establish a service session.\n"; 
				write(acceptsockfd, errorMessage, sizeof(char)*strlen(errorMessage));
				error = 1;
				continue;
			}
			else {
				*(account->service) = 0; //update locally in the thread
				Node *curr = (Node *)malloc(sizeof(Node));
				curr = head;
				int i;
				for (i = 0; i < totalAccounts; i++) {
					Account *acct = curr->accnt;
					if (strcmp(acct->name, account->name) == 0) {
						memcpy(acct, account, sizeof(Account *));
						break;
					}
					curr = curr->next;
				}
				char *successMessage = "The service session for the account has been ended.\n";
				write(acceptsockfd, successMessage, sizeof(char)*strlen(successMessage));
			}
		}
		else { //create, serve, deposit, and withdraw are all indicated by the first char of buf
			memcpy(commandInfo, &buf[1], sizeof(char)*(strlen(buf)-1)); //gets bytes 1-end of buffer into commandInfo
			//write(STDOUT, commandInfo, sizeof(char)*strlen(commandInfo));
			Node *curr = (Node *)malloc(sizeof(Node));
			switch(buf[0]) {
				case 'c': //create
					curr = head;
					int repeat = 0;
					//loop through the global list of accounts and see if any account has the same name as the one provided
					int i;
					for (i = 0; i < totalAccounts; i++) {
						Account *acct = curr->accnt;
						/*if (curr->accnt->name == NULL) {
							//last node in LL contains an account with null name, it's where the new account will be added
							break;
						}*/
						if (strcmp(acct->name, account->name) == 0) {
							char *errorMessage = "Please choose a different name, there's already an account with the name you entered.\n";
							write(acceptsockfd, errorMessage, sizeof(char)*strlen(errorMessage));
							repeat = 1;
						}
						curr = curr->next;
					}
					if (repeat == 1)
						break;
					//no accounts have same name, so we can proceed with account creation!
					memset(account->name, 0, sizeof(char)*strlen(account->name)); //clearing the previous null data
					strcpy(account->name, commandInfo); //setting the new name
					*(account->balance) = 0.0; //balance is default at 0.0
					*(account->service) = 0; //initially no service connection to this account exists
					//updating the global data structure by creating a new Node and adding it to the front of the list
					memcpy(current->accnt, account, sizeof(Account *)); 
					current->next = (Node *)malloc(sizeof(Node *));
					current = current->next; //last node of LL (current) gets contents of account copied over into it and move onto next node in LL for next addition
					char *successMessage = "The account has been created successfully.\n";
					write(acceptsockfd, successMessage, sizeof(char)*strlen(successMessage));
					break;
				case 's': //serve
					memset(account->name, 0, sizeof(char)*strlen(account->name)); //clearing the previous null data
					int found = 0;
					memset(curr, 0, sizeof(Node *));
					curr = head;
					while (curr != NULL) {
						Account *acct = curr->accnt;
						if (strcmp(acct->name, commandInfo) == 0) {
							memcpy(account, acct, sizeof(Account *));
							*(account->service) = 1;
							//strcpy(account->name, acct->name);
							//bal = *(acct->balance);
							//account->balance = &bal;
							//*(account->service) = 1;
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
					if ((account->name == NULL) || (*(account->service) == 0)) {
						char *errorMessage = "The account you are trying to deposit money into does not have an active service session. Please run serve first to establish a service session.\n"; 
						write(acceptsockfd, errorMessage, sizeof(char)*strlen(errorMessage));
					}
					else {
						*(account->balance) = *(account->balance) + atof(commandInfo); //updating deposit locally
						memset(curr, 0, sizeof(Node *));
						curr = head;
						while (curr != NULL) {
							Account *acct = curr->accnt;
							if (strcmp(acct->name, account->name) == 0) {
								memcpy(acct, account, sizeof(Account *));
								break;
							}
							curr = curr->next;
						}
						char *successMessage = "The specified amount was deposited successfully into the account.\n";
						write(acceptsockfd, successMessage, sizeof(char)*strlen(successMessage));		
					}
					break;
				case 'w':
					if ((account->name == NULL) || (*(account->service) == 0)) {
						char *errorMessage = "The account you are trying to withdraw money from does not have an active service session. Please run serve first to establish a service session.\n"; 
						write(acceptsockfd, errorMessage, sizeof(char)*strlen(errorMessage));
					}
					else {
						if (atof(commandInfo) > *(account->balance)) {
							char *errorMessage = "You cannot withdraw this amount because it is more than what your account currently has in it.\n";
							write(acceptsockfd, errorMessage, sizeof(char)*strlen(errorMessage));
						}
						else {
							*(account->balance) = *(account->balance) - atof(commandInfo);
							memset(curr, 0, sizeof(Node *));
							curr = head;
							while (curr != NULL) {
								Account *acct = curr->accnt;
								if (strcmp(acct->name, account->name) == 0) {
									memcpy(acct, account, sizeof(Account *));
									break;
								}
								curr = curr->next;
							}
							char *successMessage = "The requested amount was successfully withdrawn from the account currently being served.\n";
							write(acceptsockfd, successMessage, sizeof(char)*strlen(successMessage));
						}
					}
					break;
			}
			//outputs all current account's information
			char outputBuf[256];
			bzero(outputBuf,256);
			outputBuf[0] = '\0';
			curr = head;
			while (curr != NULL) {
				Account *acc = curr->accnt;
				if ((acc->name) != NULL) {
					sprintf(outputBuf, "Account balance: %f\nAccount name : %s\n Account Service Status: %d\n", *(acc->balance), acc->name, *(acc->service));
					write(STDOUT, outputBuf, sizeof(char)*strlen(outputBuf));
				}	
			}
			
			
		}
	}
	return 0;
}