#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "../commands.h"
#include "../../data.h"

// !vedi_estrazione n ruota
int vedi_estrazione(int client_sock, int argc, char *args[])
{
    if (argc != 1 && argc != 2)
    {
        send_response(client_sock, SRESP_BADREQ, "numero di parametri errato");
        return 0;
    }

    // controllo n
    int n;
    if (regex_match("^[0-9]+$", args[0], NULL) == 0)
    {
    nonvalidn:
        send_response(client_sock, SRESP_ERR, "n deve essere un numero positivo tra 1 e 20");
        return 0;
    }

    // estraggo n
    sscanf(args[0], "%d", &n);
    if (n < 1 || n > 20)
        goto nonvalidn;
    
    // se è specificata ricavo l' indice corrispondente alla ruota
    int indiceruota;
    if (argc == 2)
    {
        // controllo se la ruota specificata è valida
        int trovata = 0;
        int i;
        for (i = 0; i < N_RUOTE; i++)
        {
            if (strcmp(args[1], ruote_str[i]) == 0)
            {
                trovata = 1;
                indiceruota = i;
                break;
            }
        }

        if (!trovata)
        {
            send_response(client_sock, SRESP_ERR, "la ruota specificata non è valida");
        }
    }

    // apro il file delle estrazioni
    FILE *stream = fopen(PATH_ESTRAZIONI, "r");
    if (!stream)
    {
        send_response(client_sock, SRESP_ERR, "non ci sono estrazioni");
        return 0;
    }

    // avverto il client che sto per inviare le estrazioni
    send_response(client_sock, SRESP_OK, "prepararsi a ricevere le estrazioni");

    // alloco un array di estrazioni di n elementi da usare come stack circolare
    estrazione_t *estrazioni = malloc(sizeof(estrazione_t) * n);
    int testa = 0;

    // leggo tutte le estrazioni e le metto nella coda circolare
    int nlette = 0;
    while (deserializza_estrazione(stream, &estrazioni[testa]) != -1)
    {
        testa = (testa + 1) % n;
        nlette++;
    }

    // chiudo il file delle estrazioni
    fclose(stream);

    // apro un mem stream per il messaggio
    char *msg;
    size_t msglen;
    stream = open_memstream(&msg, &msglen);

    // inserisco nel messaggio il numero di estrazioni contenute nel messaggio
    int to_send = nlette < n ? nlette : n;
    serializza_int(stream, to_send, 0);
    
    // inserisco nel messaggio le utilme min(nlette, n) estrazioni al client in ordine decrescente di data
    // (si assume che il file delle estrazioni sia ordinato)
    int i;
    for (i = 0; i < to_send; i++)
    {
        testa--;
        if (testa < 0)
            testa = n - 1;

        // se la ruota è specificata invio solo quella
        if (argc == 2)
        {
            // scivo il timestamp
            serializza_int(stream, estrazioni[testa].timestamp, 0);
            
            // scrivo i numeri estratti
            int j;
            for (j = 0; j < N_DA_ESTRARRE; j++)
                serializza_int(stream, estrazioni[testa].ruote[indiceruota][j], 0);
        }

        // altrimenti invio l'estrazione di tutte le ruote
        else
        {
            serializza_estrazione(stream, &estrazioni[testa]);
        }

        fprintf(stream, "\n");
    }

    // libero lo stack circolare
    free(estrazioni);

    // chiudo il mem stream così da poter usare msg
    fclose(stream);

    // invio il messaggio contenente le estrazioni
    last_msg_operation = MSGOP_NONE;
    send_msg(client_sock, msg);

    // libero msg
    free(msg);

    return 0;
}