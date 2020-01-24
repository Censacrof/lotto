#include "data.h"

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
    
    if (mkdir(PATH_SCHEDINE_NUOVE, mode) == -1)
        if (errno != EEXIST)
            return -1;
    
    if (mkdir(PATH_ESTRAZIONI, mode) == -1)
        if (errno != EEXIST)
            return -1;

    return 0;
}


// -------------------------- int --------------------------
int serializza_int(FILE *stream, long long int i, int usespace)
{ 
    char sep = usespace ? ' ' : '\n';
    fprintf(stream, "%lld%c", i, sep);
    return 0;
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


// ------------------------- utente ------------------------
int serializza_utente(FILE *stream, const utente_t *utente)
{
    serializza_str(stream, utente->username, 0);
    serializza_str(stream, utente->passwordhash, 0);
    serializza_str(stream, utente->sessionid, 0);
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