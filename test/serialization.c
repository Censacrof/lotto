#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include "../src/data.h"

int main(int argc, char *argv[])
{
    utente_t utn;
    sprintf(utn.username, "ugo");
    sprintf(utn.passwordhash, "baldo");
    sprintf(utn.sessionid, "a1b2c3d4e5");

    utn.n_giocate = 3;
    utn.giocate = malloc(sizeof(*utn.giocate) * utn.n_giocate);
    for (int i = 0; i < utn.n_giocate; i++)
    {
        utn.giocate[i].attiva = 0;
        utn.giocate[i].vincita = i;

        // schedina
        for (int j = 0; j < N_DA_GIOCARE; j++)
            utn.giocate[i].schedina.numeri[j] = j * 3;

        for (int j = 0; j < N_TIPI_SCOMMESSE; j++)
            utn.giocate[i].schedina.importi_scommesse[j] = i * j;
        
        utn.giocate[i].schedina.ruote_selezionate = TOMASK(iBARI) | TOMASK(iFIRENZE);

        // estrazione
        utn.giocate[i].estrazione.timestamp = time(NULL);

        for (int j = 0; j < N_RUOTE; j++)
            for (int k = 0; k < N_DA_ESTRARRE; k++)
                utn.giocate[i].estrazione.ruote[j][k] = (j + k) % 2;
    }

    serializza_utente(stdout, &utn);

    if (salva_utente(&utn) == -1)
        perror("impossibile serializzare utente");

    char buff[256];

    return 0;
}