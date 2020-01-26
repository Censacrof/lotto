#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commands.h"
#include "../common.h"

int execute_command(int client_sock, char *msg)
{
    char **matches;
    int nmatches;

    // se il messaggio non produce match
    if ((nmatches = regex_match(S_MSGREGEX, msg, &matches)) == 0)
    {
        send_response(client_sock, SRESP_BADREQ, "il formato della richiesta è errato");
        return 0;
    }

    // estraggo le seguenti informazioni dal messaggio
    char *session_id = NULL;
    char *command = NULL; int command_index;
    char **args = NULL; int args_index;
    int nargs = 0;

    if (msg[0] == 'N') // messaggio senza session id
    {
        command_index = S_MSGN_COMMAND;
        args_index = S_MSGN_ARGS;
    } 
    else if (msg[0] == 'S') // messaggio con session id
    {
        // alloco e copio la stringa contenente il session id
        session_id = malloc(sizeof(char) * (strlen(matches[S_MSGS_SESSION_ID])));
        strcpy(session_id, &matches[S_MSGS_SESSION_ID][1]); // evito di copiare il carattere S

        command_index = S_MSGS_COMMAND;
        args_index = S_MSGS_ARGS;
    }

    // alloco e copio la stringa contenente il comando
    command = malloc(sizeof(char) * (strlen(matches[command_index]) + 1));
    strcpy(command, matches[command_index]);

    // eseguo la tokenizzazione degli argomenti
    const char delimiters[] = " \n";
    nargs = 0;
    char *token = strtok(matches[args_index], delimiters);
    while (token)
    {
        nargs++;
        args = realloc(args, sizeof(char *) * nargs);
        args[nargs - 1] = malloc(sizeof(char) * (strlen(token) + 1));
        strcpy(args[nargs - 1], token);
        token = strtok(NULL, delimiters);
    }

    // stampo il comando che il client sta tentando di eseguire
    char strbuff[50];
    int written = sprintf(strbuff, "!%s", command);
    for (int i = 0; i < nargs; i++)
        written += sprintf(
            strbuff + written,
            " %s",
            ((!strcmp("signup", command) || !strcmp("login", command)) && i == 1) ? "*******" : args[i]
        );
    consolelog("comando ricevuto: %s\n", strbuff);

    // in base al comando eseguo la funzione ad esso associata
    int ret;
    if (strcmp(command, "signup") == 0)
    {
        ret = signup(client_sock, nargs, args);
    }
    else if (strcmp(command, "login") == 0)
    {
        ret = login(client_sock, nargs, args);
    }
    else
    {
        send_response(client_sock, SRESP_BADREQ, "comando sconosciuto");
    }

    // segnalo l'esito dell'operazione al client (se non è già stato segnalato dalle funzioni che l'hanno gestita)
    if (last_msg_operation != MSGOP_SEND)
    {
        if (ret == 0)
            send_response(client_sock, SRESP_OK, "ok");
        else
            send_response(client_sock, SRESP_ERR, "si è verificato un errore");
    }

    // libero le risorse che non servono piu'
    regex_match_free(&matches, nmatches);
    if (session_id) free(session_id);
    free(command);
    for (int i = 0; i < nargs; i++)
        free(args[i]);
    free(args);    
    
    return 0;
}


int send_response(int client_sock, enum server_response code, char *info)
{
    char buff[128];
    if (info)
        sprintf(buff, "%d %s", code, info);
    else 
        sprintf(buff, "%d", code);
    
    if (send_msg(client_sock, buff) == -1)
    {
        consolelog("ERRORE INTERNO, impossibile iviare risposta: %s", buff);
        return -1;
    }

    consolelog("rispondo: %s\n", buff);
    return 0;
}