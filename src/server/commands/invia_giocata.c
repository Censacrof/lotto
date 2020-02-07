#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../commands.h"
#include "../../data.h"

// !invia_giocata
int invia_giocata(int client_sock)
{
    // segnalo al client che puo' inviare la schedina
    send_response(client_sock, SRESP_CONTINUE, "pronto a ricevere la schedina");

    // ricevo la schedina
    schedina_t schedina;

    // duplico il file descriptor del socket (necessario per poter chiamare
    // fclose in seguito senza chiudere la connessione con il server)
    int fd = dup(client_sock);

    // associo uno stream al file descriptor (duplicato)
    FILE *sockstream = fdopen(fd, "r");

    last_msg_operation = MSGOP_RECV;
    int ret = 0;
    if (!sockstream)
    {
        consolelog("impossibile creare stream per la serializzazione della schedina\n");
        send_response(client_sock, SRESP_ERR, "si e' verificato un errore");
        ret = -1;
        goto end;
    }

    // effettuo la deserializzazione della schedina
    deserializza_schedina(sockstream, &schedina);

    // comunico al client che la giocata e' stata registrata
    send_response(client_sock, SRESP_OK, "giocata effettuata");

end:
    // chiudo lo stream
    fclose(sockstream);

    return ret;
}