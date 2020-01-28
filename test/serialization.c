#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include "../src/data.h"
#include "../src/common.h"

#define FILE_PATH "/tmp/serialization_test"

void testint(const long long int before)
{
    long long int after;
    FILE *f = fopen(FILE_PATH, "w");
    serializza_int(f, before, 0);
    fclose(f);
    f = fopen(FILE_PATH, "r");
    deserializza_int(f, &after);
    fclose(f);

    printf("INT %s: %lld -> %lld\t\t\t\n", before == after ? "OK" : "ERR", before, after);
}

void teststr(const char *before)
{
    char *after;
    FILE *f = fopen(FILE_PATH, "w");
    serializza_str(f, before, 0);
    fclose(f);
    f = fopen(FILE_PATH, "r");
    deserializza_str(f, &after);
    fclose(f);

    printf("STR %s: %s -> %s\n", strcmp(before, after) == 0 ? "OK" : "ERR", before, after);
    free(after);
}

void testschedina(const schedina_t *before)
{
    schedina_t after;
    memset(&after, 0, sizeof(schedina_t));

    FILE *f = fopen(FILE_PATH, "w");
    serializza_schedina(f, before);
    fclose(f);
    f = fopen(FILE_PATH, "r");
    deserializza_schedina(f, &after);
    fclose(f);  

    printf("SCD %s\n", memcmp(before, &after, sizeof(schedina_t)) == 0 ? "OK" : "ERR");
}

void testestrazione(const estrazione_t *before)
{
    estrazione_t after;
    memset(&after, 0, sizeof(estrazione_t));

    FILE *f = fopen(FILE_PATH, "w");
    serializza_estrazione(f, before);
    fclose(f);
    f = fopen(FILE_PATH, "r");
    deserializza_estrazione(f, &after);
    fclose(f);  

    int different = memcmp(before, &after, sizeof(estrazione_t));
    printf("EST %s\n", different == 0 ? "OK" : "ERR");
}

void testutente(const utente_t *before)
{
    utente_t after;
    memset(&after, 0, sizeof(utente_t));

    FILE *f = fopen(FILE_PATH, "w");
    serializza_utente(f, before);
    fclose(f);
    f = fopen(FILE_PATH, "r");
    deserializza_utente(f, &after);
    fclose(f);

    int different = 0;
    if (strcmp(before->username, after.username)) { different = 1; goto end; }
    if (strcmp(before->passwordhash, after.passwordhash)) { different = 1; goto end; }
    if (before->n_giocate != after.n_giocate) { different = 1; goto end; }
    if (memcmp(before->giocate, after.giocate, sizeof(*before->giocate) * before->n_giocate)) { different = 1; goto end; }

end:
    printf("UTN %s\n", different == 0 ? "OK" : "ERR");
}

int main(int argc, char *argv[])
{
    testint(42);
    testint(-32);
    printf("-------------------------------------------\n");

    teststr("HelloWorld!");
    teststr("");
    printf("-------------------------------------------\n");

    schedina_t schedina;
    memset(&schedina, 0, sizeof(schedina_t));
    for (int i = 0; i < N_DA_GIOCARE; i++)
        schedina.numeri[i] = (i + 1) * 5;
    for (int i = 0; i < N_TIPI_SCOMMESSE; i++)
        schedina.importi_scommesse[i] = i + 1;
    schedina.ruote_selezionate = TOMASK(iBARI) | TOMASK(iCAGLIARI) | TOMASK(iFIRENZE);
    testschedina(&schedina);
    printf("-------------------------------------------\n");

    estrazione_t estrazione;
    memset(&estrazione, 0, sizeof(estrazione_t));
    estrazione.timestamp = time(NULL);
    for (int i = 0; i < N_RUOTE; i++)
        for (int j = 0; j < N_DA_ESTRARRE; j++)
            estrazione.ruote[i][j] = rand() % 90;
    testestrazione(&estrazione);
    
    printf("-------------------------------------------\n");
    utente_t utente;
    memset(&utente, 0, sizeof(utente_t));
    strcpy(utente.username, "Osvaldo");
    strcpy(utente.passwordhash, "bAmBaGiA");
    utente.n_giocate = 5;
    utente.giocate = calloc(sizeof(*utente.giocate) * utente.n_giocate, 1);
    for (int i = 0; i < utente.n_giocate; i++)
    {
        utente.giocate[i].vincita = rand() % 1000;
        utente.giocate[i].vincita = rand() % 2;

        for (int j = 0; j < N_DA_GIOCARE; j++)
            utente.giocate[i].schedina.numeri[j] = rand() % 90;
        for (int j = 0; j < N_TIPI_SCOMMESSE; j++)
            utente.giocate[i].schedina.importi_scommesse[j] = j + 1;
        utente.giocate[i].schedina.ruote_selezionate = TOMASK(iBARI) | TOMASK(iCAGLIARI) | TOMASK(iFIRENZE);

        utente.giocate[i].estrazione.timestamp = time(NULL);
        for (int j = 0; j < N_RUOTE; j++)
            for (int k = 0; k < N_DA_ESTRARRE; k++)
                utente.giocate[i].estrazione.ruote[j][k] = rand() % 90;
    }
    testutente(&utente);

    return 0;
}