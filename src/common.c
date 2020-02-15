#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <regex.h>
#include <errno.h>
#include <stdarg.h>
#include <signal.h>

#include "common.h"

// array di stringe costante che associa ad ogni codice di risposta del server una stringa leggibile
const char server_response_str[SRESP_NUM][16] = {
    [SRESP_OK] = "OK",
    [SRESP_RETRY] = "RETRY",
    [SRESP_CONTINUE] = "CONTINUE",
    [SRESP_ERR] = "ERROR",
    [SRESP_BADREQ] = "BADREQ",
    [SRESP_CLOSE] = "CLOSE"
};

// stringa globale utilizzata per identificare il processo che scrive (tramite die o consolelog) su stdout
char whoiam[70] = "";

// scrive msg seguito dal messaggio di errore corrispondente al valore di errno e termina il processo
void die(const char *msg)
{
    char buff[256];
    sprintf(buff, "%s -> %s", whoiam, msg);
    perror(buff);
    
    // mando il segnale SIGTERM al processo corrente e tutti i processi figlio
    signal(SIGTERM, SIG_IGN);
    kill(-getpid(), SIGTERM);

    exit(EXIT_FAILURE);
}

// come printf ma stampa anche il valore della variabile whoami
void consolelog(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    printf("%s -> ", whoiam);
    vprintf(format, args);

    va_end(args);
}

// variabile usata per garantire che le funzioni recv_msg e send_msg 
// vengano chiamate in modo alternato (serve ad evitare bug)
enum msg_operation last_msg_operation;

// riceve un messaggio dal socket sockfd e lo copia su una stringa null terminated 
// allocata dinamicamente di dimensioni opportune.
// restituisce la lunghezza del messaggio oppure -1 in caso di errore. 
// restituisce 0 se la connessione è stata chiusa.
// va usata in modo alternato con send_msg.
// RICORDARSI DI USARE free.
int recv_msg(int sockfd, char **s)
{
    if (last_msg_operation == MSGOP_RECV)
    {
        consolelog("ATTENZIONE: tentativo di usare recv_msg due volte consecutive senza chiamare send_msg nel mezzo");
        return -1;
    }

    last_msg_operation = MSGOP_RECV;

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
    (*s)[msg_len] = '\0'; // rendo la stringa null terminated

    return msg_len;
}


// invia un messaggio sul socket sockfd. restituisce la lunghezza del messaggio oppure -1 in caso di errori.
// restituisce 0 se la connessione è stata chiusa.
// msg DEVE puntare ad una stringa NULL TERMINATED
// va usata in modo alternato con recv_msg.
int send_msg(int sockfd, char *msg)
{
    if (last_msg_operation == MSGOP_SEND)
    {
        consolelog("ATTENZIONE: tentativo di usare send_msg due volte consecutive senza chiamare recv_msg nel mezzo\n");
        return -1;
    }

    last_msg_operation = MSGOP_SEND;

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
        // l'opzione MSG_NOSIGNAL server per far si che non venga inviato il
        // segnale SIGPIPE nel caso la connessione sia stata chiusa dall'altro host
        // e che di default causerebbe la terminazione dell'applicazione
        int sent = send(sockfd, base, n, MSG_NOSIGNAL);
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
        // l'opzione MSG_NOSIGNAL server per far si che non venga inviato il
        // segnale SIGPIPE nel caso la connessione sia stata chiusa dall'altro host
        // e che di default causerebbe la terminazione dell'applicazione
        int sent = send(sockfd, base, n, MSG_NOSIGNAL);
        if (sent == -1)
            return -1;
        else if (sent == 0)
            return 0;
        
        n -= sent;
        base += sent;
    } while(n > 0);
    
    return len;
}


// compila regex, la esegue sulla string str e se matches non è NULL alloca l'array di stringhe *matches
// contenente i match risultati. restituisce il numero di match.
// ricordare di usare regex_match_free
int regex_match(const char *regex_txt, const char *str, char **matches[])
{
    regex_t regex;
    int status;
    char error_buf[256]; // buffer per i messaggi di errore

    // compilo l'espressione regolare
    status = regcomp(
        &regex,
        regex_txt,
        REG_EXTENDED | REG_NEWLINE // abilito la sintassi estesa
    );

    if (status != 0) // c'e' stato un errore
    {
        regerror(status, &regex, error_buf, sizeof(error_buf)); // copio il messaggio di errore in error_buf
        printf("Impossibile compilare l'espressione regolare \"%s\": %s\n", regex_txt, error_buf);
        exit(EXIT_FAILURE);
    }

    // eseguo la regex
    const int max_matches = 20;
    regmatch_t match_offsets[max_matches]; // man regexec
    status = regexec(
        &regex,
        str,
        max_matches,
        match_offsets,
        0
    );

    if (status == REG_NOMATCH) // nessun match
    {
        if (matches)
            *matches = NULL;
        
        regfree(&regex);
        return 0;
    }
    else if (status != 0) // c'e' stato un errore
    {
        regerror(status, &regex, error_buf, sizeof(error_buf)); // copio il messaggio di errore in error_buf
        fprintf(stderr, "Regex match failed: %s\n", error_buf);
        regfree(&regex);
        exit(EXIT_FAILURE);
    }

    // conto il numero di match
    int nmatches;
    for (nmatches = 0; nmatches < max_matches; nmatches++)
        // se l'offset iniziale e' -1 allorra i match sono finiti ed esco dal ciclo
        if (match_offsets[nmatches].rm_so == -1)
            break;
    
    if (matches)
    {
        // alloco un array di stringhe
        *matches = (char **) malloc(sizeof(char *) * nmatches);

        // prendo ogni match e lo copio in una nuova stringa all'interno di matches
        int i;
        for (i = 0; i < nmatches; i++)
        {
            char *mbase = (char *) &str[match_offsets[i].rm_so]; // indirizzo del match
            int mlen = match_offsets[i].rm_eo - match_offsets[i].rm_so; // lunghezza del match
            
            // alloco un buffer e ci copio la stringa (null terminated)
            (*matches)[i] = (char *) malloc(sizeof(char *) * (mlen + 1));
            strncpy((*matches)[i], mbase, mlen);
            (*matches)[i][mlen] = '\0'; // rendo la stringa null terminated
        }
    }

    // libero la memoria allocata
    // TODO: implementare il caching delle regex compilate
    regfree(&regex);

    return nmatches;
}

// libera i buffer allocati da regex_match puntati da *matches
void regex_match_free(char **matches[], int nmatches)
{
    int i;
    for (i = 0; i < nmatches; i++)
        free((*matches)[i]);
    free(*matches);
    *matches = NULL;
}