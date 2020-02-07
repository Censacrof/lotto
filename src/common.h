#ifndef COMMON_H
#define COMMON_H

#include <sys/socket.h>

#define DEFAULT_SERVER_PORT "1234"
#define SESSIONID_LEN 10

enum server_response {
    // la richiesta è andata a buon fine
    SRESP_OK,

    // il server chiede di rientare l'invio di dati
    SRESP_RETRY,

    // il server chiede di continuare l'invio di dati
    SRESP_CONTINUE,

    // si è verificato un errore durante l'esecuzione della richiesta
    SRESP_ERR,

    // la richiesta non era valida
    SRESP_BADREQ,

    // il sever chiudera' la connessione in seguito a questo messaggio
    SRESP_CLOSE,

    // usato solo per contare il numero di elementi dentro l'enum
    SRESP_NUM
};

extern char whoiam[70];
void die(const char *msg);
void consolelog(const char *format, ...);

char *sockaddr_to_string(struct sockaddr *sa);


enum msg_operation {
    MSGOP_NONE,
    MSGOP_RECV,
    MSGOP_SEND
};

// variabile usata per garantire che le funzioni recv_msg e send_msg 
// vengano chiamate in modo alternato (serve ad evitare bug)
extern enum msg_operation last_msg_operation;

int recv_msg(int sockfd, char **s);
int send_msg(int sockfd, char *msg);

int regex_match(const char *regex_txt, const char *str, char **matches[]);
void regex_match_free(char **matches[], int nmatches);


#endif