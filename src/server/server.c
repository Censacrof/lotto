#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>

#include "../common.h"
#include "commands.h"

void handle_connection(int, char *);


int main(int argc, char *argv[])
{
    sprintf(whoiam, "%d::", getpid());
    consolelog("server avviato\n");

    int status;

    // creo struttura "hint" da usare con getaddrinfo
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));   // azzero la struttura
    hints.ai_family     = AF_INET;      // IPv4
    hints.ai_socktype   = SOCK_STREAM;  // TCP
    hints.ai_flags      = AI_PASSIVE;   // lascio che sia getaddrinfo inserire le informazioni

    struct addrinfo *server_info = NULL;
    if ((status = getaddrinfo(
        NULL,                           // lascio che sia getaddrinfo inserire le informazioni
        DEFAULT_SERVER_PORT,
        &hints,        
        &server_info
    )) != 0)
        die("errore durante getaddrinfo");

    // creo il socket
    int server_sock;
    if ((server_sock = socket(
        AF_INET,                        // IPv4
        SOCK_STREAM,                    // TCP
        0
    )) == -1)
        die("impossibile creare un socket");

    // faccio il bind del socket sulla porta specificata in server_info
    if (bind(server_sock, server_info->ai_addr, server_info->ai_addrlen) == -1)
        die("impossibile fare il binding di un socket");
    
    // metto il socket in ascolto
    if (listen(server_sock, 10) == -1)
        die("impossibile mettere il socket in listening");
    
    char *listening_on = sockaddr_to_string((struct sockaddr *) server_info->ai_addr);
    if (!listening_on)
        die("errore durante sockaddr_to_string");

    consolelog("server in ascolto su %s\n", listening_on);
    free(listening_on);

    // ciclo di accettazione
    while (1)
    {
        int client_sock;
        struct sockaddr_storage client_addr;
        socklen_t client_addr_size = sizeof(client_addr);
        if ((client_sock = accept(server_sock, (struct sockaddr *) &client_addr, &client_addr_size)) == -1) 
        {
            perror("impossibile accettare una connessione"); 
            continue;
        }

        char *client_name = sockaddr_to_string((struct sockaddr *) &client_addr);

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
            freeaddrinfo(server_info);

            // gestisco la connessione con il client
            handle_connection(client_sock, client_name);

            // una volta gestita la connessione termino il processo
            exit(EXIT_SUCCESS);
        }

        // PROCESSO PADRE
        // libero le risorse non utilizzate dal processo padre
        close(client_sock);
        free(client_name);
    }    

    freeaddrinfo(server_info);
    exit(EXIT_SUCCESS);
}

void handle_connection(int client_sock, char *client_name)
{
    pid_t pid = getpid();

    // inizializzo la variabile globale whoami
    sprintf(whoiam, "%d:%s", pid, client_name);

    consolelog("connessione stabilita\n");

    while (1)
    {
        last_msg_operation = MSGOP_NONE;

        char *msg; 
        int len;
        if ((len = recv_msg(client_sock, &msg)) == -1)
            die("errore durante recv_msg");
        if (len == 0)
        {
            consolelog("connessione chiusa\n");
            exit(0);
        }
        
        execute_command(client_sock, msg);
        free(msg);
    }
}