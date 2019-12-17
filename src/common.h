#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define DEFAULT_SERVER_PORT "1234"

void die(const char *msg);

char *sockaddr_to_string(struct sockaddr *sa);

int recv_msg(int sockfd, char **s);
int send_msg(int sockfd, char *msg);

#endif