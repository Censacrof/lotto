#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <dirent.h>

#include "../common.h"
#include "commands.h"

void handle_connection(int, const char *client_addr_str, const int port);
void estrattore(time_t period);

int main(int argc, char *argv[])
{
    // parametri
    if (argc < 2 || argc > 3)
    {
    usage:
        printf("uso: %s <porta> <periodo in secondi>\n", argv[0]);
        exit(EXIT_SUCCESS);
    }        
    
    // estraggo la porta
    int port;
    if (regex_match("^[0-9]+$", argv[1], NULL) == 0)
    {
        printf("formato porta non valido\n");
        goto usage;
    }
    else
        sscanf(argv[1], "%d", &port);

    // estraggo il periodo
    time_t period = 60 * 5; // 5 minuti di default
    if (argc == 3)
    {
        if (regex_match("^[0-9]+$", argv[2], NULL) == 0)
        {
            printf("formato periodo non valido\n");
            goto usage;
        }
        else
            sscanf(argv[2], "%ld", &period);
    }

    // creo il processo che gestisce le estrazioni
    pid_t child_pid;
    if ((child_pid = fork()) == -1)
    {
        perror("impossibile creare processo estrattore");
        exit(EXIT_FAILURE);
    }
    else if (child_pid == 0)
        estrattore(period);


    sprintf(whoiam, "%d:LISTENER", getpid());
    consolelog("server avviato\n");

    // intializzo il seed di random
    srand(time(NULL));

    // creo il socket
    int server_sock;
    if ((server_sock = socket(
        AF_INET,                        // IPv4
        SOCK_STREAM,                    // TCP
        0
    )) == -1)
        die("impossibile creare un socket");

    // creo l'indirizzo del server
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // faccio il bind del socket su porta e indirizzo specificati in server_addr
    if (bind(server_sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1)
        die("impossibile fare il binding di un socket");
    
    // metto il socket in ascolto
    if (listen(server_sock, 10) == -1)
        die("impossibile mettere il socket in listening");
    
    consolelog("server in ascolto sulla porta %d\n", port);

    // ciclo di accettazione
    while (1)
    {
        // resetto lo stato delle funzioni send_msg e recv_msg
        last_msg_operation = MSGOP_NONE;

        // accetto una connessione
        int client_sock;
        struct sockaddr_in client_addr;
        socklen_t client_addr_size = sizeof(client_addr);
        if ((client_sock = accept(server_sock, (struct sockaddr *) &client_addr, &client_addr_size)) == -1) 
        {
            perror("impossibile accettare una connessione"); 
            continue;
        }

        // converto l'indirizzo del client in stringa
        char client_addr_str[INET_ADDRSTRLEN + 1];
        client_addr_str[INET_ADDRSTRLEN] = '\0';
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_addr_str, INET_ADDRSTRLEN);

        // controllo se l'indirizzo del client è nella blacklist
        time_t timeleft;
        if (is_blacklisted(client_addr_str, &timeleft))
        {
            // segnalo al client che il suo indirizzo risulta in blacklist
            char buff_resp[256];
            char buff_timeleft[16];
            strftime(buff_timeleft, 16, "%H:%M:%S", gmtime(&timeleft));

            sprintf(buff_resp, "questo indirizzo (%s) risulta bannato a causa dei numerosi tentativi di login falliti. riprovare tra %s", client_addr_str, buff_timeleft);
            send_response(client_sock, SRESP_CLOSE, buff_resp);

            // chiudo la connessione e ricomincio il ciclo di accettazione
            close(client_sock);
            continue;
        }

        // creo un processo figlio per gestire la connessione
        pid_t child_pid;
        if ((child_pid = fork()) == -1)
        {
            perror("impossibile creare processo figlio");
            close(client_sock);
        }
        else if (child_pid == 0)
        {
            // PROCESSO FIGLIO
            // libero le risorse non utilizzate dal processo figlio
            close(server_sock);

            // gestisco la connessione con il client
            handle_connection(client_sock, client_addr_str, ntohs(client_addr.sin_port));

            // una volta gestita la connessione termino il processo
            exit(EXIT_SUCCESS);
        }

        // PROCESSO PADRE
        // libero le risorse non utilizzate dal processo padre
        close(client_sock);
    }

    exit(EXIT_SUCCESS);
}

void handle_connection(int client_sock, const char *client_addr_str, const int port)
{
    // randomizzo
    srand(time(NULL));

    pid_t pid = getpid();

    // inizializzo la variabile globale whoami
    sprintf(whoiam, "%d:%s:%d", pid, client_addr_str, port);

    consolelog("connessione stabilita\n");

    // segnalo al client che il server è pronto a ricevere comandi
    send_response(client_sock, SRESP_OK, "benvenuto. pronto a ricevere comandi");

    // main loop
    while (1)
    {
        // resetto lo stato delle funzioni send_msg e recv_msg
        last_msg_operation = MSGOP_NONE;

        // ricevo un messaggio
        char *msg; 
        int len;
        if ((len = recv_msg(client_sock, &msg)) == -1)
            die("errore durante recv_msg");

        // controllo se il client ha chiuso la connessione
        if (len == 0)
            break;
        
        // eseguo il comando inviato dal client
        int reti = execute_command(client_sock, msg, client_addr_str);
        free(msg);

        // se c'è bisogno chiudo la connessione ed esco dal ciclo
        if (reti == -1)
            break;
    }

    consolelog("connessione chiusa\n");
    close(client_sock);
    exit(0);
}

// calcolo il coefficiente binomiale come i coefficienti del triangolo di pascal
// (programmazione dinamica)
#define BINOMIALCACHE_SIZE 20
int binomialcache_initialized = 0;
unsigned int binomialcache[BINOMIALCACHE_SIZE][BINOMIALCACHE_SIZE];
unsigned int binomialcoeff(unsigned int n, unsigned int k)
{
    // casi base
    if (n == k || k == 0)
        return 1;
    if (n < 0 || k < 0 || k > n) 
        return 0;
    
    // se la cache non è inizializzata la inizializzo
    if (!binomialcache_initialized)
    {
        for (int i = 1; i < BINOMIALCACHE_SIZE; i++)
            for (int j = 1; j < i; j++)
                binomialcache[i][j] = 0;

        binomialcache_initialized = 1;
    }

    // se l'elemento è presente in cache lo restituisco
    if (n < BINOMIALCACHE_SIZE && k < BINOMIALCACHE_SIZE)
        if (binomialcache[n][k] != 0)
            return binomialcache[n][k];
    
    // altrimenti lo calcolo, lo inserisco in cache e lo restituisco
    unsigned int res = binomialcoeff(n - 1, k - 1) + binomialcoeff(n - 1, k);

    if (n < BINOMIALCACHE_SIZE && k < BINOMIALCACHE_SIZE)
        binomialcache[n][k] = res;

    return res;
}

void estrattore(const time_t period)
{
    // randomizzo
    srand(time(NULL));

    sprintf(whoiam, "%d:ESTRATTORE", getpid());
    
    while (1)
    {
        // sospendo il processo
        sleep(period);

        // creo un estrazione
        estrazione_t estrazione;
        memset(&estrazione, 0, sizeof(estrazione));

        estrazione.timestamp = time(NULL);

        // srand(0); // DEBUG
        for (int i = 0; i < N_RUOTE; i++)
        {
            // creo un alias per la ruota corrente
            int *estratti = estrazione.ruote[i];

            for (int j = 0; j < N_DA_ESTRARRE; j++)
            {
                // estraggo un numero tra 1 e 90 che non sia già stato estratto
                int n;
                while(1)
                {
                    n = 1 + (rand() % 90);

                    // controllo se il numero è già stato estratto su questa ruota
                    for (int k = 0; k < j; k++)
                        if (n == estratti[k])
                            continue;
                    break;
                }

                // aggiungo il numero all'estrazione
                estratti[j] = n;
            }
        }

        // salvo l'estrazione
        salva_estrazione(&estrazione);

        // apro la cartella contenente i file registro di tutti gli utenti
        inizializza_directories();
        DIR *dir = opendir(PATH_UTENTI);
        if (!dir)
        {
            consolelog("impossibile aprire la cartella \"%s\". l'estrazione è stata annullata\n");
            continue;
        }

        // per ogni utente
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            // se l'entry corrente non è un file regolare (ma è una directory, un link, un file speciale, ...)
            // passo oltre
            if (entry->d_type != DT_REG)
                continue;

            // carico l'utente
            utente_t utente;
            if (carica_utente(entry->d_name, &utente) == -1)
            {
                consolelog("impossibile caricare l'utente \"%s\"\n", entry->d_name);
                continue;
            }

            // scorro tutte le giocate dell'utente
            for (int i = 0; i < utente.n_giocate; i++)
            {
                // se la giocata non è attiva passo oltre
                if (utente.giocate[i].attiva == 0)
                    continue;
                
                // imposto la giocata come non attiva
                utente.giocate[i].attiva = 0;

                // copio l'estrazione corrente nella giocata dell'utente
                memcpy(&utente.giocate[i].estrazione, &estrazione, sizeof(estrazione_t));

                // ricavo quanti numeri sono stati giocati nella schedina
                int n_numeri_giocati = 0;
                for (int j = 0; j < N_DA_GIOCARE; j++)
                    if (utente.giocate[i].schedina.numeri[j] != 0)
                        n_numeri_giocati++;

                // ricavo quante sono le ruote della giocata e per ognuna di queste ruote quanti numeri sono stati indovinati
                int n_numeri_indovinati[N_RUOTE];
                int n_ruote_selezionate = 0;
                for (int j = 0; j < N_RUOTE; j++)
                {
                    // se la ruota j non è stata selezionata passo oltre
                    int mask = 1u << j;
                    if (!(utente.giocate[i].schedina.ruote_selezionate & mask))
                        continue;

                    // conto i numeri indovinati sulla ruota j
                    n_numeri_indovinati[n_ruote_selezionate] = 0;            
                    for (int k = 0; k < N_DA_GIOCARE; k++)
                    {
                        if (utente.giocate[i].schedina.numeri[k] == 0)
                            continue;                        

                        for (int l = 0; l < N_DA_ESTRARRE; l++)
                            if (utente.giocate[i].schedina.numeri[k] == estrazione.ruote[j][l])
                                n_numeri_indovinati[n_ruote_selezionate]++;
                    }

                    // incremento il numero di ruote selezionate
                    n_ruote_selezionate++;
                }

                // per ogni tipo di scommessa calcolo la vincita
                for (int j = 0; j < N_TIPI_SCOMMESSE; j++)
                {
                    // alias
                    int *vincita = utente.giocate[i].vincita;
                    int *importi_scommesse = utente.giocate[i].schedina.importi_scommesse;

                    // inizializzo la vincita corrispondente a questo tipo di scommessa
                    vincita[j] = 0;

                    // se nella schedina non è presente questo tipo di scommessa passo oltre
                    if (importi_scommesse[j] == 0)
                        continue;
                    
                    const double vincite_lorde[N_TIPI_SCOMMESSE] = { 11.23, 250.00, 4500.00, 120000.00, 6000000.00 };

                    // sommo la vincita relativa ad ogni ruota selezionata k per questo tipo di scommessa j
                    for (int k = 0; k < n_ruote_selezionate; k++)
                    {
                        // se il numero di numeri indovinati su questa ruota è minore dei numeri richiesti da indovinare per questo tipo di scommessa
                        if (n_numeri_indovinati[k] <= j)
                            continue;

                        vincita[j] += importi_scommesse[j] * (vincite_lorde[j] / binomialcoeff(n_numeri_giocati, n_numeri_indovinati[k])) / n_ruote_selezionate;
                    }
                }
            }

            if (salva_utente(&utente) == -1)
            {
                consolelog("impossibile salvare utente, esito giocate attive non registrato\n");
                continue;
            }
        }

        // chiudo la cartella
        closedir(dir);

        consolelog("estrazione effettuata\n");
    }

    exit(EXIT_SUCCESS);
}