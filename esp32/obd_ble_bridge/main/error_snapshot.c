#include "error_snapshot.h"
#include <string.h>
#include <stdio.h>
#include "esp_timer.h" // Per ottenere timestamp in microsecondi
#include "esp_log.h"

// Array in RAM per memorizzare gli snapshot degli errori
static error_snapshot_t snapshots[MAX_ERROR_SNAPSHOTS];
static int snapshot_count = 0; // Numero di snapshot attualmente salvati in RAM

// --- SALVATAGGIO IN RAM ---
void error_snapshot_save(
    const char* error_code, int rpm, int speed, int temp, int fuel, int mil,
    const char* marca, const char* modello, const char* vin, const char* targa, long km
) {
    // Se pieno, shift a sinistra e sovrascrive il più vecchio
    if (snapshot_count >= MAX_ERROR_SNAPSHOTS) {
        for (int i = 1; i < MAX_ERROR_SNAPSHOTS; ++i)
            snapshots[i-1] = snapshots[i];
        snapshot_count = MAX_ERROR_SNAPSHOTS - 1;
    }
    // Inserisce il nuovo snapshot in fondo
    error_snapshot_t* s = &snapshots[snapshot_count++];
    s->rpm = rpm;
    s->speed = speed;
    s->temp = temp;
    s->fuel = fuel;
    s->mil = mil;
    s->km = km;

    // Copia stringhe con protezione da overflow
    strncpy(s->error_code, error_code, sizeof(s->error_code)-1);
    s->error_code[sizeof(s->error_code)-1] = 0;
    strncpy(s->marca, marca, sizeof(s->marca)-1);
    s->marca[sizeof(s->marca)-1] = 0;
    strncpy(s->modello, modello, sizeof(s->modello)-1);
    s->modello[sizeof(s->modello)-1] = 0;
    strncpy(s->vin, vin, sizeof(s->vin)-1);
    s->vin[sizeof(s->vin)-1] = 0;
    strncpy(s->targa, targa, sizeof(s->targa)-1);
    s->targa[sizeof(s->targa)-1] = 0;

    // Timestamp in microsecondi
    s->timestamp_us = esp_timer_get_time();
    
    // Salva subito anche su SPIFFS per persistenza
    error_snapshot_save_to_file(s); // ogni nuovo errore viene subito salvato su SPIFFS
}

// Ritorna numero di snapshot in RAM
int error_snapshot_get_count(void) {
    return snapshot_count;
}

// Ritorna puntatore a snapshot specifico
const error_snapshot_t* error_snapshot_get(int idx) {
    if (idx < 0 || idx >= snapshot_count) return NULL;
    return &snapshots[idx];
}

// --- SALVATAGGIO SU SPIFFS (formato csv) ---
void error_snapshot_save_to_file(const error_snapshot_t* s) {
    FILE* f = fopen("/spiffs/snapshots.txt", "a");
    if (!f) return;
    // salva: timestamp,error_code,rpm,speed,temp,fuel,mil,marca,modello,vin,targa,km
    fprintf(f, "%llu,%s,%d,%d,%d,%d,%d,%s,%s,%s,%s,%ld\n",
        s->timestamp_us, s->error_code, s->rpm, s->speed, s->temp, s->fuel, s->mil,
        s->marca, s->modello, s->vin, s->targa, s->km);
    fclose(f);
}

// --- CARICA TUTTI GLI ERRORI DAL FILE ---
int error_snapshot_load_all_from_file(error_snapshot_t* arr, int max_count) {
    FILE* f = fopen("/spiffs/snapshots.txt", "r");
    if (!f) return 0;
    int n = 0;
    while (!feof(f) && n < max_count) {
        char line[256];
        if (!fgets(line, sizeof(line), f)) break; // Legge una riga
        error_snapshot_t snap;
        memset(&snap, 0, sizeof(snap));
        // Parsing dei 12 campi separati da virgola
        int r = sscanf(line, "%llu,%15[^,],%d,%d,%d,%d,%d,%23[^,],%23[^,],%23[^,],%15[^,],%ld",
            &snap.timestamp_us, snap.error_code, &snap.rpm, &snap.speed,
            &snap.temp, &snap.fuel, &snap.mil,
            snap.marca, snap.modello, snap.vin, snap.targa, &snap.km
        );
        // Se tutti i campi sono stati letti correttamente, aggiunge all'array
        if (r == 12) {
            arr[n++] = snap;
        }
    }
    fclose(f);
    return n; // Numero di snapshot caricati
}

// --- CANCELLAZIONE FILE ERRORI ---
void error_snapshot_clear_file(void) {
    remove("/spiffs/snapshots.txt"); // Elimina il file
}
