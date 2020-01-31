#include <stdlib.h>
#include <stdio.h>
#include <crypt.h>
#include <unistd.h>
#include <crypt.h>
#include <string.h>

#include "../commands.h"
#include "../../data.h"

#define N_LOGINTRIES 3

int tries_left = N_LOGINTRIES;

// !login username password
int login(int client_sock, const char *client_addr_str, int nargs, char *args[])
{
    if (nargs != 2)
    {
        send_response(client_sock, SRESP_BADREQ, "numero di parametri errato");
        return 0;
    }

    // controllo che l'username e la password siano validi
    // è importante per proteggersi da attacchi di PATH INJECTION
    if (regex_match(USERNAME_REGEX, args[0], NULL) == 0 || regex_match(PASSWORD_REGEX, args[1], NULL) == 0)
        goto wrongcredentials;

    // l'username è case insensitive, quindi lo converto in lowercase per facilitare i confronti
    for (char *c = args[0]; *c; c++) *c = *c >= 'A' && *c <= 'Z' ? *c + 32 : *c;

    // controllo se l'utente esiste
    if (!utente_exists(args[0]))
        goto wrongcredentials;
    
    // carico l'utente
    utente_t utente;
    if (carica_utente(args[0], &utente) == -1)
        return -1;
    
    // estraggo il salt da utente.password
    char salt[PASSWORDSALT_LEN + 1];
    strncpy(salt, utente.passwordhash, PASSWORDSALT_LEN);
    salt[PASSWORDSALT_LEN] = '\0';

    // calcolo l'hash della password mandata dal client con il salt dell'utente
    char *newhash = crypt(args[1], salt);

    // confronto il nuovo hash con quello vecchio
    if (strcmp(newhash, utente.passwordhash) != 0)
        goto wrongcredentials;

    // la password è corretta
    tries_left = N_LOGINTRIES;

    // genero sessionid (10 caratteri alphanumerici)
    char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    int charsetlen = strlen(charset);
    for (int i = 0; i < SESSIONID_LEN; i++)
    {
        session.id[i] = charset[rand() % (charsetlen + 1)];
    }
    session.id[SESSIONID_LEN] = '\0';
    
    // invio il sessionid al client
    send_response(client_sock, SRESP_OK, session.id);
    return 0;

wrongcredentials:
    if (--tries_left <= 0)
        blacklist(client_addr_str);
    
    send_response(client_sock, SRESP_RETRY, "username o password sono invalidi");
    return tries_left > 0 ? 0 : -1;
}