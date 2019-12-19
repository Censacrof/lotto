#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>

#include "../src/common.h"

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("uso: %s <ip-address> <porta>\n", argv[0]);
        exit(EXIT_SUCCESS);
    }

    struct addrinfo hint, *server_addr_info;
    memset((void *) &hint, 0, sizeof(hint));
    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(argv[1], argv[2], &hint, &server_addr_info) == -1)
        die("errore durante getaddrinfo");

    // creo il socket
    int sockfd;
    if ((sockfd = socket(
        server_addr_info->ai_family,
        server_addr_info->ai_socktype,
        server_addr_info->ai_protocol
    )) == -1)
        die("client: impossibile creare un socket");

    if (connect(sockfd, server_addr_info->ai_addr, server_addr_info->ai_addrlen) == -1)
        die("client: impossibile connettere il socket");

    while (1)
    {
        printf("> ");
        
        char *req = NULL;
        size_t req_len = 0;
        getline(&req, &req_len, stdin);

        if (req[req_len - 1] == '\n' || req[req_len - 1] == '\r')
            req[req_len - 1] = '\0';

        if (req[req_len - 2] == '\n' || req[req_len - 2] == '\r')
            req[req_len - 2] = '\0';
        

        int sent = send_msg(sockfd, req);
        free(req);

        if (sent == -1) 
        {
            perror("impossibile inviare un messaggio");
            continue;
        }            
        else if (sent == 0)
        {
            printf("\nil server ha chiuso la connessione\n");
            break;
        }            
        else
        {
            char *res;
            int received = recv_msg(sockfd, &res);
            if (received == -1)
                die("impossibile ricevere un messaggio");
            else if (received == 0)
            {
                printf("\nil server ha chiuso la connessione\n");
                break; 
            }
            
            printf("%s\n", res);
            free(res);
        }
    }
    

    close(sockfd);
    freeaddrinfo(server_addr_info);
    exit(EXIT_SUCCESS);
}