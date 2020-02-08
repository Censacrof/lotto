#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commands.h"
#include "../common.h"

// inizializzo la variabile globale session
struct session session = { "", "" };

// restituisce -1 se va chiusa la connessione (non per forza a causa di un errore)
int execute_command(int client_sock, char *msg, const char *client_addr_str)
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
    char session_id[SESSIONID_LEN + 1] = "";
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
        // copio la stringa contenente il session id
        strncpy(session_id, &matches[S_MSGS_SESSION_ID][1], SESSIONID_LEN); // evito di copiare il carattere S
        session_id[SESSIONID_LEN] = '\0';

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

    // valore che verrà restituito in fondo alla funzione (modificato dalle funzioni che eseguono i comandi)
    int ret = 0;
    
    // in base al comando eseguo la funzione ad esso associata
    // comanindi che non richiedono un sessionid
    if (strcmp(command, "signup") == 0)
    {
        ret = signup(client_sock, nargs, args);
    }
    else if (strcmp(command, "login") == 0)
    {
        ret = login(client_sock, client_addr_str, nargs, args);
    }

    // controllo il sessionid
    else if (strcmp(session_id, session.id) != 0 || strlen(session_id) != SESSIONID_LEN)
    {
        if (session_id[0] == '\0')
            send_response(client_sock, SRESP_BADREQ, "è necessario fornire un sessionid (effettuare login)");
        else
            send_response(client_sock, SRESP_BADREQ, "comando sconosciuto");
        
        ret = 0;
    }

    // comandi che richiedono un sessionid
    else if (strcmp(command, "invia_giocata") == 0)
    {
        ret = invia_giocata(client_sock);
    }

    // comando sconosciuto
    else
    {
        send_response(client_sock, SRESP_BADREQ, "comando sconosciuto");
        ret = 0;
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
    free(command);
    for (int i = 0; i < nargs; i++)
        free(args[i]);
    free(args);    
    
    return ret;
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

    consolelog("rispondo [%s]: %s\n", server_response_str[code], info);
    return 0;
}