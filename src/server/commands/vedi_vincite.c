#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "../commands.h"
#include "../../data.h"

// !vedi_vincite 
int vedi_vincite(int client_sock)
{
    // leggo l'utente
    utente_t utente;
    if (carica_utente(session.username, &utente) == -1)
    {
        send_response(client_sock, SRESP_ERR, "si è verificato un errore");
        consolelog("impossibile caricare l'utente %s\n", session.username);
        return 0;
    }

    // do l'ok al client
    send_response(client_sock, SRESP_OK, "prepararsi a ricevere le vittorie");

    // conto il numero di vincite dell'utente e salvo l'indirizzo delle giocate vincenti
    int nvittorie = 0;
    giocata_t **giocate_vincenti = malloc(sizeof(giocata_t *) * utente.n_giocate);

    int i;
    for (i = 0; i < utente.n_giocate; i++)
    {
        // se la giocata è ancora attiva passo oltre
        if (utente.giocate[i].attiva == 1)
            continue;

        // controllo se la giocata è vincente
        int j;
        for (j = 0; j < N_TIPI_SCOMMESSE; j++)
        {
            if (utente.giocate[i].vincita[j] > 0)
            {
                giocate_vincenti[nvittorie] = &utente.giocate[i];
                nvittorie++;
                break;
            }
        }
    }

    // apro un mem stream per il messaggio da inviare
    char *msg;
    size_t msglen;
    FILE *stream = open_memstream(&msg, &msglen);

    // scrivo nel messaggio il numero di vincite
    serializza_int(stream, nvittorie, 0);

    // scrivo le giocate vincenti
    for (i = 0; i < nvittorie; i++)
    {
        serializza_giocata(stream, giocate_vincenti[i]);
    }

    // libero giocate vincenti
    free(giocate_vincenti);

    // chiudo lo stream così da poter usare msg
    fclose(stream);

    // invio il messaggio
    last_msg_operation = MSGOP_NONE;
    send_msg(client_sock, msg);

    // libero msg
    free(msg);

    return 0;
}