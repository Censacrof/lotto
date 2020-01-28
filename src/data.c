#include "data.h"
#include "common.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/file.h>
#include <errno.h>
#include <fcntl.h>

// crea le cartelle necessarie se non sono già presenti
int inizializza_directories()
{
    const mode_t mode = 0777; // permessi di accesso

    if (mkdir(PATH_DATA, mode) == -1)
        if (errno != EEXIST)
            return -1;
    
    if (mkdir(PATH_UTENTI, mode) == -1)
        if (errno != EEXIST)
            return -1;
    
    if (mkdir(PATH_ESTRAZIONI, mode) == -1)
        if (errno != EEXIST)
            return -1;

    return 0;
}

// preleva un token (separato da \n, '\r', '\t', ' ') dallo stream e lo alloca dinamicamente in *tok
// restituisce la lunghezza del token oppure -1 in caso di errore. ricordarsi di usare free.
int streamgettoken(FILE *stream, char **tok)
{
    if (!tok)
    {
        errno = EINVAL; // invalid argument
        return -1;
    }

    // alloco una stringa vuota
    *tok = malloc(sizeof(char) * 1);
    (*tok)[0] = '\0';
    int toklen = 0;

    // scorro lo stream finchè nono trovo un carattere che non sia un separatore
    char c;
    do {
        c = fgetc(stream);
    } while((c == '\n' || c == '\r' || c == '\t' || c == ' ') && c != EOF);
    
    do {
        // se lo stream è terminato esco
        if (c == EOF)
            return toklen;

        (*tok)[toklen] = c; // scrivo il carattere nella stringa
        toklen++;
        *tok = realloc(*tok, sizeof(char) * (toklen + 1)); // alloco un byte in più per '\0'
        (*tok)[toklen] = '\0'; // rendo la stringa null terminated

        c = fgetc(stream);
    } while(c != '\n' && c != '\r' && c != '\t' && c != ' ');

    return toklen;
}


// -------------------------- int --------------------------
int serializza_int(FILE *stream, long long int i, int usespace)
{ 
    char sep = usespace ? ' ' : '\n';
    fprintf(stream, "%lld%c", i, sep);
    return 0;
}

int deserializza_int(FILE *stream, long long int *i)
{
    char *tok;
    if (streamgettoken(stream, &tok) == -1)
        goto error;

    if (regex_match("^[+-]{0,1}[0-9]{1,}$", tok, NULL) == 0)
    {
        free(tok);
        goto error;
    }        
    
    sscanf(tok, "%lld", i);
    free(tok);
    return 0;

error:
    consolelog("impossibile deserializzare un intero, assegno valore di default\n");
    *i = 0;
    return -1;
}

// -------------------------- str --------------------------
int serializza_str(FILE *stream, const char *s, int usespace)
{
    char sep = usespace ? ' ' : '\n';

    // le stringhe vuote vengono serializzate tramite la sequenza di caratteri \0
    if (s[0] == '\0')
        fprintf(stream, "\\0%c", sep);
    else
        fprintf(stream, "%s%c", s, sep);
    
    return 0;
}

// ricordarsi di usare free
int deserializza_str(FILE *stream, char **s)
{
    if (streamgettoken(stream, s) == -1)
    {
        consolelog("impossibile deserializzare una stringa, assegno valore di default\n");
        if (s)
        {
            // in caso di errore imposto la stringa vuota
            *s = malloc(sizeof(char) * 1);
            (*s)[0] = '\0';
        }
        return -1;
    }

    // la sequenza di caratteri \0 indica una stringa vuota
    if (strcmp(*s, "\\0") == 0)
    {
        free(*s);
        *s = malloc(sizeof(char) * 1);
        (*s)[0] = '\0';
    }
        
    return 0;
}


// ------------------------ schedina -----------------------
int serializza_schedina(FILE *stream, const schedina_t *schedina)
{
    int i;
    for (i = 0; i < N_DA_GIOCARE; i++)
        serializza_int(stream, schedina->numeri[i], 1);
    fprintf(stream, "\n");

    for (i = 0; i < N_TIPI_SCOMMESSE; i++)
        serializza_int(stream, schedina->importi_scommesse[i], 1);
    fprintf(stream, "\n");

    serializza_int(stream, schedina->ruote_selezionate, 0);

    return 0;
}

int deserializza_schedina(FILE *stream, schedina_t *schedina)
{
    long long int bigint;
    int i;
    for (i = 0; i < N_DA_GIOCARE; i++)
    {
        deserializza_int(stream, &bigint);
        schedina->numeri[i] = bigint;
    }        
    
    for (i = 0; i < N_TIPI_SCOMMESSE; i++)
    {
        deserializza_int(stream, &bigint);
        schedina->importi_scommesse[i] = bigint;
    }
      
    deserializza_int(stream, &bigint);
    schedina->ruote_selezionate = bigint;

    return 0;
}


// ----------------------- estrazione ----------------------
int serializza_estrazione(FILE *stream, const estrazione_t *estrazione)
{
    serializza_int(stream, estrazione->timestamp, 0);

    int i;
    for (i = 0; i < N_RUOTE; i++)
    {
        int j;
        for (j = 0; j < N_DA_ESTRARRE; j++)
            serializza_int(stream, estrazione->ruote[i][j], 1);
        
        fprintf(stream, "\n");
    }     

    return 0;
}

int deserializza_estrazione(FILE *stream, estrazione_t *estrazione)
{
    long long int bigint;
    deserializza_int(stream, &bigint);
    estrazione->timestamp = bigint;

    int i, j;
    for (i = 0; i < N_RUOTE; i++)
        for (j = 0; j < N_DA_ESTRARRE; j++)
        {
            deserializza_int(stream, &bigint);
            estrazione->ruote[i][j] = bigint;
        }
    
    return 0;
}

// ------------------------- utente ------------------------
int serializza_utente(FILE *stream, const utente_t *utente)
{
    serializza_str(stream, utente->username, 0);
    serializza_str(stream, utente->passwordhash, 0);
    serializza_int(stream, utente->n_giocate, 0);

    int i;
    for (i = 0; i < utente->n_giocate; i++)
    {
        serializza_int(stream,  utente->giocate[i].vincita, 0);
        serializza_int(stream,  utente->giocate[i].attiva, 0);

        serializza_schedina(stream, &utente->giocate[i].schedina);
        serializza_estrazione(stream, &utente->giocate[i].estrazione);
    }

    return 0;
}

int deserializza_utente(FILE *stream, utente_t *utente)
{
    char *str;
    deserializza_str(stream, &str);
    strncpy(utente->username, str, USERNAME_LEN); 
    utente->username[USERNAME_LEN] = '\0';
    free(str);

    deserializza_str(stream, &str);
    strncpy(utente->passwordhash, str, PASSWORDHASH_LEN); 
    utente->passwordhash[PASSWORDHASH_LEN] = '\0';
    free(str);

    long long int bigint;
    deserializza_int(stream, &bigint);
    utente->n_giocate = bigint;

    // calloc è come malloc ma riempie di zeri la memoria allocata
    // (mi serve per poter confrontare le struture dati con memcmp)
    utente->giocate = calloc(sizeof(*utente->giocate) * utente->n_giocate, 1);

    int i;
    for (i = 0; i < utente->n_giocate; i++)
    {
        deserializza_int(stream, &bigint);
        utente->giocate[i].vincita = bigint;

        deserializza_int(stream, &bigint);
        utente->giocate[i].attiva = bigint;

        deserializza_schedina(stream, &utente->giocate[i].schedina);
        deserializza_estrazione(stream, &utente->giocate[i].estrazione);
    }

    return 0;
}

// retistuisce true se esiste un utente con questo username
int utente_exists(const char *username)
{
    char filename[128];
    sprintf(filename, "%s%s", PATH_UTENTI, username);

    return access(filename, F_OK) == 0;
}

int salva_utente(const utente_t *utente)
{
    // creo le cartelle necessarie se non sono già presenti
    if (inizializza_directories() == -1)
        return -1;

    char filename[64];
    sprintf(filename, PATH_UTENTI "%s", utente->username);

    // apro il file (lo creo se non esiste assegnando i diritti di accesso)
    int fd = open(filename, O_WRONLY | O_CREAT, 0755);
    if (fd == -1)
        return fd;

    // eseguo il lock esclusivo sul file
    // (si usa il lock esclusivo quando si scrive e quello condiviso quando si legge)
    flock(fd, LOCK_EX);

    // associo uno stream al file descriptor
    FILE *stream = fdopen(fd, "w");
    if (!stream)
        return -1;

    serializza_utente(stream, utente);

    // rimuovo il lock
    flock(fd, LOCK_UN);

    // chiudo stream (al suo interno chiama anche close)
    fclose(stream);

    return 0;
}


int carica_utente(const char *username, utente_t *utente)
{
    char filename[128];
    sprintf(filename, "%s%s", PATH_UTENTI, username);

    // controllo che l'utente esista
    if (!utente_exists(username))
        return -1;

    // apro il file
    int fd = open(filename, O_RDONLY);
    if (fd == -1)
        return -1;
    
    // eseguo il lock condiviso sul file
    flock(fd, LOCK_SH);

    // associo uno stream al file descriptor
    FILE *stream = fdopen(fd, "r");
    if (!stream)
        return -1;
    
    deserializza_utente(stream, utente);

    // rimuovo il lock
    flock(fd, LOCK_UN);

    // chiudo stream (al suo interno chiama anche close)
    fclose(stream);

    return 0;
}