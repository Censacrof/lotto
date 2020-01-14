#ifndef DATA_H
#define DATA_H

#include <time.h>
#include <stdio.h>

/*  STRUTTURA DELLE DIRECTORY
    /data
        /schedine_nuove         -> contiene le schedine valide per la prossima estrazione
            schdedina1
            schdedina2
            ...
            schdedinaN

        /estrazioni             -> contiene le estrazioni passate
            estrazione1
            estrazione2
            ...
            estrazioneN

        /utenti                 -> contiene gli utenti
            utente1
            utente2
            ...
            utenteN
*/

#define N_DA_ESTRARRE 5
#define N_DA_GIOCARE 10
#define USERNAME_LEN 20
#define PASSWORDHASH_LEN 64
#define SESSIONID_LEN 10

// macro che restituisce 2^index. usata per generare maschere da usare con gli indici
#define TOMASK(index) 1U << index

// indici degli array contenenti le ruote
enum {
    iBARI,
    iCAGLIARI,
    iFIRENZE,
    iGENOVA,
    iMILANO,
    iNAPOLI,
    iPALERMO,
    iROMA,
    iTORINO,
    iVENEZIA,
    iNAZIONALE,

    N_RUOTE // numero di ruote
};

// indici degli array contenenti i tipi di scommessi
enum {
    iESTRATTO,
    iAMBO,
    iTERNO,
    iQUATERNA,
    iCINQUINA,

    N_TIPI_SCOMMESSE
};

typedef struct estrazione estrazione_t;
typedef struct schedina schedina_t;
typedef struct utente utente_t;

struct estrazione {
    // timestamp corrispondente all'istante dell'estrazione
    long int timestamp;

    // per ogni ruota i numeri estratti corrispondenti
    int ruote[N_RUOTE][N_DA_ESTRARRE];
};

struct schedina {
    // array contenente i numeri da giocare
    int numeri[N_DA_GIOCARE];

    // array contenente per ogni tipo di scommessa l'importo corrispondente (in centesimi)
    int importi_scommesse[N_TIPI_SCOMMESSE];

    // bitmask delle rute su cui si vuole scommettere (da usare con la macro TOMASK)
    unsigned int ruote_selezionate;
};

struct utente {
    char username[USERNAME_LEN + 1];
    char passwordhash[PASSWORDHASH_LEN + 1];
    char sessionid[SESSIONID_LEN + 1];

    // dimesione numero di elementi dell'array giocate
    int n_giocate;

    // array contenente le giocate passate
    struct {
        // vincita in centesimi
        int vincita;

        // booleano
        int attiva;

        schedina_t schedina;
        estrazione_t estrazione;
    } *giocate;
};


// funzioni di serializzazione e deserializzazione
int serializza_utente(FILE *stream, const utente_t *utente);
int serializza_schedina(FILE *stream, const schedina_t *schedina);
int serializza_estrazione(FILE *stream, const estrazione_t *estrazione);

// funzioni di salvataggio dati
int salva_utente(const utente_t *utente);


#endif