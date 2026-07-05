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
#include "adc_mon_oneshot.h"
#include "boost_control.h"
#include "tensOS.h"
#include "oled.h"
#include "wifi.h"

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
//variable to signal deinit
s_monitor_bool = false;
adc_oneshot_unit_handle_t adc2_handle;
static TaskHandle_t s_monitor_task_handle = NULL; 
 
 

void monitor_task(void *pvParameters) {
int raw_val = 0; 
    
    // Variables para la batería
    uint32_t sum_bat_raw = 0;
    uint32_t count_bat = 0;

    // 1 kHz sampling
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(1); 

    while (s_monitor_bool) {
        if (cali_handle == NULL) {
            ESP_LOGE(TAG, "No calibration.");
            vTaskDelay(pdMS_TO_TICKS(100)); // Espera larga si hay error crítico
            continue; 
        }

        // --- 1. Lectura Corriente Canal A ---
        if (adc_oneshot_read(adc2_handle, ADC_CURR_A, &raw_val) == ESP_OK) {
            process_current(raw_val, 'A');
        }

        // --- 2. Lectura Corriente Canal B ---
        if (adc_oneshot_read(adc2_handle, ADC_CURR_B, &raw_val) == ESP_OK) {
            process_current(raw_val, 'B');
        }

        // --- 3. Lectura Voltaje Canal A ---
        if (adc_oneshot_read(adc2_handle, ADC_VOL_A, &raw_val) == ESP_OK) {
            process_voltage(raw_val, 'A');
        }

        // --- 4. Lectura Voltaje Canal B ---
        if (adc_oneshot_read(adc2_handle, ADC_VOL_B, &raw_val) == ESP_OK) {
            process_voltage(raw_val, 'B');
        }

        // --- 5. Lectura Batería (con promediado) ---
        if (adc_oneshot_read(adc2_handle, ADC_BAT, &raw_val) == ESP_OK) {
            sum_bat_raw += raw_val;
            count_bat++;
            
            // 32 lecturas para promediar
            if (count_bat >= 32) {
                uint32_t avg_bat_raw = sum_bat_raw / 32;
                int volt_mv;
                adc_cali_raw_to_voltage(cali_handle, avg_bat_raw, &volt_mv);
                
                // Transformar milivoltios a % usando tu tabla e interpolación
                batt_percentage = calc_percentage(volt_mv);
                
                // Resetear contadores
                sum_bat_raw = 0;
                count_bat = 0;
            }
        }

        // --- CRÍTICO: Ceder control al RTOS ---
        // Al no haber interrupciones DMA, si no bloqueas la tarea, el ESP32 crasheará por inanición (Starvation)
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
    s_monitor_task_handle = NULL;
    vTaskDelete(NULL);
}

// Tarea ADC


void adc_monitor_init(void) {
    ESP_LOGI(TAG, "Configurando ADC en modo One-Shot...");
    
    // Inicialización calibración (se mantiene igual)
    adc_calibrate_init();
    
    // 1. Inicializar la unidad ADC2
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_2,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc2_handle));

    // 2. Definir configuraciones según atenuación
    adc_oneshot_chan_cfg_t config_curr = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_CURRENT,
    };
    
    adc_oneshot_chan_cfg_t config_vol = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_VOL,
    };

    // 3. Aplicar configuración a canales únicos (no hace falta repetir como en el pattern)
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc2_handle, ADC_CURR_A, &config_curr));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc2_handle, ADC_CURR_B, &config_curr));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc2_handle, ADC_VOL_A, &config_vol));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc2_handle, ADC_VOL_B, &config_vol));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc2_handle, ADC_BAT, &config_vol));

    // 4. Definición de la tarea que ahora tendrá que hacer "polling"
    xTaskCreate(monitor_task, "Monitor", 4096, NULL, 10, &s_monitor_task_handle);
    s_monitor_bool = true;
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

//stop 
void adc_stop(void){
    if (s_monitor_task_handle != NULL) {
    s_monitor_bool = false;
    //make sure task is done
    while (s_monitor_task_handle != NULL) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    ESP_LOGI(TAG, "ADC TASK STOPPED");  
    }
    if (adc2_handle != NULL){
        ESP_ERROR_CHECK(adc_oneshot_del_unit(adc2_handle));
        adc2_handle = NULL;
        ESP_LOGI(TAG, "ADC2 freed");
    }
    if (cali_handle != NULL){
        ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(cali_handle));
        cali_handle = NULL;
        ESP_LOGI(TAG, "Calib freed");
    }
    wifi_init();
}
void adc_calibrate_init(void) {
    ESP_LOGI(TAG, "Configuring calibration...");
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_2,
        .atten = ADC_ATTEN_DB_6,           
        .bitwidth = ADC_BITWIDTH_12,
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