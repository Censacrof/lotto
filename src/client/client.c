#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "../common.h"

// riceve un messaggio dal server ed estrae il campo codice di risposta ed info.
// per info viene allocata dinamicamente una stringa. ricordarsi di usare free.
// restituisce la lunghezza della risposta ricevuta (0 se la connessione è stata chiusa) o -1 in caso di errore.
int get_response(int sockfd, int *code, char **info)
{
    char *resp;
    int resplen;
    if ((resplen = recv_msg(sockfd, &resp)) == -1)
    {
        consolelog("impossibile ricevere un messaggio\n");
        return -1;
    }

    char **matches = NULL;
    int nmatches = regex_match("^([0-9]+)([ \\n\\r\\t]+(.*))?", resp, &matches);
    free(resp);

    // se non ci sono matches
    if (nmatches == 0)
    {
        consolelog("la risposta è in un formato sconosciuto:\n%s\n");;
        return -1;
    }

    // estraggo il codice di risposta
    sscanf(matches[1], "%d", code);

    // se la risposta non contiene info
    if (nmatches < 4)
    {
        // metto una stringa vuota in info
        *info = malloc(sizeof(char) * 1);
        (*info)[0] = '\0';
    }

    // altrimenti la risposta contiene info
    else
    {
        // copio il match in info
        int len = strlen(matches[3]);
        *info = malloc(sizeof(char) * (len + 1));
        strcpy(*info, matches[3]);
    }

    regex_match_free(&matches, nmatches);
    return resplen;
}

int main(int argc, char *argv[])
{

    // creo il socket di comunicazione
    int server_sock;
    if ((server_sock = socket(
        AF_INET,                // IPv4
        SOCK_STREAM,            // TCP
        0
    )) == -1)
        die("impossibile creare un socket");
    
    int port;
    sscanf(DEFAULT_SERVER_PORT, "%d", &port);
    
    // creo l'indirizzo del server
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    // creo la connessione
    if (connect(server_sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1)
        die("impossibile connettersi");

    // ricevo dal server il messaggio di benvenuto (o di non benuto se siamo in blacklist)
    int resplen;
    int code;
    char *info;
    if ((resplen = get_response(server_sock, &code, &info)) <= 0)
    {
        consolelog("impossibile ottenere messaggio di benvenuto");
        exit(EXIT_FAILURE);
    }

    if (code != SRESP_OK)
    {
        consolelog("il server ha chiuso la connessione: %s", info);
        close(server_sock);
        exit(EXIT_SUCCESS);
    }

    // main loop
    while (1)
    {
        
    }
    
    close(server_sock);
    exit(EXIT_SUCCESS);
}