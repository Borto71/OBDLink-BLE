#ifndef ERROR_SNAPSHOT_H
#define ERROR_SNAPSHOT_H

#include <stdint.h>

#define MAX_ERROR_SNAPSHOTS 10

typedef struct {
    int rpm;
    int speed;
    int temp;
    int fuel;
    int mil;
    char error_code[16];
    uint64_t timestamp_us;
} error_snapshot_t;

// --- Gestione memoria volatile ---
void error_snapshot_save(const char* error_code, int rpm, int speed, int temp, int fuel, int mil);
int error_snapshot_get_count(void);
const error_snapshot_t* error_snapshot_get(int idx);

// --- Gestione su SPIFFS (persistente) ---
void error_snapshot_save_to_file(const error_snapshot_t* s);  // salva uno snapshot su file
int error_snapshot_load_all_from_file(error_snapshot_t* arr, int max_count);  // carica tutti gli snapshot salvati, ritorna quanti letti
void error_snapshot_clear_file(void); // cancella il file snapshot

#endif // ERROR_SNAPSHOT_H
