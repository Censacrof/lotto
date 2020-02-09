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
    int msglen;
    char *msg;
    if ((msglen = recv_msg(client_sock, &msg)) <= 0)
    {
        consolelog("impossibile ricevere la schedina\n");
        return 0;
    }

    // deserializzo la schedina
    schedina_t schedina;

    // associo uno stream al alla stringa msg
    FILE *stream = fmemopen(msg, msglen, "r");
    int ret = 0;
    if (!stream)
    {
        consolelog("impossibile creare stream per la deserializzazione della schedina\n");
        send_response(client_sock, SRESP_ERR, "si e' verificato un errore");
        ret = -1;
        goto end;
    }

    // effettuo la deserializzazione della schedina
    deserializza_schedina(stream, &schedina);

    // salvo la giocata
    if (salva_giocata(session.username, &schedina) == -1)
    {
        send_response(client_sock, SRESP_ERR, "impossibile effettuare giocata");
        ret = 0;
        goto end;
    }

    // comunico al client che la giocata e' stata registrata
    send_response(client_sock, SRESP_OK, "giocata effettuata");

end:
    // chiudo lo stream
    fclose(stream);
    free(msg);

    return ret;
}