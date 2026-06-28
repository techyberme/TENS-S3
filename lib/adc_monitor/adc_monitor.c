//RTOS maneja las tareas de las interrupciones
#include "freertos/FreeRTOS.h" 
#include "freertos/semphr.h" //gestiona la sincro entre partes del código
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_adc/adc_continuous.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "adc_monitor.h"
#include "boost_control.h"
#include "tensOS.h"
#include "oled.h"


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

extern TensChannel_t ch_A;
extern TensChannel_t ch_B;
float batt_percentage = 0;
bool isCharging = false;
static const char *TAG = "ADC";
static adc_cali_handle_t cali_handle = NULL;
static adc_continuous_handle_t handle = NULL;

static TaskHandle_t s_monitor_task_handle = NULL; 
 
 
// When frame is completed, callback is triggered
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
    uint8_t result[256]; //  conv_frame_size
    //length of buffer
    uint32_t ret_num = 0;  
    //local batt variable
    uint32_t sum_bat_raw = 0;
    uint32_t count_bat = 0;
    while (1) {
        if(ulTaskNotifyTake(pdTRUE, portMAX_DELAY)){
            if (cali_handle == NULL) {
                    ESP_LOGE(TAG, "No calibration.");
                    continue; // Evita el crash
                }
            // Buffer reading
            esp_err_t ret = adc_continuous_read(handle, result, 256, &ret_num, 0);
            
            if (ret == ESP_OK) {
               for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) {
                //take data and metadata
                    adc_digi_output_data_t *p = (adc_digi_output_data_t *)&result[i];
                    uint32_t chan = p->type2.channel;
                    uint32_t val = p->type2.data; 

                    // Current processing
                    if (chan == ADC_CURR_A) {
                        process_current(val, 'A'); 
                    } 
                    else if (chan == ADC_CURR_B) {
                        process_current(val, 'B');
                    }
                    //Voltage processing
                    else if (chan == ADC_VOL_A) {
                        process_voltage(val, 'A');
                    }
                    else if (chan == ADC_VOL_B) {
                        process_voltage(val, 'B');
                    }
                    // Battery voltage processing
                    else if (chan == ADC_BAT) {
                        sum_bat_raw += val;
                        count_bat++;
                        
                        // 32 batches
                        if (count_bat >= 32) {
                            uint32_t avg_bat_raw = sum_bat_raw / 32;
                            int volt_mv;
                            adc_cali_raw_to_voltage(cali_handle, avg_bat_raw, &volt_mv);
                            
                            // Transformar milivoltios a % usando tu tabla e interpolación
                            batt_percentage = calc_percentage(volt_mv);
                            
                            // reset vars
                            sum_bat_raw = 0;
                            count_bat = 0;
                        }
                    }
               }
            }
        }
    }
}

// Tarea ADC


void adc_monitor_init(void) {
    ESP_LOGI(TAG, "Configurando ADC...");
    //Inicialización calibración
    adc_calibrate_init();
    
    // Configuración del Driver Continuo
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 1024, // buffer in the ram
        .conv_frame_size = 256,     // 256/(4 bytes/muestra) = 64 muestras que es envían a la cpu en batch, interrupción
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle)); //el handle es la dirección de la ram

    // Configuración del Hardware (Canal y Velocidad)
    adc_continuous_config_t config = {
        .sample_freq_hz = 70000, // 70 kHz, 10 kHz for each loop
        .conv_mode = ADC_CONV_SINGLE_UNIT_2,  //ADC 2
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2, // Formato Tipo 2 para el S3
    };

    adc_digi_pattern_config_t adc_pattern[7] = {
        { .atten = ADC_ATTEN_CURRENT, .channel = ADC_CURR_A, .unit = ADC_UNIT_2, .bit_width = ADC_BITWIDTH_DEFAULT },
        { .atten = ADC_ATTEN_CURRENT, .channel = ADC_CURR_B, .unit = ADC_UNIT_2, .bit_width = ADC_BITWIDTH_DEFAULT },
        { .atten = ADC_ATTEN_CURRENT, .channel = ADC_CURR_A, .unit = ADC_UNIT_2, .bit_width = ADC_BITWIDTH_DEFAULT },
        { .atten = ADC_ATTEN_CURRENT, .channel = ADC_CURR_B, .unit = ADC_UNIT_2, .bit_width = ADC_BITWIDTH_DEFAULT },
        { .atten = ADC_ATTEN_VOL, .channel = ADC_VOL_A,  .unit = ADC_UNIT_2, .bit_width = ADC_BITWIDTH_DEFAULT },
        { .atten = ADC_ATTEN_VOL, .channel = ADC_VOL_B,  .unit = ADC_UNIT_2, .bit_width = ADC_BITWIDTH_DEFAULT },
        { .atten = ADC_ATTEN_VOL, .channel = ADC_BAT,    .unit = ADC_UNIT_2, .bit_width = ADC_BITWIDTH_DEFAULT }
    };

    config.pattern_num = 7;    
    config.adc_pattern = &adc_pattern[0];

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

void charging_monitor_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << 2),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}


 
void adc_calibrate_init(void) {
    ESP_LOGI(TAG, "Configuring calibration...");
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_2,
        .atten = ADC_ATTEN_DB_6,           
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    // Efuse reading
    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle);
    
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "No efuses detected");
    }
}

void process_current(uint32_t raw_val, char channel) {
    int volt_mv = 0;
    
    //Convert value
    if (cali_handle != NULL) {
        adc_cali_raw_to_voltage(cali_handle, raw_val, &volt_mv);
    } else {
        return; 
    }


    float current_ma = (float)volt_mv / 24.9f;

   
    if (channel == 'A') {
        ch_A.current = current_ma; 
        
        // Overcurrent
        if (current_ma > 50.0f) { 
            boost_stop(ERR_OVERCURRENT);
            ESP_LOGE("ADC_CURR", "¡Overcurrent in Channel A! %.2f mA. Shutdown Flyback.", current_ma);
        }
    } 
    else if (channel == 'B') {
        ch_B.current = current_ma; 
        
         if (current_ma > 50.0f) { 
            boost_stop(ERR_OVERCURRENT);
            ESP_LOGE("ADC_CURR", "¡Overcurrent in Channel B! %.2f mA. Shutdown Flyback.", current_ma);
        }
    }
}

void process_voltage(uint32_t raw_val, char channel) {
    int volt_mv = 0;
    
    if (cali_handle != NULL) {
        adc_cali_raw_to_voltage(cali_handle, raw_val, &volt_mv);
    } else {
        return;
    }

    static uint32_t sum_v_a = 0, count_v_a = 0;
    static uint32_t sum_v_b = 0, count_v_b = 0;

    // Division factor
    const float div_factor = (91.0f + 10.0f) / 10.0f;

    switch (channel) {
        case 'A':
            sum_v_a += volt_mv;
            count_v_a++;
            //take 32 samples
            if (count_v_a >= 32) {
                float avg_mv = (float)sum_v_a / 32.0f;
                // conversion
                ch_A.voltage = (avg_mv / 1000.0f) * div_factor;
                // Reset
                sum_v_a = 0;
                count_v_a = 0;
            }
            break;

        case 'B':
            sum_v_b += volt_mv;
            count_v_b++;
            
            if (count_v_b >= 32) {
                float avg_mv = (float)sum_v_b / 32.0f;
                ch_B.voltage  = (avg_mv / 1000.0f) * div_factor;
                
                sum_v_b = 0;
                count_v_b = 0;
            }
            break;
            
        default:
            break;
    }
}

float calc_percentage(int volt){
    const float div_factor = (75.0f + 15.0f) / 15.0f;
    float updated_volt = (volt) * div_factor;
    if (updated_volt >= battery_curve[0][0]) return 100.0;
    if (updated_volt <= battery_curve[8][0]) return 0.;

    // Interpolation
    for (int i = 0; i < 8; i++) {
        float v_high = battery_curve[i][0];
        float p_high = battery_curve[i][1];
        float v_low  = battery_curve[i+1][0];
        float p_low  = battery_curve[i+1][1];
        if (updated_volt <= v_high && updated_volt >= v_low) {
            float percentage = p_low + (updated_volt - v_low) * (p_high - p_low) / (v_high - v_low);
            return percentage;
        }
    }
    return 0.;
}



void charge_task(void *pvParameters) {
    const TickType_t xFrequency = pdMS_TO_TICKS(2000); // Check every 2 secs
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        if (gpio_get_level(CHARGE_PIN) == 0) { 
            //Check again just in case
            vTaskDelay(pdMS_TO_TICKS(50)); 
            
            if (gpio_get_level(CHARGE_PIN) == 0) { 
                ESP_LOGW("POWER", "Charge detected, stopping");
                display_charge_shutdown_warning();
                boost_stop(ERR_CHARGE);
            }
            
        }
    }
 }