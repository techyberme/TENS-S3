#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

/* Estructura de datos almacenados en NVS */
typedef struct {
    bool doctor;
    int program;
    int duration;
} doctor_data_t;


void init_nvs(void);


void write_doctor(bool doctor, int program, int duration);


doctor_data_t read_doctor(void);

#endif 