#ifndef DATA_H
#define DATA_H

#include <time.h>
#include <stdio.h>
#include <arpa/inet.h>

/*  STRUTTURA DELLE DIRECTORY
    /data
        /utenti                 -> contiene gli utenti
            utente1
            utente2
            ...
            utenteN

        estrazioni              -> contiene le estrazioni passate
        blacklist               -> file contenente ip e timestamp degli ip blacklistati
*/

// percorsi
#define PATH_DATA "data/"
#define PATH_UTENTI PATH_DATA "utenti/"
#define PATH_ESTRAZIONI PATH_DATA "estrazioni"
#define PATH_BLACKLIST PATH_DATA "blacklist"

// costanti
#define N_DA_ESTRARRE 5
#define N_DA_GIOCARE 10
#define USERNAME_LEN 20
#define USERNAME_REGEX_WITHOUT_ANCHORS "[a-zA-Z0-9_]{3,20}"
#define USERNAME_REGEX "^" USERNAME_REGEX_WITHOUT_ANCHORS "$" // è importante limitare i caratteri utilizzabili per proteggerci da attacchi PATH INJECTION
#define PASSWORDSALT_LEN 20 // modalità + salt + separatori
#define PASSWORDHASH_LEN 106 // modalità + salt + hash (sha512crypt) + separatori
#define PASSWORD_REGEX "^[a-zA-Z0-9_]{3,20}$"
#define BLACKLIST_TIMETOWAIT 60 * 30 // 30 minuti

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

// array di strighe che contengono il nome delle varie ruote
extern const char ruote_str[N_RUOTE][16];

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
typedef struct giocata giocata_t;
typedef struct utente utente_t;

struct estrazione {
    // timestamp corrispondente all'istante dell'estrazione
    time_t timestamp;

    // per ogni ruota i numeri estratti corrispondenti
    int ruote[N_RUOTE][N_DA_ESTRARRE];
};

// lista degli importi che è possibile puntare (in centesimi)
extern const int importi_possibili[];

// array contenente il nome dei tipi di scommesse
extern const char tipi_scommesse_str[N_TIPI_SCOMMESSE][16];

struct schedina {
    // array contenente i numeri da giocare
    int numeri[N_DA_GIOCARE];

    // array contenente per ogni tipo di scommessa l'importo corrispondente (in centesimi)
    int importi_scommesse[N_TIPI_SCOMMESSE];

    // bitmask delle rute su cui si vuole scommettere (da usare con la macro TOMASK)
    unsigned int ruote_selezionate;
};

// array contenente le giocate passate
struct giocata {
    // vincita in centesimi per ogni tipo di scommessa
    int vincita[N_TIPI_SCOMMESSE];

    // booleano
    int attiva;

    schedina_t schedina;
    estrazione_t estrazione;
};

struct utente {
    char username[USERNAME_LEN + 1];
    char passwordhash[PASSWORDHASH_LEN + 1];

    // dimesione numero di elementi dell'array giocate
    int n_giocate;

    // array contenente le giocate passate
    giocata_t *giocate;
};

int inizializza_directories();

// funzioni booleane per controllare la validità dei dati ed evntualmente convertirli
int numero_da_giocare_valido(const char *numstr, int *num);
int importo_valido(const char *importostr, int *importo);
int ruota_valida(const char *ruotastr, unsigned int *ruotamask);

// funzioni di serializzazione e deserializzazione
int serializza_int(FILE *stream, long long int i, int usespace);
int serializza_str(FILE *stream, const char *s, int usespace);
int serializza_utente(FILE *stream, const utente_t *utente);
int serializza_schedina(FILE *stream, const schedina_t *schedina);
int serializza_estrazione(FILE *stream, const estrazione_t *estrazione);
int serializza_giocata(FILE *stream, const giocata_t *giocata);

int deserializza_int(FILE *stream, long long int *i);
int deserializza_str(FILE *stream, char **s);
int deserializza_utente(FILE *stream, utente_t *utente);
int deserializza_schedina(FILE *stream, schedina_t *schedina);
int deserializza_estrazione(FILE *stream, estrazione_t *estrazione);
int deserializza_giocata(FILE *stream, giocata_t *giocata);

// funzioni di salvataggio/controllo dati non volatili
int utente_exists(const char *username);
int salva_utente(const utente_t *utente);
int carica_utente(const char *username, utente_t *utente);
int salva_giocata(const char *username, const schedina_t *schedina);

int salva_estrazione(const estrazione_t *estrazione);

int blacklist(const char *ip);
int is_blacklisted(const char *ip, time_t *timeleft);

#endif