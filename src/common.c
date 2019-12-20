#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>

#include "common.h"

void die(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}


// estrae indirizzo e porta dalla struttura sockaddr e lo converte in stringa null-terminated
// bisogna ricordarsi di deallocare la stringa con free quando non serve più
// restituisce NULL in caso di errore
// es: 127.0.0.1:12345
char *sockaddr_to_string(struct sockaddr *sa)
{
    char *s = NULL;
    switch (sa->sa_family)
    {
        case AF_INET:
        {
            s = (char *) malloc(sizeof(char) * INET_ADDRSTRLEN + 7); // + 7 perchè: la porta massima è 65535, un carattere è ':', la stringa è null terminated
            if (!inet_ntop(
                AF_INET,
                &(((struct sockaddr_in *) sa)->sin_addr),
                s,
                INET_ADDRSTRLEN
            ))
                return NULL;

            int port = ntohs(((struct sockaddr_in *) sa)->sin_port); // endianness
            sprintf(&s[strlen(s)], ":%d", port);
            break;
        }
    
        case AF_INET6:
        {
            s = (char *) malloc(sizeof(char) * INET6_ADDRSTRLEN + 7); // + 7 perchè: la porta massima è 65535, un carattere è ':', la stringa è null terminated
            if (!inet_ntop(
                AF_INET6,
                &(((struct sockaddr_in6 *) sa)->sin6_addr),
                s,
                INET6_ADDRSTRLEN
            ))
                return NULL;

            int port = ntohs(((struct sockaddr_in6 *) sa)->sin6_port); // endianness
            sprintf(&s[strlen(s)], ":%d", port);
            break;
        }

        default:
        {
            errno = EINVAL; // invalid argument
            return NULL;
        }
    }

    return s;
}

// riceve un messaggio dal socket sockfd e lo copia su una stringa null terminated 
// allocata dinamicamente di dimensioni opportune.
// restituisce la lunghezza del messaggio oppure -1 in caso di errore. 
// restituisce 0 se la connessione è stata chiusa.
// RICORDARSI DI USARE free.
int recv_msg(int sockfd, char **s)
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
            return -1;
        else if (read == 0)
            return 0;

        n -= read;
        base += read;
    } while(n > 0);
    msg_len = ntohs(msg_len); // endianness

    // alloco un buffer in grado di contenere il messaggio (null terminated)
    *s = (char *) malloc(msg_len + 1);
    if (!*s)
        return -1;

    // ricevo il messaggio sotto forma di testo, non c'è bisogno di tenere conto dell'endianness 
    // perchè i caratteri sono rappresentati su un solo byte
    base = *s;
    n = msg_len;
    do
    {
        int read = recv(sockfd, base, n, 0);
        if (read == -1)
        {
            free(*s);
            return -1;
        }            
        else if (read == 0)
        {
            free(*s);
            return 0;
        }

        n -= read;
        base += read;
    } while(n > 0);
    (*s)[msg_len + 1] = '\0'; // rendo la stringa null terminated

    return msg_len;
}


// invia un messaggio sul socket sockfd. restituisce la lunghezza del messaggio oppure -1 in caso di errori.
// restituisce 0 se la connessione è stata chiusa.
// msg DEVE puntare ad una stringa NULL TERMINATED
int send_msg(int sockfd, char *msg)
{
    // prima della trasmissione del messaggio vero e proprio invio 2 bytes
    // contenenti la lunghezza di quest'ultimo
    int len = strlen(msg);         // da non inviare

    // vieto l'invio di messaggi vuoti
    if (len == 0)
    {
        errno = EPERM; // operation not permitted
        return -1;
    }
        

    uint16_t msg_len = htons(len); // da inviare (endianness)
    int n = 2;
    void *base = (void *) &msg_len;
    do 
    {
        int sent = send(sockfd, base, n, 0);
        if (sent == -1)
            return -1;
        else if (sent == 0)
            return 0;
        
        n -= sent;
        base += sent;
    } while(n > 0);
    
    // invio il messaggio sotto forma di testo, non c'è bisogno di tenere conto dell'endianness 
    // perchè i caratteri sono rappresentati su un solo byte
    n = len;
    base = msg;
    do 
    {
        int sent = send(sockfd, base, n, 0);
        if (sent == -1)
            return -1;
        else if (sent == 0)
            return 0;
        
        n -= sent;
        base += sent;
    } while(n > 0);
    
    return len;
}