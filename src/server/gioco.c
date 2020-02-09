#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>

#include "gioco.h"
#include "../data.h"
#include "../common.h"

void aggiorna_giocate(utente_t *utente, estrazione_t *estrazione);
unsigned int binomialcoeff(unsigned int n, unsigned int k);

// corpo del processo che si occupa di eseguire le estrazioni
void estrattore(const time_t period)
{
    // randomizzo
    srand(time(NULL));

    sprintf(whoiam, "%d:ESTRATTORE", getpid());
    
    while (1)
    {
        // sospendo il processo
        sleep(period);

        // creo un estrazione
        estrazione_t estrazione;
        memset(&estrazione, 0, sizeof(estrazione));

        estrazione.timestamp = time(NULL);

        srand(0); // DEBUG
        for (int i = 0; i < N_RUOTE; i++)
        {
            // creo un alias per la ruota corrente
            int *estratti = estrazione.ruote[i];

            for (int j = 0; j < N_DA_ESTRARRE; j++)
            {
                // estraggo un numero tra 1 e 90 che non sia già stato estratto
                int n;
                while(1)
                {
                    n = 1 + (rand() % 90);

                    // controllo se il numero è già stato estratto su questa ruota
                    for (int k = 0; k < j; k++)
                        if (n == estratti[k])
                            continue;
                    break;
                }

                // aggiungo il numero all'estrazione
                estratti[j] = n;
            }
        }

        // salvo l'estrazione
        salva_estrazione(&estrazione);

        // apro la cartella contenente i file registro di tutti gli utenti
        inizializza_directories();
        DIR *dir = opendir(PATH_UTENTI);
        if (!dir)
        {
            consolelog("impossibile aprire la cartella \"%s\". l'estrazione è stata annullata\n");
            continue;
        }

        // per ogni utente
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            // se l'entry corrente non è un file regolare (ma è una directory, un link, un file speciale, ...)
            // passo oltre
            if (entry->d_type != DT_REG)
                continue;

            // carico l'utente
            utente_t utente;
            if (carica_utente(entry->d_name, &utente) == -1)
            {
                consolelog("impossibile caricare l'utente \"%s\"\n", entry->d_name);
                continue;
            }

            // aggiorno le giocate attive dell'utente
            aggiorna_giocate(&utente, &estrazione);
            
            // salvo l'utente
            if (salva_utente(&utente) == -1)
            {
                consolelog("impossibile salvare utente, esito giocate attive non registrato\n");
                continue;
            }
        }

        // chiudo la cartella
        closedir(dir);

        consolelog("estrazione effettuata\n");
    }

    exit(EXIT_SUCCESS);
}

// calcola e aggiorna le vincite relative a tutte le giocate attive dell'utente in base all'estrazione fornita
void aggiorna_giocate(utente_t *utente, estrazione_t *estrazione)
{
    // scorro tutte le giocate dell'utente
    for (int i = 0; i < utente->n_giocate; i++)
    {
        // se la giocata non è attiva passo oltre
        if (utente->giocate[i].attiva == 0)
            continue;
        
        // imposto la giocata come non attiva
        utente->giocate[i].attiva = 0;

        // copio l'estrazione corrente nella giocata dell'utente
        memcpy(&utente->giocate[i].estrazione, estrazione, sizeof(estrazione_t));

        // ricavo quanti numeri sono stati giocati nella schedina
        int n_numeri_giocati = 0;
        for (int j = 0; j < N_DA_GIOCARE; j++)
            if (utente->giocate[i].schedina.numeri[j] != 0)
                n_numeri_giocati++;

        // ricavo quante sono le ruote della giocata e per ognuna di queste ruote quanti numeri sono stati indovinati
        int n_numeri_indovinati[N_RUOTE];
        int n_ruote_selezionate = 0;
        for (int j = 0; j < N_RUOTE; j++)
        {
            // se la ruota j non è stata selezionata passo oltre
            int mask = 1u << j;
            if (!(utente->giocate[i].schedina.ruote_selezionate & mask))
                continue;

            // conto i numeri indovinati sulla ruota j
            n_numeri_indovinati[n_ruote_selezionate] = 0;            
            for (int k = 0; k < N_DA_GIOCARE; k++)
            {
                if (utente->giocate[i].schedina.numeri[k] == 0)
                    continue;                        

                for (int l = 0; l < N_DA_ESTRARRE; l++)
                    if (utente->giocate[i].schedina.numeri[k] == estrazione->ruote[j][l])
                        n_numeri_indovinati[n_ruote_selezionate]++;
            }

            // incremento il numero di ruote selezionate
            n_ruote_selezionate++;
        }

        // per ogni tipo di scommessa calcolo la vincita
        for (int j = 0; j < N_TIPI_SCOMMESSE; j++)
        {
            // alias
            int *vincita = utente->giocate[i].vincita;
            int *importi_scommesse = utente->giocate[i].schedina.importi_scommesse;

            // inizializzo la vincita corrispondente a questo tipo di scommessa
            vincita[j] = 0;

            // se nella schedina non è presente questo tipo di scommessa passo oltre
            if (importi_scommesse[j] == 0)
                continue;
            
            const double vincite_lorde[N_TIPI_SCOMMESSE] = { 11.23, 250.00, 4500.00, 120000.00, 6000000.00 };

            // sommo la vincita relativa ad ogni ruota selezionata k per questo tipo di scommessa j
            for (int k = 0; k < n_ruote_selezionate; k++)
            {
                // se il numero di numeri indovinati su questa ruota è minore dei numeri richiesti da indovinare per questo tipo di scommessa
                if (n_numeri_indovinati[k] <= j)
                    continue;

                vincita[j] += importi_scommesse[j] * (vincite_lorde[j] / binomialcoeff(n_numeri_giocati, n_numeri_indovinati[k])) / n_ruote_selezionate;
            }
        }
    }
}


// calcolo il coefficiente binomiale come i coefficienti del triangolo di pascal
// (programmazione dinamica)
#define BINOMIALCACHE_SIZE 20
int binomialcache_initialized = 0;
unsigned int binomialcache[BINOMIALCACHE_SIZE][BINOMIALCACHE_SIZE];
unsigned int binomialcoeff(unsigned int n, unsigned int k)
{
    // casi base
    if (n == k || k == 0)
        return 1;
    if (n < 0 || k < 0 || k > n) 
        return 0;
    
    // se la cache non è inizializzata la inizializzo
    if (!binomialcache_initialized)
    {
        for (int i = 1; i < BINOMIALCACHE_SIZE; i++)
            for (int j = 1; j < i; j++)
                binomialcache[i][j] = 0;

        binomialcache_initialized = 1;
    }

    // se l'elemento è presente in cache lo restituisco
    if (n < BINOMIALCACHE_SIZE && k < BINOMIALCACHE_SIZE)
        if (binomialcache[n][k] != 0)
            return binomialcache[n][k];
    
    // altrimenti lo calcolo, lo inserisco in cache e lo restituisco
    unsigned int res = binomialcoeff(n - 1, k - 1) + binomialcoeff(n - 1, k);

    if (n < BINOMIALCACHE_SIZE && k < BINOMIALCACHE_SIZE)
        binomialcache[n][k] = res;

    return res;
}