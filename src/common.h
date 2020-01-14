#ifndef COMMON_H
#define COMMON_H

#include <sys/socket.h>

#define DEFAULT_SERVER_PORT "1234"

enum server_response {
    // la richiesta è andata a buon fine
    SRESP_OK,

    // la richiesta era valida ma non è stato possibile portarla a termine
    SRESP_RETRY,

    // si è verificato un errore durante l'esecuzione della richiesta
    SRESP_ERR,

    // la richiesta non era valida
    SRESP_BADREQ,

    // usato solo per contare il numero di elementi dentro l'enum
    SRESP_NUM
};

extern char whoiam[70];
void die(const char *msg);

char *sockaddr_to_string(struct sockaddr *sa);

int recv_msg(int sockfd, char **s);
int send_msg(int sockfd, char *msg);

int regex_match(const char *regex_txt, const char *str, char **matches[]);
void regex_match_free(char **matches[], int nmatches);


#endif