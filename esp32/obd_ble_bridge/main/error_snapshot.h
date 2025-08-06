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

// Salva uno snapshot con tutti i dati correnti in caso di errore
void error_snapshot_save(const char* error_code, int rpm, int speed, int temp, int fuel, int mil);

// Ritorna quanti snapshot sono attualmente salvati
int error_snapshot_get_count(void);

// Restituisce il puntatore allo snapshot all'indice idx (NULL se out of range)
const error_snapshot_t* error_snapshot_get(int idx);

#endif // ERROR_SNAPSHOT_H
