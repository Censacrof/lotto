#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "../src/common.h"

void client();
void server();

sem_t *synchro;
int main(int argc, char *argv[])
{
    char sem_name[] = "sync";
    synchro = sem_open(sem_name, O_CREAT | O_EXCL, 0, 0);
    sem_unlink(sem_name);

    pid_t pid = fork();
    if (pid == -1)
        die("errore durante fork");

    if (!pid)
        client();
    else
        server();
}

void server()
{
    printf("SERVER STARTED\n");
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
    {
        sem_post(synchro);
        die("server: errore durante getaddrinfo");
    }

    // creo il socket
    int server_sock;
    if ((server_sock = socket(
        AF_INET,                        // IPv4
        SOCK_STREAM,                    // TCP
        0
    )) == -1)
    {
        sem_post(synchro);
        die("server: impossibile creare un socket");
    }

    // faccio il bind del socket sulla porta specificata in server_info
    if (bind(server_sock, server_info->ai_addr, server_info->ai_addrlen) == -1)
    {
        sem_post(synchro);
        die("server: impossibile fare il binding di un socket");
    }
        
    
    // metto il socket in ascolto
    if (listen(server_sock, 10) == -1) 
    {
        sem_post(synchro);
        die("server: impossibile mettere il socket in listening");
    }
        
    
    // segnalo al client che può stabilire una connessione
    sem_post(synchro);

    // ciclo di accettazione
    int client_sock;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_size = sizeof(client_addr);
    if ((client_sock = accept(server_sock, (struct sockaddr *) &client_addr, &client_addr_size)) == -1)
        die("server: impossibile accettare una connessione");

    char *msg;
    int msg_len = recv_msg(client_sock, &msg);
    if (msg_len == -1)
        die("server: impossibile ricevere un messaggio");
    else if (msg_len == 0)
        printf("server: il client si è disconnesso\n");
    else
    {
        printf("server: ricevuto -> %s (len = %ld)\n", msg, strlen(msg));
        free(msg);
    }

    close(client_sock);

    freeaddrinfo(server_info);
    exit(EXIT_SUCCESS);
}

void client()
{
    printf("CLIENT STARTED\n");

    struct addrinfo hint, *server_addr_info;
    memset((void *) &hint, 0, sizeof(hint));
    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;

    if (getaddrinfo("127.0.0.1", DEFAULT_SERVER_PORT, &hint, &server_addr_info) == -1)
        die("client: errore durante getaddrinfo");

    // creo il socket
    int sockfd;
    if ((sockfd = socket(
        server_addr_info->ai_family,
        server_addr_info->ai_socktype,
        server_addr_info->ai_protocol
    )) == -1)
        die("client: impossibile creare un socket");

    // aspetto che il server sia pronto a ricever una connesione
    sem_wait(synchro);

    if (connect(sockfd, server_addr_info->ai_addr, server_addr_info->ai_addrlen) == -1)
        die("client: impossibile connettere il socket");

    char msg[] = "Hello world!";
    int sent = send_msg(sockfd, msg);
    if (sent == -1)
        die("client: impossibile inviare un messaggio");
    else if (sent == 0)
        printf("client: il server si è disconnesso\n");
    else
        printf("client: inviato -> %s (len = %d)\n", msg, sent);

    close(sockfd);
    freeaddrinfo(server_addr_info);
    exit(EXIT_SUCCESS);
}