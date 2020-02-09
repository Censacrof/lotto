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

    // l'username è case insensitive, quindi lo converto in lowercase per facilitare i confronti
    for (char *c = args[0]; *c; c++) *c = *c >= 'A' && *c <= 'Z' ? *c + 32 : *c;

    while (1)
    {
        // controllo se l'username è valido
        int nmatches;
        char **matches;
        if ((nmatches = regex_match("^[ \n\r\t]*(" USERNAME_REGEX_WITHOUT_ANCHORS ")[ \n\r\t]*$", args[0], &matches)) != 2)
            goto nonvalidusername;
        
        // sostituisco in args[0] la versione "estratta" dell'username (senza spazi all'inizio o alla fine)
        free(args[0]);
        args[0] = malloc(sizeof(char) * (strlen(matches[1]) + 1));
        strcpy(args[0], matches[1]);
        regex_match_free(&matches, nmatches);

        // se l'username non è già utilizzato (non esiste un file data/utenti/username)
        // esco dal ciclo
        if (!utente_exists(args[0]))
            break;

    nonvalidusername:
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

    // genero l'hash della password inviata dall'utente utilizzando l'algoritmo sha512crypt
    // (lo stesso usato per le password degli utenti nel file /etc/shadow)
    // devo prima creare la stringa salt nel seguente formato: $id$randomstr$
    // dove: 
    //      - id serve a selezionare l'algoritmo di hashing (6 nel caso di sha512crypt)
    //      - randomstr è una stringa casuale di 16 caratteri utilizzata per perturbare
    //        l'algoritmo rendendo difficile l'utilizzo di rainbow tables per invertire
    //        gli hash delle password più comunemente utilizzate
    char salt[PASSWORDSALT_LEN + 1];
    char randomstr[17];
    randomstr[16] = '\0';
    for (int i = 0; i < 16; i++)
    {
        const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ/.";
        randomstr[i] = charset[rand() % sizeof(charset - 1)];
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
    utn.n_giocate = 0;
    utn.giocate = NULL;

    // salvo l'utente
    if (salva_utente(&utn) == -1)
        return -1;

    send_response(client_sock, SRESP_OK, "utente creato con successo");
    return 0;
}