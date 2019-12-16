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
    struct addrinfo hint, *server_addr_info;
    memset((void *) &hint, 0, sizeof(hint));
    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;

    if (getaddrinfo("127.0.0.1", DEFAULT_SERVER_PORT, &hint, &server_addr_info) == -1)
        die("errore durante getaddrinfo");

    // creo il socket
    int sockfd;
    if ((sockfd = socket(
        server_addr_info->ai_family,
        server_addr_info->ai_socktype,
        server_addr_info->ai_protocol
    )) == -1)
        die("impossibile creare un socket");

    if (connect(sockfd, server_addr_info->ai_addr, server_addr_info->ai_addrlen) == -1)
        die("impossibile connettere il socket");
    
    printf("socket connesso\n");

    char msg[] = "Hello world!";
    if (send_msg(sockfd, msg) == -1)
        die("impossibile inviare un messaggio");

    close(sockfd);
    freeaddrinfo(server_addr_info);
    exit(EXIT_SUCCESS);
}