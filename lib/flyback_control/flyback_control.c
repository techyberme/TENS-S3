#include "driver/i2c.h"
#include "flyback_control.h"
#include "esp_log.h"
// Configuración I2C
#define I2C_MASTER_SCL_IO    8    // Ajusta según tus pines
#define I2C_MASTER_SDA_IO    9
#define I2C_MASTER_NUM       I2C_NUM_0  //EScojo el primer puerto I2C
#define I2C_MASTER_FREQ_HZ   100000 // 400kHz para rapidez
void flyback_init(void) {
    esp_err_t err;
    // 1. Inicializar bus I2C
    ESP_LOGE("INIT", "inicializando");
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,    //La esp32-s3 es el máster
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE, //Activación de resistencias internas para estabilidad
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) {
        ESP_LOGE("INIT", "Error en i2c_param_config: %s", esp_err_to_name(err));
    }
    err = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);  //0,0,0 es el tamaño buffers rx,tx y flags.
                                                             //Datos cortos, no hace falta buffer adicional
if (err != ESP_OK) {
        ESP_LOGE("INIT", "Error en i2c_driver_install: %s", esp_err_to_name(err));
    }
    // Configuración Pin Enable
    gpio_config_t io_conf = { 
        .pin_bit_mask = (1ULL << FLYBACK_EN_GPIO),  //Config al pin 21 
        .mode = GPIO_MODE_OUTPUT,  //output
        .pull_up_en = 1 // Por seguridad, el pin a 3.3V. Apaga el flyback
    };
    gpio_config(&io_conf);
    gpio_set_level(FLYBACK_EN_GPIO, 1); // Empezamos apagados
}

void set_DAC_value(uint16_t value) {
    if (value > 4095) value = 4095;
    // Protocolo MCP4725: [C2,C1,C0,X,X,PD1,PD0,X] + [D11...D4] + [D3...D0,X,X,X,X]
    // Para escritura rápida:
    uint8_t data[2]; //I2C envía paquetes de 
    //Primer paquete, cojo los 4 MSB y escojo modo escritura rápida
    data[0] = (value >> 8) & 0x0F; 
    //Segundo paquete, 8LSB
    data[1] = value & 0xFF;        // 8 bits LSB
    
    esp_err_t err =  i2c_master_write_to_device(I2C_MASTER_NUM, MCP4725_ADDR, data, 2, pdMS_TO_TICKS(10)); //Time out de 10 ms, por si el bus está bloqueado


    // Verificamos el resultado
    if (err != ESP_OK) {
        ESP_LOGE("DAC_I2C", "Error de escritura: %s (Direccion: 0x%02X)", esp_err_to_name(err), MCP4725_ADDR);
    }
}
void flyback_enable(bool enable) {
    gpio_set_level(FLYBACK_EN_GPIO, !enable); // Encendido del Flyback
}
void flyback_stop() {
    gpio_set_level(FLYBACK_EN_GPIO, 1); // Encendido del Flyback
} 