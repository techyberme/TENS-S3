#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

/* Estructura de datos almacenados en NVS */
typedef struct {
    bool doctor;
    uint32_t frequency;
    uint32_t deadtime;
    uint8_t mode;
    uint32_t burst_hz;
    uint32_t duration;
} doctor_data_t;

typedef struct {
    uint8_t program_id;
    uint8_t duration;
    uint8_t avg_intensity_ch_a;
    uint8_t avg_intensity_ch_b;
} stats_t;



void init_nvs(void);


void write_doctor(const doctor_data_t *doctor_data);


doctor_data_t read_doctor(void);

void write_stats(uint8_t program_id, uint8_t duration, uint8_t avg_intensity_ch_a, uint8_t avg_intensity_ch_b);
void export_stats_to_serial(void);
#endif 