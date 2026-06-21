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




void init_nvs(void);


void write_doctor(const doctor_data_t *doctor_data);


doctor_data_t read_doctor(void);

#endif 