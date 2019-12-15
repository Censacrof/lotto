#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define DEFAULT_SERVER_PORT "1234"

void die(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}


// estrae l'indirizzo dalla struttura sockaddr e lo converte in stringa null-terminated
char *sockaddr_to_string(struct sockaddr *sa)
{
    char *s = NULL;
    switch (sa->sa_family)
    {
        case AF_INET:
            s = (char *) malloc(sizeof(char) * INET_ADDRSTRLEN + 7); // + 7 perchè: la porta massima è 65535, un carattere è ':', la stringa è null terminated
            inet_ntop(
                AF_INET,
                &(((struct sockaddr_in *) sa)->sin_addr),
                s,
                INET_ADDRSTRLEN
            );

            sprintf(&s[strlen(s)], ":%d", ntohs(((struct sockaddr_in *) sa)->sin_port));
            break;
    
        case AF_INET6:
            s = (char *) malloc(sizeof(char) * INET6_ADDRSTRLEN + 7); // + 7 perchè: la porta massima è 65535, un carattere è ':', la stringa è null terminated
            inet_ntop(
                AF_INET6,
                &(((struct sockaddr_in6 *) sa)->sin6_addr),
                s,
                INET6_ADDRSTRLEN
            );

            sprintf(&s[strlen(s)], ":%d", ntohs(((struct sockaddr_in6 *) sa)->sin6_port));
            break;

        default:
            break;       
    }

    return s;
}


#endif