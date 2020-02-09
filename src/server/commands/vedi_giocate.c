#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "../commands.h"
#include "../../data.h"

// !vedi_giocate tipo
int vedi_giocate(int client_sock, int argc, char *args[])
{
    if (argc != 1)
    {
        send_response(client_sock, SRESP_BADREQ, "numero di parametri errato");
        return 0;
    }

    // controllo il tipo di giocate richieste
    int attive;
    if (strcmp(args[0], "0") == 0)
        attive = 0;
    else if (strcmp(args[0], "1") == 0)
        attive = 1;
    else
    {
        send_response(client_sock, SRESP_ERR, "argomento non valido");
        return 0;
    }
    
    // carico l'utente
    utente_t utente;
    if (carica_utente(session.username, &utente) == -1)
    {
        send_response(client_sock, SRESP_ERR, "si è verificato un errore");
        return 0;
    }

    // segnalo al client che sto per inviare le schedine
    send_response(client_sock, SRESP_OK, "prepararsi a ricevere le schedine");

    // preparo il messaggio contenente le schedine da inviare
    char *msg;
    size_t msglen;
    FILE *stream = open_memstream(&msg, &msglen);

    // conto il numero di schedine richieste e lo scrivo all'inizio del messaggio
    int nrichieste = 0;
    for (int i = 0; i < utente.n_giocate; i++)
        if (utente.giocate[i].attiva == attive)
            nrichieste++;
    serializza_int(stream, nrichieste, 0);

    // inserisco tutte le giocate richieste
    for (int i = 0; i < utente.n_giocate; i++)
    {
        if (utente.giocate[i].attiva != attive)
            continue;
        
        serializza_schedina(stream, &utente.giocate[i].schedina);
    }

    // chiudo lo stream per poter usare msg
    fclose(stream);

    // invio il messaggio
    last_msg_operation = MSGOP_NONE;
    send_msg(client_sock, msg);

    // libero msg
    free(msg);

    return 0;
}