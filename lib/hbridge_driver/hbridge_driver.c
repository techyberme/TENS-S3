#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/mcpwm_prelude.h"
#include "hbridge_driver.h"

static const char *TAG = "H-Bridge"; 
static mcpwm_oper_handle_t operators[2];
static mcpwm_cmpr_handle_t comparators[2];
static mcpwm_gen_handle_t generators[4];
static mcpwm_timer_handle_t timer = NULL;  //Time base
static tens_mode_t current_mode = TENS_MODE_CONTINUO;
static bool en_A = false;
static bool en_B = false;
volatile bool A_bridge_silence = false; //volatile to be constantly read.
volatile bool B_bridge_silence = false; 
//Ámbito global (fuera de las funciones)
static mcpwm_timer_handle_t timer1 = NULL;
static mcpwm_oper_handle_t oper_or = NULL;
static mcpwm_gen_handle_t gen_or = NULL;
static mcpwm_cmpr_handle_t cmp_up = NULL;
static mcpwm_cmpr_handle_t cmp_down = NULL;
static esp_timer_handle_t burst_timer = NULL;
// Fixed programs doctor
const tens_program_t PROGRAM_DATABASE[] = {
    [0] = { .frequency_hz = 4000, .deadtime_ticks = 50, .mode = TENS_MODE_CONTINUO, .burst_hz = 0 },
    [1] = { .frequency_hz = 2000,  .deadtime_ticks = 50, .mode = TENS_MODE_BURST,    .burst_hz = 100}, // 100Hz Burst
    [2] = { .frequency_hz = 6000, .deadtime_ticks = 50, .mode = TENS_MODE_CONTINUO, .burst_hz = 0 },
};
// Timer 100 Hz
static void burst_callback(void* arg) {
    static bool output_en = true;
    // If burst, commute
    if (output_en) {
        if (en_A) {
            mcpwm_generator_set_force_level(generators[0], -1, true);  //-1 turns off the force level.
            mcpwm_generator_set_force_level(generators[1], -1, true);
            //mcpwm_generator_set_force_level(gen_or, -1, true);
            
        }
        if (en_B){
            mcpwm_generator_set_force_level(generators[2], 0, true);  
            mcpwm_generator_set_force_level(generators[3], 0, true);
        }
        A_bridge_silence=false;
        B_bridge_silence=true;
    } else {
        if (en_A) {
            mcpwm_generator_set_force_level(generators[0], 0, true); 
            mcpwm_generator_set_force_level(generators[1], 0, true);
            //mcpwm_generator_set_force_level(gen_or, 0, true);
        }
        if (en_B){
            mcpwm_generator_set_force_level(generators[2], -1, true);  
            mcpwm_generator_set_force_level(generators[3], -1, true);
        }
        A_bridge_silence=true;
        B_bridge_silence=false;
    }
    output_en = !output_en;

}
void hbridge_init(const tens_program_t *prog)
{
    hbridge_deinit(); // Ensure previous configuration is cleared
    uint32_t timer_period = 10000000 / prog->frequency_hz; // 10 MHz / frequency
    //uint32_t timer_period = 2500; 
   // TIMER Definition
    ESP_LOGI(TAG, "Inicializando PWM a %lu Hz (Periodo: %lu ticks)", 
            prog->frequency_hz, timer_period);
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT, // PLL Clock 160 MHz
        .resolution_hz = 10000000, //10 MHz Prescaler, 0.1us/ticks
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP, //just upwards
        .period_ticks = timer_period, // 2500 ticks for 1 cycle, 10 MHz/ 2500= 4 KHz Timer
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));
    for (int i=0;i<2;i++){
        // ----- OPERATOR, Definition of channel ----- 
        ESP_LOGI(TAG, "Config operators");
        mcpwm_operator_config_t operator_config = {
            .group_id = 0,
        };
        ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &operators[i]));
        ESP_LOGI(TAG, "Connect operators to the same timer");
        ESP_ERROR_CHECK(mcpwm_operator_connect_timer(operators[i], timer));


        // -----Comparator-----//
        ESP_LOGI(TAG, "Configcomparators");
        mcpwm_comparator_config_t compare_config = {
            .flags.update_cmp_on_tez = true,
        };
        ESP_ERROR_CHECK(mcpwm_new_comparator(operators[i], &compare_config, &comparators[i]));
        //ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparators[i], timer_period/2));  //50% Duty ratio
        ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparators[i], 1250));
    }
    

    // -----Generator-----//
    ESP_LOGI(TAG, "Create generators");
    const int gen_gpios[4] = {HBRIDGE_GPIO_A1,HBRIDGE_GPIO_B1,HBRIDGE_GPIO_A2,HBRIDGE_GPIO_B2}; 
    for (int i=0;i<4;i++){
        mcpwm_generator_config_t gen_config = {.gen_gpio_num = gen_gpios[i]};
        //gens 0 and 1, operator 0 and 2 and 3, operator 1
        int oper_idx = i/2;
        ESP_ERROR_CHECK(mcpwm_new_generator(operators[oper_idx], &gen_config, &generators[i]));
        mcpwm_generator_set_force_level(generators[i], 0, true); //generators start stopped
    }
    
    // Generator action
    for (int i=0; i<2; i++){
    ESP_LOGI(TAG, "Set generator action on timer and compare event");
    int gen_idx = i * 2;
    //Timer Event
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generators[gen_idx],
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, 
        MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));  //changed to low from high
    //Comparator Event
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generators[gen_idx],
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, 
        comparators[i], MCPWM_GEN_ACTION_LOW)));  //Changed from low to high
    //deadtime config
    ESP_LOGI(TAG, "Setup deadtime");
    mcpwm_dead_time_config_t dt_config = {
        .posedge_delay_ticks = prog->deadtime_ticks,   //Changed from pos to negative
        .negedge_delay_ticks = 0
    };
    ESP_ERROR_CHECK(mcpwm_generator_set_dead_time(generators[gen_idx],generators[gen_idx], &dt_config));   

    dt_config = (mcpwm_dead_time_config_t) {
      .posedge_delay_ticks = 0,
      .negedge_delay_ticks = prog->deadtime_ticks, //Changed from neg to
      //.negedge_delay_ticks = 50, //Changed from neg to
      .flags.invert_output = true,
    };
    //Deadtime only can be assigned one posedge or negedge for both PWM on the same operator.
    //Generator 0 controls Generator 1 so we don't need to define gen. 1
    ESP_ERROR_CHECK(mcpwm_generator_set_dead_time(generators[gen_idx],generators[gen_idx + 1], &dt_config));  
    }
    
    // --- 100 Hz BURST ---
    //Pointer to interruption
    current_mode = prog->mode;
    if (current_mode == TENS_MODE_BURST) {
        ESP_LOGI(TAG, "creating burst");
        const esp_timer_create_args_t periodic_timer_args = {
            .callback = &burst_callback,
            .name = "tens_burst"
        };
        //Timer creation
        uint64_t period = 1000000ULL / (prog->burst_hz * 2);
        ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &burst_timer));
        ESP_ERROR_CHECK(esp_timer_start_periodic(burst_timer, period));
    }
    // 100 Hz -> T = 5000us
    
    mcpwm_timer_config_t timer1_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000, 
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
        .period_ticks = timer_period/2, // half main timer
    };
    
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer1_config, &timer1));
    // 2. Sincronizar fase: Timer 1 se reinicia cuando Timer 0 llega a cero (TEZ)
    mcpwm_sync_handle_t timer0_sync_src;
    mcpwm_timer_sync_src_config_t sync_src_config = {
        .timer_event = MCPWM_TIMER_EVENT_EMPTY, // Evento TEZ del Timer 0
    };
    ESP_ERROR_CHECK(mcpwm_new_timer_sync_src(timer, &sync_src_config, &timer0_sync_src));

    mcpwm_timer_sync_phase_config_t sync_phase_config = {
        .sync_src = timer0_sync_src,
        .count_value = 0,
        .direction = MCPWM_TIMER_DIRECTION_UP,
    };
    ESP_ERROR_CHECK(mcpwm_timer_set_phase_on_sync(timer1, &sync_phase_config));
    mcpwm_operator_config_t oper_or_config = {.group_id = 0};
    ESP_ERROR_CHECK(mcpwm_new_operator(&oper_or_config, &oper_or));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper_or, timer1));

    uint32_t margen_ticks = 50;  
    mcpwm_comparator_config_t cmp_config = {.flags.update_cmp_on_tez = true};

    ESP_ERROR_CHECK(mcpwm_new_comparator(oper_or, &cmp_config, &cmp_up));
    uint32_t compare_time = prog->deadtime_ticks + margen_ticks;
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(cmp_up,compare_time)); 

    // 4. Crear Generador y asignar acciones
    mcpwm_generator_config_t gen_or_config = {.gen_gpio_num = GPIO_OR};
    ESP_ERROR_CHECK(mcpwm_new_generator(oper_or, &gen_or_config, &gen_or));

    // At TEZ (tick 0): Both base signals become 1. NAND(1,1) = 0.
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(gen_or,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));

    // At cmp_up (deadtime_ticks): One base signal becomes 0. NAND(1,0) = 1.
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(gen_or,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, cmp_up, MCPWM_GEN_ACTION_LOW)));
    mcpwm_generator_set_force_level(gen_or, 1, true);
    // uint32_t margen_ticks = 5; // El tiempo que la señal OR "tarda" en bajar y "se adelanta" en subir

    // // Seguridad: Evitar underflow si el margen es mayor que la mitad del deadtime
    // if (deadtime_ticks <= (margen_ticks * 2)) {
    //     ESP_LOGE(TAG, "Margen demasiado grande para el deadtime actual");
    //     return;
    // }

    // mcpwm_comparator_config_t cmp_config = {.flags.update_cmp_on_tez = true};
    // ESP_ERROR_CHECK(mcpwm_new_comparator(oper_or, &cmp_config, &cmp_up));
    // ESP_ERROR_CHECK(mcpwm_new_comparator(oper_or, &cmp_config, &cmp_down));

    // // Flanco de BAJADA: Un poco después de empezar el DT (en el tick 5)
    // ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(cmp_down, margen_ticks)); 
    
    // // Flanco de SUBIDA: Un poco antes de terminar el DT (ej: si DT es 50, sube en 45)
    // ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(cmp_up, deadtime_ticks - margen_ticks));

    // mcpwm_generator_config_t gen_or_config = {.gen_gpio_num = GPIO_OR};
    // ESP_ERROR_CHECK(mcpwm_new_generator(oper_or, &gen_or_config, &gen_or));

    // // Configuración de acciones para crear el pulso invertido
    // // Queremos que la señal esté en ALTO por defecto
    // ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(gen_or,
    //     MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));

    // // Baja en el primer comparador
    // ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(gen_or,
    //     MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, cmp_down, MCPWM_GEN_ACTION_LOW)));

    // // Sube en el segundo comparador
    // ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(gen_or,
    //     MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, cmp_up, MCPWM_GEN_ACTION_HIGH)));

    ESP_LOGI(TAG, "Enable and start timer");
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer1)); 
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer1, MCPWM_TIMER_START_NO_STOP));

    
}
void hbridge_stop(char channel){
    if (channel == 'A'){
        mcpwm_generator_set_force_level(generators[0], 0, true);
        mcpwm_generator_set_force_level(generators[1], 0, true);
       // mcpwm_generator_set_force_level(gen_or, 0, true);
        en_A = false;
        }
    if (channel == 'B'){
        mcpwm_generator_set_force_level(generators[2], 0, true);
        mcpwm_generator_set_force_level(generators[3], 0, true);
        en_B = false;
        }
    //mcpwm_timer_start_stop(timer, MCPWM_TIMER_STOP_EMPTY);
}
void hbridge_start(char channel){   
    //enable A and forcing is turned off
    if (channel == 'A'){
        en_A = true;
        mcpwm_generator_set_force_level(generators[0], -1, true);
        mcpwm_generator_set_force_level(generators[1], -1, true);
        mcpwm_generator_set_force_level(gen_or, -1, true);
        }
    if (channel == 'B'){
        en_B = true;
        mcpwm_generator_set_force_level(generators[2], -1, true);
        mcpwm_generator_set_force_level(generators[3], -1, true);
        }
}


void hbridge_set_mode(tens_mode_t mode) {
    // Change from BURST to continous, make sure no gen is forced
    if (current_mode == TENS_MODE_BURST && mode != TENS_MODE_BURST) {
        for (int i = 0; i < 4; i++) {
            mcpwm_generator_set_force_level(generators[i], -1, true); 
        }
        A_bridge_silence = false;
        B_bridge_silence = false;
        ESP_LOGI(TAG, "Freed generators");
    }
    current_mode = mode;
}

void hbridge_deinit(void) {
    if (timer) mcpwm_timer_disable(timer);
    if (timer1) mcpwm_timer_disable(timer1);
    //Stop and delete the burst timer if it exists
    if (burst_timer != NULL) {
        esp_timer_stop(burst_timer);
        esp_timer_delete(burst_timer);
        burst_timer = NULL;
    }

    // Destroy generators
    for (int i = 0; i < 4; i++) {
        if (generators[i]) {
            mcpwm_del_generator(generators[i]);
            generators[i] = NULL;
        }
    }
    if (gen_or) {
        mcpwm_del_generator(gen_or);
        gen_or = NULL;
    }

    // Destroy comparators
    for (int i = 0; i < 2; i++) {
        if (comparators[i]) {
            mcpwm_del_comparator(comparators[i]);
            comparators[i] = NULL;
        }
    }
    if (cmp_up) { mcpwm_del_comparator(cmp_up); cmp_up = NULL; }
    if (cmp_down) { mcpwm_del_comparator(cmp_down); cmp_down = NULL; }

    // Destroy operators
    for (int i = 0; i < 2; i++) {
        if (operators[i]) {
            mcpwm_del_operator(operators[i]);
            operators[i] = NULL;
        }
    }
    if (oper_or) { mcpwm_del_operator(oper_or); oper_or = NULL; }

    // Eliminate timers
    if (timer) { mcpwm_del_timer(timer); timer = NULL; }
    if (timer1) { mcpwm_del_timer(timer1); timer1 = NULL; }
    
    ESP_LOGI(TAG, "Hardware PWM liberado correctamente");
}