#include <stdlib.h>
#include <stdio.h>
#include <crypt.h>
#include <unistd.h>
#include <crypt.h>
#include <string.h>

#include "../commands.h"
#include "../../data.h"

#define N_TENTATIVI 3

// !signup username password
int signup(int client_sock, int nargs, char *args[])
{
    if (nargs != 2)
    {
        send_response(client_sock, SRESP_BADREQ, "numero di parametri errato");
        return 0;
    }

    // controllo se l'username è gia utilizzato ed è valido (3 tentativi)
    char filepath[128];
    int i;
    for (i = 0; i < N_TENTATIVI; i++)
    {   
        sprintf(filepath, "%s%s", PATH_UTENTI, args[0]);
    
        // se l'username è già utilizzato (esiste un file data/utenti/username)
        // oppure non è valido
        if (regex_match(USERNAME_REGEX, args[0], NULL) == 0 || access(filepath, F_OK) != -1)
        {
            if (i == N_TENTATIVI - 1)
            {
                send_response(client_sock, SRESP_BADREQ, "username già in uso/non valido e tentativi terminati");
                return 0;
            }

            // dico al client di mandare un altro username
            send_response(client_sock, SRESP_RETRY, "username già in uso/non valido");
            
            // libero la stringa puntata da args[0]
            free(args[0]);

            // ricevo il nuovo username e lo copio in args[0]
            int reti;
            reti = recv_msg(client_sock, &args[0]);
            if (reti == 0 || reti == -1) // se il client ha chiuso la connessione o c'è stato un errore
                return reti;
        }
    }

    // genero l'hash della password inviata dall'utente utilizzando l'algoritmo sha512crypt
    // (lo stesso usato per le password degli utenti nel file /etc/shadow)
    // devo prima creare la stringa salt nel seguente formato: $id$randomstr$
    // dove: 
    //      - id serve a selezionare l'algoritmo di hashing (6 nel caso di sha512crypt)
    //      - randomstr è una stringa casuale di 16 caratteri utilizzata per perturbare
    //        l'algoritmo rendendo difficile l'utilizzo di rainbow tables per invertire
    //        gli hash delle password più comunemente utilizzate
    char salt[PASSWORDSALT_LEN];
    char randomstr[17];
    randomstr[16] = '\0';
    for (i = 0; i < 16; i++)
    {
        const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ/.";
        int n = rand() % sizeof(charset) - 1;
        randomstr[i] = charset[n];
    }
    sprintf(salt, "$6$%s$", randomstr);

    // calcolo l'hash
    char *hash = crypt(args[1], salt);
    if (!hash)
    {
        consolelog("impossibile generare hash della password\n");
        return -1;
    }

    // creo l'utente
    utente_t utn;
    strcpy(utn.username, args[0]);
    strcpy(utn.passwordhash, hash);
    free(hash);
    utn.sessionid[0] = '\0'; // stringa vuota
    utn.n_giocate = 0;
    utn.giocate = NULL;

    // salvo l'utente
    if (salva_utente(&utn) == -1)
        return -1;

    send_response(client_sock, SRESP_OK, "utente creato con successo");
    return 0;
}