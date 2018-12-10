CC = gcc

all : libraries bankingServer bankingClient

libraries : libraries.c
	$(CC) -c  libraries.c

bankingServer : server.c
	$(CC) server.c libraries.o -g -o bankingServer -lpthread

bankingClient : client.c
	$(CC) client.c libraries.o -g -o bankingClient -lpthread

clean : 
	rm -f bankingServer bankingClient libraries.o
