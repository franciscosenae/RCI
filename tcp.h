#ifndef TCP
#define TCP

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include "functions.h"

int reach_tcp(char *, char *);
int serv_tcp(char *);
int send_tcp(char *, int);
int new_connection(User *);
void dissipate(char *, User *);

#endif
