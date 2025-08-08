#ifndef ERROR_SNAPSHOT_H
#define ERROR_SNAPSHOT_H

#include <stdint.h>

#define MAX_ERROR_SNAPSHOTS 10 // Numero massimo di errori memorizzabili in RAM

// Struttura che rappresenta uno "snapshot" di errore
typedef struct {
    int rpm;
    int speed;
    int temp;
    int fuel;
    int mil;
    char error_code[16];
    uint64_t timestamp_us;
    char marca[24];
    char modello[24];
    char vin[24];
    char targa[16];
    long km;
} error_snapshot_t;

// --- Funzioni per gestione in RAM ---
void error_snapshot_save(
    const char* error_code, int rpm, int speed, int temp, int fuel, int mil,
    const char* marca, const char* modello, const char* vin, const char* targa, long km
);
int error_snapshot_get_count(void); // Numero di snapshot in RAM
const error_snapshot_t* error_snapshot_get(int idx); // Ottieni puntatore a snapshot specifico


// --- Funzioni per gestione persistente su SPIFFS ---
void error_snapshot_save_to_file(const error_snapshot_t* s);  // Salva singolo snapshot su file
int error_snapshot_load_all_from_file(error_snapshot_t* arr, int max_count);  // Carica tutti gli snapshot dal file
void error_snapshot_clear_file(void); // Cancella file snapshot

#endif // ERROR_SNAPSHOT_H
