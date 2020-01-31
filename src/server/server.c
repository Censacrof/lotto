#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>

#include "../common.h"
#include "commands.h"

void handle_connection(int, const char *client_addr_str, const int port);

int main(int argc, char *argv[])
{
    sprintf(whoiam, "%d::", getpid());
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
    
    int port;
    sscanf(DEFAULT_SERVER_PORT, "%d", &port);

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
            send_response(client_sock, SRESP_ERR, "questo indirizzo risulta bannato a causa dei numerosi tentativi di login falliti. riprovare più tardi");

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