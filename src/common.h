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


// estrae indirizzo e porta dalla struttura sockaddr e lo converte in stringa null-terminated
// bisogna ricordarsi di deallocare la stringa con free quando non serve più
// es: 127.0.0.1:12345
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



// riceve un messaggio dal socket sockfd e lo copia su una stringa null terminated 
// allocata dinamicamente di dimensioni opportune.
// restituisce il puntatore alla stringa o NULL in caso di errore.
// RICORDARSI DI USARE free
char *recv_msg(int sockfd)
{
    // prima della trasmissione del messaggio vero e proprio il peer manda 2 bytes
    // contenenti la lunghezza di quest'ultimo
    uint16_t msg_len;
    int n = 2;
    void *base = (void *) &msg_len;
    do 
    {
        int read = recv(sockfd, base, n, 0);
        if (read == -1)
            return NULL;
        
        n -= read;
        base += read;
    } while(n > 0);
    msg_len = ntohs(msg_len); // endianness

    // alloco un buffer in grado di contenere il messaggio (null terminated)
    char *s = malloc(msg_len + 1);
    if (!s)
        return NULL;
    

    // ricevo il messaggio sotto forma di testo, non c'è bisogno di tener conto dell'endianness 
    // perchè i caratteri sono rappresentati su un solo byte
    base = s;
    n = msg_len;
    do
    {
        int read = recv(sockfd, base, n, 0);
        if (read == -1)
            return NULL;
        
        n -= read;
        base += read;
    } while(n > 0);
    s[msg_len + 1] = '\0'; // rendo la stringa null terminated

    return s;
}


#endif