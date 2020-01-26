#include <stdlib.h>
#include <stdio.h>
#include <crypt.h>
#include <unistd.h>
#include <crypt.h>
#include <string.h>

#include "../commands.h"
#include "../../data.h"

// !login username password
int login(int client_sock, int nargs, char *args[])
{
    if (nargs != 2)
    {
        send_response(client_sock, SRESP_BADREQ, "numero di parametri errato");
        return 0;
    }

    // controllo che l'username e la password siano validi
    // è importante per proteggersi da attacchi di PATH INJECTION
    if (regex_match(USERNAME_REGEX, args[0], NULL) == 0 || regex_match(PASSWORD_REGEX, args[1], NULL) == 0)
    {
        send_response(client_sock, SRESP_BADREQ, "username o password contengono dei caratteri non validi");
        return 0;
    }

    // l'username è case insensitive, quindi lo converto in lowercase per facilitare i confronti
    for (char *c = args[0]; *c; c++) *c = *c >= 'A' && *c <= 'Z' ? *c + 32 : *c;

    // controllo se l'utente esiste
    if (!utente_exists(args[0]))
    {
        send_response(client_sock, SRESP_RETRY, "username o password sono invalidi");
        return 0;
    }
    
    // carico l'utente
    utente_t utente;
    if (carica_utente(args[0], &utente) == -1)
        return -1;
    
    // estraggo il salt da username->password
    char salt[PASSWORDSALT_LEN]; 
    strncpy(salt, utente.passwordhash, PASSWORDSALT_LEN - 1);
    salt[PASSWORDHASH_LEN - 1] = '\0';

    // calcolo l'hash della password mandata dal client con il salt dell'utente
    char *newhash = crypt(args[1], salt);

    // confronto in nuovo hash con quello vecchio
    if (strcmp(newhash, utente.passwordhash) != 0)
    {
        // la password non è correttta
        send_response(client_sock, SRESP_RETRY, "username o password sono invalidi");
        free(newhash);
        return 0;
    }

    // la password è corretta
    free(newhash);

    // genero sessionid (10 caratteri alphanumerici)
    char charset[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    for (int i = 0; i < SESSIONID_LEN; i++)
        utente.sessionid[i] = charset[rand() % sizeof(charset)];

    // salvo l'utente (con il nuovo sessionid)
    salva_utente(&utente);
    
    send_response(client_sock, SRESP_OK, utente.sessionid);
    return 0;
}