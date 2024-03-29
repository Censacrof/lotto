#ifndef COMMANDS_H
#define COMMANDS_H

#include "../common.h"
#include "../data.h"

// espressione regolare (sintassi POSIX estesa) che definisce
// la struttura dei messaggi ricevuti dal server (escluso i primi 2 bytes).
// il primo carattere puo' essere N o S per distinguere i casi
// in cui il messaggio contiene un session id oppure no.
#define S_MSGREGEX "^(N|S[a-z0-9]{10})!([A-z_0-9]+) *(([A-z_0-9]+ *)*) *$"

// indici dei match relativi al caso in di messaggio senza session id
#define S_MSGN_COMMAND 2
#define S_MSGN_ARGS 3

// indici dei match relativi al caso in di messaggio con session id
#define S_MSGS_SESSION_ID 1
#define S_MSGS_COMMAND 2
#define S_MSGS_ARGS 3

// struttura globale che mantiene le informazioni relative alla sessione
struct session {
    char username[USERNAME_LEN + 1];
    char id[SESSIONID_LEN + 1];
};
extern struct session session;

int execute_command(int client_sock, char *msg, const char *client_addr_str);
int send_response(int client_sock, enum server_response code, char *info);

// comandi del server
int signup(int client_sock, int argc, char *args[]);
int login(int client_sock, const char *client_addr_str, int argc, char *args[]);
int invia_giocata(int client_sock);
int vedi_giocate(int client_sock, int argc, char *args[]);
int vedi_estrazione(int client_sock, int argc, char *args[]);
int vedi_vincite(int client_sock);
int esci(int client_sock);

#endif