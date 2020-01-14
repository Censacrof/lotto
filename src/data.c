#include "data.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/file.h>
#include <errno.h>
#include <fcntl.h>

int serializza_utente(FILE *stream, const utente_t *utente)
{
    fprintf(stream, "%s\n%s\n%s\n%d\n",
        utente->username,
        utente->passwordhash,
        utente->sessionid,
        utente->n_giocate
    );

    for (int i = 0; i < utente->n_giocate; i++)
    {
        fprintf(stream, "%d\n%d\n",
            utente->giocate[i].vincita,
            utente->giocate[i].attiva
        );

        serializza_schedina(stream, &utente->giocate[i].schedina);
        serializza_estrazione(stream, &utente->giocate[i].estrazione);
    }

    return 0;
}


int serializza_schedina(FILE *stream, const schedina_t *schedina)
{
    for (int i = 0; i < N_DA_GIOCARE; i++)
        fprintf(stream, "%d ", schedina->numeri[i]);
    fprintf(stream, "\n");

    for (int i = 0; i < N_TIPI_SCOMMESSE; i++)
        fprintf(stream, "%d ", schedina->importi_scommesse[i]);
    fprintf(stream, "\n");

    fprintf(stream, "%d\n", schedina->ruote_selezionate);

    return 0;
}


int serializza_estrazione(FILE *stream, const estrazione_t *estrazione)
{
    fprintf(stream, "%ld\n", estrazione->timestamp);

    for (int i = 0; i < N_RUOTE; i++)
    {
        for (int j = 0; j < N_DA_ESTRARRE; j++)
            fprintf(stream, "%d ", estrazione->ruote[i][j]);
        
        fprintf(stream, "\n");
    }     

    return 0;
}


// percorsi
#define PATH_DATA "data/"
#define PATH_UTENTI PATH_DATA "utenti/"
#define PATH_SCHEDINE_NUOVE PATH_DATA "schedine_nuove/"
#define PATH_ESTRAZIONI PATH_DATA "estrazioni/"

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