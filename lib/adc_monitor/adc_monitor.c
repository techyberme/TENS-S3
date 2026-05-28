//RTOS maneja las tareas de las interrupciones
#include "freertos/FreeRTOS.h" 
#include "freertos/semphr.h" //gestiona la sincro entre partes del código
#include "freertos/task.h"
#include "esp_adc/adc_continuous.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "adc_monitor.h"
#include "flyback_control.h"


static const float battery_curve[9][2] = {
    {8400, 100.0},
    {8300, 95.0},
    {8220, 90.0},
    {8040, 80.0},
    {7680, 50.0},
    {7600, 40.0},
    {7460, 20.0},
    {7380, 10.0},
    {6400, 0.0}   
};

volatile float current_ma_global = 0.0f; //volátil para que lo lea siempre
static const char *TAG = "ADC";
static adc_cali_handle_t cali_handle = NULL;
static adc_continuous_handle_t handle = NULL;
static TaskHandle_t s_monitor_task_handle = NULL; // Handle de la tarea que procesará los datos
// Nuevos handles para la lectura de voltaje
static adc_oneshot_unit_handle_t volt_adc_handle;
static adc_cali_handle_t volt_cali_handle = NULL;

// Se dispara el callback cuando se llena el frame
static bool IRAM_ATTR adc_conv_done_cb(adc_continuous_handle_t handle, 
                                       const adc_continuous_evt_data_t *edata, 
                                       void *user_data) {
    BaseType_t mustYield = pdFALSE;
    // Analizamos la prioridad de la tarea
    vTaskNotifyGiveFromISR(s_monitor_task_handle, &mustYield);
    //TRUE: Máxima prioridad
    //FALSE: Mínima Prioridad
    return (mustYield == pdTRUE);
}

void monitor_task(void *pvParameters) {
    uint8_t result[256]; // Coincide con conv_frame_size
    uint32_t ret_num = 0;  //la función de lectura lo rellena con la longitud del buffer

    while (1) {
        // Bloqueo eficiente CPU, (…, tiempo de espera eterno)
        if(ulTaskNotifyTake(pdTRUE, portMAX_DELAY)){
            if (cali_handle == NULL) {
                    ESP_LOGE(TAG, "No calibratoin.");
                    continue; // Evita el crash
                }
            // Buffer reading
            esp_err_t ret = adc_continuous_read(handle, result, 256, &ret_num, 0);
            
            if (ret == ESP_OK) {
                uint32_t max_raw = 0;
                int sum_volt = 0; 
                int current_volt = 0;
                int max_volt = 0;
                int num_meas = ret_num / SOC_ADC_DIGI_RESULT_BYTES;
                for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) {
                    adc_digi_output_data_t *p = (void*)&result[i];
                    uint32_t val = p->type2.data; 
                        //peak current detection
                        if (val > max_raw) {
                        max_raw = val; 
                    }

                    // Calibration of each measurement
                    adc_cali_raw_to_voltage(cali_handle, val, &current_volt);
                    sum_volt += current_volt;
                }
                //avg reading
                int avg_volt = sum_volt / num_meas;
                //calibration max reading
                adc_cali_raw_to_voltage(cali_handle, max_raw, &max_volt); 

                float max_current = (float)(max_volt) / 25.0f; // Rsense = 25 ohm
                float avg_current = (float)(avg_volt) / 25.0f;
                current_ma_global = avg_current;
                // SEGURIDAD CRÍTICA
                if (max_current > 50.0f) { // Ejemplo: Límite 50mA
                    flyback_stop(ERR_OVERCURRENT);
                }
                                
            }
        }
    }
}
void current_monitor_calibrate_init(void) {
    ESP_LOGI(TAG, "Configurando esquema de calibración...");
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_CURRENT,           
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    // Esto lee los eFuses internos del S3 y crea la curva matemática
    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle);
    
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "Error al crear esquema de calibración. ¿eFuses no grabados?");
    }
}
// Tarea ADC


void current_monitor_init(void) {
    ESP_LOGI(TAG, "Configurando ADC...");
    //Inicialización calibración
    current_monitor_calibrate_init();
    
    // Configuración del Driver Continuo
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 1024, // Tamaño del buffer en RAM, almacena las muestras antes de que se pierdan
        .conv_frame_size = 256,     // 256/(4 bytes/muestra) = 64 muestras que es envían a la cpu en batch, interrupción
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle)); //el handle es la dirección de la ram

    // Configuración del Hardware (Canal y Velocidad)
    adc_continuous_config_t config = {
        .sample_freq_hz = 20 * 1000, // 20kHz (5 veces la freq del puente en H)
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,  //ADC 1
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2, // Formato Tipo 2 para el S3
    };

    adc_digi_pattern_config_t adc_pattern = {
        .atten = ADC_ATTEN_CURRENT,
        .channel = ADC_CHANNEL_2, // GPIO 3 en S3
        .unit = ADC_UNIT_1,
        .bit_width = ADC_BITWIDTH_12,
    };

    config.pattern_num = 1;    //solo un canal ADC
    config.adc_pattern = &adc_pattern;

    ESP_ERROR_CHECK(adc_continuous_config(handle, &config));
    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = adc_conv_done_cb, //para conv. terminada, vamos con la función
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));
    //Definición tarea
    xTaskCreate(monitor_task, "Monitor", 4096, NULL, 10, &s_monitor_task_handle);
    // Arrancar la captación automática
    ESP_ERROR_CHECK(adc_continuous_start(handle));
}

void voltage_monitor_init() {
    // 1. Configuración de la unidad ADC2
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_2,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &volt_adc_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12, // Rango hasta ~3.1V para cubrir tus 80V escalados
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(volt_adc_handle, ADC_VOL_A, &config)); 
    ESP_ERROR_CHECK(adc_oneshot_config_channel(volt_adc_handle, ADC_VOL_B, &config)); 
    ESP_ERROR_CHECK(adc_oneshot_config_channel(volt_adc_handle, ADC_BAT, &config)); 
    // Calib
    voltage_monitor_calibrate_init();
    
}

void voltage_monitor_calibrate_init(void) {
    ESP_LOGI(TAG, "Configuring calibration...");
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_2,
        .atten = ADC_ATTEN_DB_12,           
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    // Efuse reading
    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, &volt_cali_handle);
    
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "Error al crear esquema de calibración. ¿eFuses no grabados?");
    }
}

float get_voltage(char channel) {
    adc_channel_t chan;
    if (volt_cali_handle == NULL) {
        ESP_LOGE(TAG, "Error: No calibration available");
        return -1.0f; 
    }
    if (channel == 'A'){
       chan = ADC_VOL_A;
    }
    if (channel == 'B'){
       chan = ADC_VOL_B;
    }
    int raw_val;
    int voltage_mv;
    float sum = 0;
    const int num_samples = 32;
    for (int i = 0; i < num_samples; i++) {
        ESP_ERROR_CHECK(adc_oneshot_read(volt_adc_handle, chan, &raw_val));
        if (volt_cali_handle) {
            adc_cali_raw_to_voltage(volt_cali_handle, raw_val, &voltage_mv);
            sum += voltage_mv;
        }
    }
    
    float avg_mv = sum / num_samples;
    
    // V_real = V_adc * (R_high + R_low) / R_low
    float factor = (75.0f + 20.0f) / 10.0f;
    
    return (avg_mv / 1000.0f) * factor; 
}

uint8_t get_battery(void) {
    int raw_val;
    int voltage_mv;
    float sum = 0;
    const int num_samples = 32; //a bit more robust
    for (int i = 0; i < num_samples; i++) {
        ESP_ERROR_CHECK(adc_oneshot_read(volt_adc_handle, ADC_BAT, &raw_val));
        if (volt_cali_handle) {
            adc_cali_raw_to_voltage(volt_cali_handle, raw_val, &voltage_mv);
            sum += voltage_mv;
        }
    }
    //conversion to battery voltage
    float v_bat = sum / num_samples * 5;
    

    // 2. Límites absolutos (Saturación)
    if (v_bat >= battery_curve[0][0]) return 100;
    if (v_bat <= battery_curve[8][0]) return 0;

    // Interpolation
    for (int i = 0; i < 8; i++) {
        float v_high = battery_curve[i][0];
        float p_high = battery_curve[i][1];
        float v_low  = battery_curve[i+1][0];
        float p_low  = battery_curve[i+1][1];
        if (v_bat <= v_high && v_bat >= v_low) {
            float percentage = p_low + (v_bat - v_low) * (p_high - p_low) / (v_high - v_low);
            return (uint8_t)percentage;
        }
    }
    return 0; 
}

