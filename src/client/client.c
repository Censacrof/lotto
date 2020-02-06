#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "../common.h"
#include "../data.h"

char sessionid[SESSIONID_LEN + 1] = "";

#define RESPINFO_LEN 1024
struct response {
    enum server_response code;
    char info[RESPINFO_LEN + 1];
};
void echo_response(const struct response *resp)
{
    printf("[%d]: %s\n", resp->code, resp->info);
}

int send_command(int sockfd, const char *command, int argc, char *args[]);
int get_response(int sockfd, struct response *resp, int echo);

int signup(int sockfd, int argc, char *args[]);
int login(int sockfd, int argc, char *args[]);
int invia_giocata(int sockfd, int argc, char *args[]);

int main(int shellargc, char *shellargv[])
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
    struct response resp;
    if ((resplen = get_response(server_sock, &resp, 1)) <= 0)
    {
        consolelog("impossibile ottenere messaggio di benvenuto");
        exit(EXIT_FAILURE);
    }

    if (resp.code != SRESP_OK)
    {
        consolelog("il server ha chiuso la connessione: %s", resp.info);
        close(server_sock);
        exit(EXIT_SUCCESS);
    }

    // main loop
    while (1)
    {
        // promtp
        printf("\n> ");

        // leggo un comando
        char userinput[1024];
        fgets(userinput, sizeof(userinput) / sizeof(char), stdin);

        // estraggo comando e argomenti
        char **matches;
        int nmatches = regex_match("^ *!?([A-z_0-9]+) *(([A-z_0-9]+ *)*) *$", userinput, &matches);
        
        // se il comando non è nel formato corretto continuo
        if (nmatches == 0)
        {
            printf("il formato del comando non è corretto\n");
            continue;
        }

        // copio il comando
        char *command = malloc(sizeof(char) * (strlen(matches[1]) + 1));
        strcpy(command, matches[1]);

        // eseguo la tokenizzazione degli argomenti
        const char delimiters[] = " \n\r\t";
        char **args = NULL;
        int nargs = 0;
        char *token = strtok(matches[2], delimiters);
        while (token)
        {
            nargs++;
            args = realloc(args, sizeof(char *) * nargs);
            args[nargs - 1] = malloc(sizeof(char) * (strlen(token) + 1));
            strcpy(args[nargs - 1], token);
            token = strtok(NULL, delimiters);
        }

        int ret;
        
        // eseguo il comando corrispondente
        if (strcmp(command, "signup") == 0)
            ret = signup(server_sock, nargs, args);
        
        else if (strcmp(command, "login") == 0)
            ret = login(server_sock, nargs, args);
        
        else if (strcmp(command, "invia_giocata") == 0)
            ret = invia_giocata(server_sock, nargs, args);
        
        // comando sconosciuto
        else
            printf("comando sconosciuto\n");

        // libero le risorse che non servono piu'
        regex_match_free(&matches, nmatches);
        free(command);
        for (int i = 0; i < nargs; i++)
            free(args[i]);
        free(args);

        // se la funzione che ha gestito il comando ha restituito -1 esco
        if (ret == -1)
            break;
    }
    
    close(server_sock);
    exit(EXIT_SUCCESS);
}


// invia un comando sul socket sockfd. restituisce la lunghezza del messaggio inviato oppure -1 in caso di errori.
// restituisce 0 se la connessione è stata chiusa.
int send_command(int sockfd, const char *command, int argc, char *args[])
{
    char *msg;
    size_t msglen;

    // apro un mem_stream. permette di scrivere in buffer in memoria allocato dinamicamente la cui dimensione
    // cambia automaticamente con le scritture. scrivo nel buffer tramite fprintf e dopo che avrò chiuso l stream
    // msg punterà al buffer e msglen conterrà lunghezza del buffer. analogo agli stringstream del c++.
    FILE *f = open_memstream(&msg, &msglen);
    if (!f)
        die("(send_command) impossibile aprire memstream");
    
    // se il client ha già ottenuto un sessionid
    if (sessionid[0] != '\0')
        fprintf(f, "S%s!", sessionid);
    else
        fprintf(f, "N!");
    
    // comando
    fprintf(f, "%s", command);

    // argomenti
    for (int i = 0; i < argc; i++)
        fprintf(f, " %s", args[i]);
    
    // chiudo lo stream
    fclose(f);

    // invio il messaggio
    int ret = send_msg(sockfd, msg);
    
    // libero il buffer contenente il messaggio
    free(msg);
    
    return ret;
}

// riceve un messaggio dal server ed estrae il campo codice di risposta ed informazioni aggiuntive.
// restituisce la lunghezza della risposta ricevuta (0 se la connessione è stata chiusa) o -1 in caso di errore.
int get_response(int sockfd, struct response *resp, int echo)
{
    // azzero la struttura
    resp->code = SRESP_ERR;
    resp->info[0] = '\0';

    char *resptxt;
    int ret;
    if ((ret = recv_msg(sockfd, &resptxt)) == -1)
    {
        consolelog("impossibile ricevere un messaggio\n");
        return -1;
    }
    else if (ret == 0)
    {
        consolelog("il server ha chiuso la connessione\n");
        return 0;
    }

    // eseguo la regex adeguata sulla risposta del sever
    char **matches;
    int nmatches = regex_match("^([0-9]+)([ \\n\\r\\t]+(.*))?", resptxt, &matches);
    free(resptxt);

    // se il formato della risposta non è quello aspettato
    if (nmatches != 2 && nmatches != 4)
    {
        consolelog("formato della risposta del server sconosciuto");
        ret = -1;
        goto end;
    }

    // estraggo il codice
    sscanf(matches[1], "%d", (int *) &resp->code);

    // estraggo le info (se sono presenti)
    if (nmatches == 4)
        strncpy(resp->info, matches[3], RESPINFO_LEN);
    
    if (echo)
        echo_response(resp);

end:
    regex_match_free(&matches, nmatches);
    return ret;
}


// ----------------------------- comandi -----------------------------
int signup(int sockfd, int argc, char *args[])
{
    if (argc != 2)
    {
        printf("numero di parametri errato\n");
        return 0;
    }

    if (send_command(sockfd, "signup", argc, args) <= 0)
        return -1;

    struct response resp;
    while (1)
    {
        // attendo la risposta dal server
        if (get_response(sockfd, &resp, 1) == -1)
            return -1;
        
        switch (resp.code)
        {
            case SRESP_RETRY:
            {
                printf("\tusername: ");
                char username[USERNAME_LEN + 1];
                fgets(username, USERNAME_LEN, stdin);
                
                send_msg(sockfd, username);
                continue;
            }                
            
            case SRESP_OK:
                return 0;
        
            default:
                return -1;
        }
    }

    return 0;
}

int login(int sockfd, int argc, char *args[])
{
    if (argc != 2)
    {
        printf("numero di parametri errato\n");
        return 0;
    }

    if (send_command(sockfd, "login", argc, args) <= 0)
        return -1;
    
    struct response resp;
    if (get_response(sockfd, &resp, 0) <= 0)
        return -1;
    
    if (resp.code == SRESP_OK && strlen(resp.info) == SESSIONID_LEN)
    {
        strcpy(sessionid, resp.info);
        printf("login effettuato con successo\n");
    }
    else
    {
        printf("impossibile effettuare login\n");
        echo_response(&resp);
        return -1;
    }    

    return 0;
}

int invia_giocata(int sockfd, int argc, char *args[])
{
    if (send_command(sockfd, "invia_giocata", argc, args) <= 0)
        return -1;
    
    struct response resp;
    if (get_response(sockfd, &resp, 1) <= 0)
        return -1;
    
    return 0;
}