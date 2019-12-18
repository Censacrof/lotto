#ifndef COMMON_H
#define COMMON_H

#define DEFAULT_SERVER_PORT "1234"

void die(const char *msg);

char *sockaddr_to_string(struct sockaddr *sa);

int recv_msg(int sockfd, char **s);
int send_msg(int sockfd, char *msg);

#endif