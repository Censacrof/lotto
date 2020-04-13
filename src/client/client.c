#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>

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
    printf("[%s]: %s\n", server_response_str[resp->code], resp->info);
}

int send_command(int sockfd, const char *command, int argc, char *args[]);
int get_response(int sockfd, struct response *resp, int echo);

int help(int argc, char *args[]);
int signup(int sockfd, int argc, char *args[]);
int login(int sockfd, int argc, char *args[]);
int invia_giocata(int sockfd, int argc, char *args[]);
int vedi_giocate(int sockfd, int argc, char *args[]);
int vedi_estrazione(int sockfd, int argc, char *args[]);
int vedi_vincite(int sockfd);
int esci(int sockfd);

int main(int shellargc, char *shellargv[])
{
    // parametri
    if (shellargc != 3)
    {
    usage:
        printf("uso: %s <IP server> <porta server>\n", shellargv[0]);
        exit(EXIT_SUCCESS);
    }
    
    // estraggo la porta
    int port;
    if (regex_match("^[0-9]+$", shellargv[2], NULL) == 0)
    {
        printf("formato porta non valido\n");
        goto usage;
    }
    else
        sscanf(shellargv[2], "%d", &port);

    // creo l'indirizzo del server
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, shellargv[1], &server_addr.sin_addr) == 0)
    {
        printf("formato IP non valido\n");
        goto usage;
    }

    // creo il socket di comunicazione
    int server_sock;
    if ((server_sock = socket(
        AF_INET,                // IPv4
        SOCK_STREAM,            // TCP
        0
    )) == -1)
        die("impossibile creare un socket");

    // creo la connessione
    if (connect(server_sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1)
        die("impossibile connettersi");

    // ricevo dal server il messaggio di benvenuto (o di non benuto se siamo in blacklist)
    int resplen;
    struct response resp;
    if ((resplen = get_response(server_sock, &resp, 1)) <= 0)
    {
        consolelog("impossibile ottenere messaggio di benvenuto\n");
        exit(EXIT_FAILURE);
    }

    if (resp.code != SRESP_OK)
    {
        consolelog("il server ha chiuso la connessione\n");
        close(server_sock);
        exit(EXIT_SUCCESS);
    }

    consolelog("digitare !help per vedere la lista dei comandi\n");

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
        int nmatches = regex_match("^ *!?([A-z_0-9]+) *(([-.A-z_0-9]+ *)*) *$", userinput, &matches);
        
        // se il comando non è nel formato corretto continuo
        if (nmatches == 0)
        {
            consolelog("il formato del comando non è corretto\n");
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
        if (strcmp(command, "help") == 0)
            ret = help(nargs, args);

        else if (strcmp(command, "signup") == 0)
            ret = signup(server_sock, nargs, args);
        
        else if (strcmp(command, "login") == 0)
            ret = login(server_sock, nargs, args);
        
        else if (strcmp(command, "invia_giocata") == 0)
            ret = invia_giocata(server_sock, nargs, args);
        
        else if (strcmp(command, "vedi_giocate") == 0)
            ret = vedi_giocate(server_sock, nargs, args);

        else if (strcmp(command, "vedi_estrazione") == 0)
            ret = vedi_estrazione(server_sock, nargs, args);
        
        else if (strcmp(command, "vedi_vincite") == 0)
            ret = vedi_vincite(server_sock);

        else if (strcmp(command, "esci") == 0)
            ret = esci(server_sock);
        
        // comando sconosciuto
        else
            consolelog("comando sconosciuto\n");

        // libero le risorse che non servono piu'
        regex_match_free(&matches, nmatches);
        free(command);
        int i;
        for (i = 0; i < nargs; i++)
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
    int i;
    for (i = 0; i < argc; i++)
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
    int nmatches = regex_match("^([0-9]+)([ \n\r\t]+(.*))?", resptxt, &matches);
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
int help(int argc, char *args[])
{
    if (argc != 0 && argc != 1)
    {
        printf("uso: !help [comando]\n");
        return 0;
    }

    // se non è specificato il comando mostro la lista di tutti i comandi
    if (argc == 0)
    {
        printf(
"\
\ncomandi disponibili:\n\
    !help [comando]\n\
    !signup <username> <password>\n\
    !login <username> <password>\n\
    !invia_giocata <schedina>\n\
    !vedi_giocate <tipo>\n\
    !vedi_estrazione <n> [ruota]\n\
    !vedi_vincite\n\
    !esci\n\
"
        );

        return 0;
    }

    // il comando è specificato
    if (strcmp(args[0], "help") == 0)
        printf("\n!help [comando]\nmostra le istruzioni di comando se specificato altrimenti la lista di tutti i comandi\n");
    
    else if (strcmp(args[0], "signup") == 0)
        printf("\n!signup <username> <password>\ncrea un utente con username e password specificati\n");
    
    else if (strcmp(args[0], "login") == 0)
        printf("\n!login <username> <password>\ntenta di autenticare l'utente specificato con la password fornita\n");
    
    else if (strcmp(args[0], "invia_giocata") == 0)
        printf("\n!invia_giocata -r <ruota1 ... ruotaN> -n <num1 ... numN> -i <importoEstratto importoAmbo ... importoCinquina>\ninvia una giocata al server\n");

    else if (strcmp(args[0], "vedi_giocate") == 0)
        printf("\n!vedi_giocate <tipo>\nmostra le giocate effettuate. (tipo = 0: attive; tipo = 1: non attive)\n");

    else if (strcmp(args[0], "vedi_estrazione") == 0)
        printf("\n!vedi_estrazione <n> [ruota]\nmostra le ultime n estrazioni della ruota fornita. se la ruota non è specificata mostra le estrazioni relative a tutte le ruote\n");

    else if (strcmp(args[0], "vedi_vincite") == 0)
        printf("\n!vedi_vincite\nmostra tutte le vincite\n");

    else if (strcmp(args[0], "esci") == 0)
        printf("\n!esci\ntermina la sessione e/o chiude la connessione\n");
    
    else
        printf("comando \"%s\" sconosciuto\n", args[0]);

    return 0;
}


int signup(int sockfd, int argc, char *args[])
{
    if (argc != 2)
    {
        consolelog("numero di parametri errato\n");
        return 0;
    }

    // invio il comando al server
    if (send_command(sockfd, "signup", argc, args) <= 0)
        return -1;

    struct response resp;
    while (1)
    {
        // ottengo la risposta dal server
        if (get_response(sockfd, &resp, 1) == -1)
            return -1;
        
        switch (resp.code)
        {
            // se l'username è già in uso
            case SRESP_RETRY:
            {
                // ne chiedo un altro all'utente
                consolelog("\tusername: ");
                char username[USERNAME_LEN + 1];
                fgets(username, USERNAME_LEN, stdin);
                
                // e lo invio al server
                send_msg(sockfd, username);
                continue;
            }
            
            // se la registrazione è andata a buon fine
            case SRESP_OK:
                return 0;
        
            // c'è stato un problema
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
        consolelog("numero di parametri errato\n");
        return 0;
    }

    // invio il comando
    if (send_command(sockfd, "login", argc, args) <= 0)
        return -1;
    
    // ottengo la risposta dal server
    struct response resp;
    if (get_response(sockfd, &resp, 0) <= 0)
        return -1;
    
    // se è andato tutto bene estraggo il session id
    if (resp.code == SRESP_OK && strlen(resp.info) == SESSIONID_LEN)
    {
        strcpy(sessionid, resp.info);
        consolelog("login effettuato con successo\n");
    }

    // se il server ha chiuso la connessione esco (inserimento in blacklist)
    else if (resp.code == SRESP_CLOSE)
    {
        consolelog("il server ha chiuso la connessione\n");
        return -1;
    }

    // c'è stato un problema
    else
    {
        consolelog("impossibile effettuare login\n");

        // mostro la risposta del server
        echo_response(&resp);
    }    

    return 0;
}

int invia_giocata(int sockfd, int argc, char *args[])
{
    // costrusco una schedina con i parametri passati dall'utente
    schedina_t schedina;
    memset(&schedina, 0, sizeof(schedina));

    // interi che indicano se le varie parti della schedina sono state specificate e in quale quantità
    int got_ruote = 0;
    int got_numeri = 0;
    int got_importi = 0;

    // automa a stati finiti
    enum {
        EXPECT_OPTION,
        EXPECT_RUOTA,
        EXPECT_NUMERO,
        EXPECT_IMPORTO,

        NUM_EXPECTED
    } state = EXPECT_OPTION;
    
    // server per mostrare più detagli riguardo all'uso scorretto del comando
    const char expected_str[NUM_EXPECTED][128] = {
        [EXPECT_OPTION] = "{-r, -n, -i}",
        [EXPECT_RUOTA] = "{tutte, bari, cagliari, firenze, genova, milano, napoli, palermo, roma, torino, venezia, nazionale}",
        [EXPECT_NUMERO] = "{tra 1 e 90}",
        [EXPECT_IMPORTO] = "{ 0.00, 0.05, 0.10, 0.20, 0.50, 1.00, 2.00, 3.00, 5.00, 10.00, 20.00, 50.00, 100.00, 200.00 }"
    };

    int i;
    char *arg = "";
    for (i = 0; i < argc; i++)
    {
        // argomento corrente
        arg = args[i];

        // se l'argomento corrente è un'opzione e non stiamo aspettando opzioni
        // imposto lo stato a EXPECT_OPTION e ripeto l'iterazione
        if (arg[0] == '-' && state != EXPECT_OPTION)
        {
            i--;
            state = EXPECT_OPTION;
            continue;
        }

        switch (state)
        {
            case EXPECT_OPTION:
                if (strcmp(arg, "-r") == 0)
                {
                    state = EXPECT_RUOTA;
                    continue;
                }
                else if (strcmp(arg, "-n") == 0)
                {
                    state = EXPECT_NUMERO;
                    continue;
                }
                else if (strcmp(arg, "-i") == 0)
                {
                    state = EXPECT_IMPORTO;
                    continue;
                }
                else
                    goto wrongparameter;
                break; // switch

            case EXPECT_RUOTA:
            {
                unsigned int mask;
                if (!ruota_valida(arg, &mask))
                    goto wrongparameter;

                // setto il bit corrispondente alla ruota specificata
                schedina.ruote_selezionate |= mask;

                got_ruote = 1;
                break; // switch
            }
            
            case EXPECT_NUMERO:
            {
                // controllo se i numeri sono già stati tutti inseriti
                if (got_numeri >= N_DA_GIOCARE)
                {
                    consolelog("numero di numeri da giocare superato");
                    return 0;
                }

                // controllo se l'argomento è un numero da giocare valido
                int n;
                if (!numero_da_giocare_valido(arg, &n))
                    goto wrongparameter;

                // controllo se il numero è già stato inserito nella schedina
                int j;
                for (j = 0; j < got_numeri; j++)
                {
                    if (schedina.numeri[j] == n)
                    {
                        consolelog("il numero %d è stato inserito più volte nella schedina\n", n);
                        return 0;
                    }
                }
                
                // il parametro è valido quindi lo inserisco nella schedina
                schedina.numeri[got_numeri] = n;
                got_numeri++;
                break; // switch
            }

            case EXPECT_IMPORTO:
            {
                // controllo se gli importi sono già stati specificati per tutte le scommesse
                if (got_importi >= N_DA_GIOCARE)
                {
                    consolelog("il numero di numeri supera il numero di tipi di scomesse (estratto, ambo, ..., cinquina)");
                    return 0;
                }

                // controllo se l'argomento è un importo valido
                int n;
                if (!importo_valido(arg, &n))
                    goto wrongparameter;
                
                // il parametro è valido quindi lo inserisco nella schedina
                schedina.importi_scommesse[got_importi] = n;
                got_importi++;
                break; // switch
            }
            
            case NUM_EXPECTED: // specifico per non produrre warnings durante la compilazione
                break; // switch
        }
    }

    // se ci sono delle sezioni della schedina che risultano non compilate
    if (got_ruote == 0 || got_numeri == 0 || got_importi == 0)
    {
        consolelog("parametri mancanti\n");
        return 0;
    }

    // invio il comando al server
    char *dummy_args = ""; // il comando lato server non ha argomenti (le schedine vengo scambiati successivamente)
    if (send_command(sockfd, "invia_giocata", 0, (char **) dummy_args) <= 0)
        return -1;
    
    // ricevo la risposta
    struct response resp;
    if (get_response(sockfd, &resp, 1) <= 0)
        return -1;
    
    // se il server dice di continuare procedo alla serializzazione su sucket
    // della schedina
    if (resp.code == SRESP_CONTINUE)
    {
        // creo un mem_stream per il messaggio
        char *msg;
        size_t msglen;
        FILE *stream = open_memstream(&msg, &msglen);
        if (!stream)
        {
            consolelog("impossibile creare stream per la serializzazione della schedina\n");
            return -1;
        }

        // effettuo la serializzazione della schedina
        serializza_schedina(stream, &schedina);

        // chiudo lo stream
        fclose(stream);

        // invio il messaggio
        if (send_msg(sockfd, msg) <= 0)
        {
            consolelog("impossibile inviare schedina\n");
            free(msg);
            return 0;
        }

        free(msg);
    } 
    else
    {
        // se la risposta non è SRESP_OK
        return 0;
    }

    consolelog("schedina inviata\n");

    // ricevo l'esito dell'operazione dal server    
    last_msg_operation = MSGOP_SEND;
    if (get_response(sockfd, &resp, 1) <= 0)
        return -1;
    
    if (resp.code != SRESP_OK)
       consolelog("giocata non effettuata");

    return 0;

wrongparameter:
    consolelog("errore argomento %d: previsto \"%s\", trovato \"%s\"\n", i + 1, expected_str[state], arg);
    return 0;
}

int vedi_giocate(int sockfd, int argc, char *args[])
{
    if (argc != 1)
    {
        consolelog("numero di parametri errato\n");
        return 0;
    }

    if (strcmp(args[0], "0") != 0 && strcmp(args[0], "1") != 0)
    {
        consolelog("tipo errato (0 = non attive, 1 = attive)\n");
        return 0;
    }

    // invio il comando al server
    if (send_command(sockfd, "vedi_giocate", argc, args) == -1)
        return -1;
    
    // ricevo la risposta
    struct response resp;
    if (get_response(sockfd, &resp, 0) == -1)
        return -1;
    
    // se il server non da l'ok esco
    if (resp.code != SRESP_OK)
    {
        echo_response(&resp);
        return 0;
    }
    
    // ricevo il messaggio contenente tutte le schedine richieste
    char *msg;
    int msglen;
    last_msg_operation = MSGOP_NONE;
    if ((msglen = recv_msg(sockfd, &msg)) <= 0)
    {
        consolelog("impossibile ricevere le giocate\n");
        return 0;
    }

    // apro il messaggio come uno stream
    FILE *stream = fmemopen(msg, msglen, "r");

    // leggo il numero di schedine da leggere dallo stream
    long long int ndaleggere;
    deserializza_int(stream, &ndaleggere);

    // per ogni schedina ricevuta stampo le informazioni
    int i;
    for (i = 0; i < ndaleggere; i++)
    {
        schedina_t schedina;
        deserializza_schedina(stream, &schedina);

        printf("%d) ", i + 1);

        // stampo le ruote selezionate
        int j;
        for (j = 0; j < N_RUOTE; j++)
        {
            unsigned int mask = 1u << j;
            if (schedina.ruote_selezionate & mask)
                printf("%s ", ruote_str[j]);
        }

        // stampo i numeri giocati
        for (j = 0; j < N_DA_GIOCARE; j++)
            if (schedina.numeri[j] != 0)
                printf("%d ", schedina.numeri[j]);
        
        // stampo gli importi per ogni tipo di scommessa
        for (j = 0; j < N_TIPI_SCOMMESSE; j++)
        {
            if (schedina.importi_scommesse[j] != 0)
            {
                // conversione da centesimi a euro
                float importo = ((float) schedina.importi_scommesse[j]) / 100.0f;
                printf("* %s %.2f ", tipi_scommesse_str[j], importo);
            }
        }

        printf("\n");
    }

    // chiudo sream e libero msg
    fclose(stream);
    free(msg);

    return 0;
}

int vedi_estrazione(int sockfd, int argc, char *args[])
{
    if (argc != 1 && argc != 2)
    {
        consolelog("numero di parametri errato\n");
        return 0;
    }

    // controllo n
    int n;
    if (regex_match("^[0-9]+$", args[0], NULL) == 0)
    {
    nonvalidn:
        consolelog("n deve essere un numero positivo tra 1 e 20\n");
        return 0;
    }

    // estraggo n
    sscanf(args[0], "%d", &n);
    if (n < 1 || n > 20)
        goto nonvalidn;

    // se è specificata ricavo l' indice corrispondente alla ruota
    int indiceruota;
    if (argc == 2)
    {
        // controllo se la ruota specificata è valida
        int trovata = 0;
        int i;
        for (i = 0; i < N_RUOTE; i++)
        {
            if (strcmp(args[1], ruote_str[i]) == 0)
            {
                trovata = 1;
                indiceruota = i;
                break;
            }
        }

        if (!trovata)
        {
            consolelog("la ruota specificata non è valida\n");
            return 0;
        }
    }

    // invio il comando al server
    if (send_command(sockfd, "vedi_estrazione", argc, args) == -1)
        return -1;
    
    // ricevo la risposta
    struct response resp;
    if (get_response(sockfd, &resp, 0) == -1)
        return -1;
    
    // se il server non da l'ok esco
    if (resp.code != SRESP_OK)
    {
        echo_response(&resp);
        return 0;
    }
    
    // ricevo il messaggio contenente tutte le estrazioni richieste
    char *msg;
    int msglen;
    last_msg_operation = MSGOP_NONE;
    if ((msglen = recv_msg(sockfd, &msg)) <= 0)
    {
        consolelog("impossibile ricevere le estrazioni\n");
        return 0;
    }

    // apro il messaggio come uno stream
    FILE *stream = fmemopen(msg, msglen, "r");

    // leggo il numero di estrazioni da leggere dallo stream e alloco un'array di schedine
    long long int ndaleggere;
    deserializza_int(stream, &ndaleggere);

    // per ogni estrazione ricevuta stampo le informazioni
    int i;
    for (i = 0; i < ndaleggere; i++)
    {
        // se la ruota è specificata
        if (argc == 2)
        {
            // leggo il timestamp
            long long int bigint;
            deserializza_int(stream, &bigint);

            // converto il timestamp in stringa
            char datebuff[64] = "";
            strftime(datebuff, 64, "%d/%m/%Y - %H:%M:%S", localtime((time_t *) &bigint));

            printf("\n***************************************\n-------- %s --------\n***************************************\n%12s\t", datebuff, ruote_str[indiceruota]);

            int j;
            for (j = 0; j < N_DA_ESTRARRE; j++)
            {
                deserializza_int(stream, &bigint);
                printf("  %2d", (int) bigint);
            }
        }

        // se la ruota non è specificata stampo tutte le ruote
        else
        {
            estrazione_t estrazione;
            deserializza_estrazione(stream, &estrazione);

            // converto il timestamp in stringa
            char datebuff[64] = "";
            strftime(datebuff, 64, "%d/%m/%Y - %H:%M:%S", localtime(&estrazione.timestamp));

            printf("\n***************************************\n-------- %s --------\n***************************************\n", datebuff);

            int j;
            for (j = 0; j < N_RUOTE; j++)
            {
                // stampo il nome della ruota
                printf("%12s\t", ruote_str[j]);

                int k;
                for (k = 0; k < N_DA_ESTRARRE; k++)
                {
                    printf("%2d  ", estrazione.ruote[j][k]);
                }

                printf("\n");
            }
        }

        printf("\n");
    }

    // chiudo stream e libero msg
    fclose(stream);
    free(msg);

    return 0;
}

int vedi_vincite(int sockfd)
{
    // invio il comando al server
    char *dummy_args = "";
    if (send_command(sockfd, "vedi_vincite", 0, &dummy_args) == -1)
        return 0;

    // ricevo la risposta
    struct response resp;
    if (get_response(sockfd, &resp, 0) == -1)
        return 0;
    
    // se il server non da l'ok esco
    if (resp.code != SRESP_OK)
    {
        echo_response(&resp);
        return 0;
    }

    // ricevo il messaggio contente le giocate
    char *msg;
    int msglen;
    last_msg_operation = MSGOP_NONE;
    if ((msglen = recv_msg(sockfd, &msg)) <= 0)
        return 0;
    
    // apro msg come se fosse uno stream
    FILE *stream = fmemopen(msg, sizeof(char) * msglen, "r");

    // leggo il numero di giocate vincenti
    long long int bigint;
    deserializza_int(stream, &bigint);
    int nvittorie = bigint;

    // array contenente le vincite totali per categoria di scommessa
    int vincite_totali[N_TIPI_SCOMMESSE];
    memset(vincite_totali, 0, sizeof(vincite_totali));        

    // stampo tutte le giocate vincenti
    int i;
    for (i = 0; i < nvittorie; i++)
    {
        giocata_t giocata;
        deserializza_giocata(stream, &giocata);

        // aggiungo le vincite di questa giocata al totale delle vincite
        int j;
        for (j = 0; j < N_TIPI_SCOMMESSE; j++)
        {
            vincite_totali[j] += giocata.vincita[j];
        }

        // converto il timestamp in stringa
        char datebuff[64] = "";
        strftime(datebuff, 64, "%d/%m/%Y - %H:%M:%S", localtime(&giocata.estrazione.timestamp));

        // stampo la data dell'estrazione
        printf("\nestrazione del %s\n", datebuff);

        // per ogni ruota selezionata
        for (j = 0; j < N_RUOTE; j++)
        {
            // se la ruota corrente non è selezionata passo oltre
            unsigned int mask = 1u << j;
            if ((giocata.schedina.ruote_selezionate & mask) == 0)
                continue;
            
            // stampo il nome della ruota
            printf("%12s: ", ruote_str[j]);

            // stampo i numeri indovinati su questa ruota
            int k;
            for (k = 0; k < N_DA_GIOCARE; k++)
            {
                int l;
                for (l = 0; l < N_DA_ESTRARRE; l++)
                {
                    if (giocata.schedina.numeri[k] == giocata.estrazione.ruote[j][l])
                        printf(" %2d", giocata.schedina.numeri[k]);
                }
            }

            printf("\n");
        }

        // stampo un riepilogo delle vincite di questa giocata
        int k;
        for (k = 0; k < N_TIPI_SCOMMESSE; k++)
        {
            // conversione centesimi ad euro
            float v = ((float) giocata.vincita[k]) / 100.0f;

            printf("%c: %.2f  ", tipi_scommesse_str[k][0] - 32, v);
        }

        printf("\n**************************************\n");
    }

    // chiudo lo stream
    fclose(stream);

    // libero msg
    free(msg);

    // stampo il totale delle vincite per tipo di scommeessa
    printf("\nRIEPILOGO VINCITE:\n");
    for (i = 0; i < N_TIPI_SCOMMESSE; i++)
    {
        // conversione centesimi ad euro
        float v = ((float) vincite_totali[i]) / 100.0f;

        printf("%12s: %.2f\n", tipi_scommesse_str[i], v);
    }

    printf("\n");



    return 0;
}

int esci(int sockfd)
{
    char *dummy_args = "";
    send_command(sockfd, "esci", 0, (char **) dummy_args);

    struct response resp;
    get_response(sockfd, &resp, 1);
    
    return -1;
}