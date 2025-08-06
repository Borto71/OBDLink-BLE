#include "error_snapshot.h"
#include <string.h>
#include "esp_timer.h"  // Per esp_timer_get_time()

static error_snapshot_t snapshots[MAX_ERROR_SNAPSHOTS];
static int snapshot_count = 0;

void error_snapshot_save(const char* error_code, int rpm, int speed, int temp, int fuel, int mil) {
    if (snapshot_count >= MAX_ERROR_SNAPSHOTS) {
        // Shift left: cancella la più vecchia
        for (int i = 1; i < MAX_ERROR_SNAPSHOTS; ++i) {
            snapshots[i-1] = snapshots[i];
        }
        snapshot_count = MAX_ERROR_SNAPSHOTS - 1;
    }
    error_snapshot_t* s = &snapshots[snapshot_count++];
    s->rpm = rpm;
    s->speed = speed;
    s->temp = temp;
    s->fuel = fuel;
    s->mil = mil;
    strncpy(s->error_code, error_code, sizeof(s->error_code)-1);
    s->error_code[sizeof(s->error_code)-1] = 0;
    s->timestamp_us = esp_timer_get_time();
}

int error_snapshot_get_count(void) {
    return snapshot_count;
}

const error_snapshot_t* error_snapshot_get(int idx) {
    if (idx < 0 || idx >= snapshot_count) return NULL;
    return &snapshots[idx];
}
