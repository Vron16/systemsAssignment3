#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<float.h>
#include<dirent.h>
#include<linux/limits.h>
#include<errno.h>

#include "client.h"
#include "server.h"

void writeFatalError(char *errorMessage){
	write(STDERR, errorMessage, sizeof(char)*strlen(errorMessage));
	write(STDOUT, errorMessage, sizeof(char)*strlen(errorMessage));
}

void removeSubstring(char *str, const char *substr){
	while( str=strstr(str,substr) )
    		memmove(str,str+strlen(substr),1+strlen(str+strlen(substr)));
}