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
volatile bool A_bridge_silence = false; //volatile to be constantly read.
volatile bool B_bridge_silence = false; 

// Timer 100 Hz
static void burst_callback(void* arg) {
    static bool output_en = true;
    // If burst, commute
    if (current_mode == TENS_MODE_BURST) {
        if (output_en) {
            mcpwm_generator_set_force_level(generators[0], -1, true);  //-1 turns off the force level.
            mcpwm_generator_set_force_level(generators[1], -1, true);
            mcpwm_generator_set_force_level(generators[2], 0, true);  
            mcpwm_generator_set_force_level(generators[3], 0, true);
            A_bridge_silence=false;
            B_bridge_silence=true;
        } else {
            mcpwm_generator_set_force_level(generators[0], 0, true);
            mcpwm_generator_set_force_level(generators[1], 0, true);
            mcpwm_generator_set_force_level(generators[2], -1, true);
            mcpwm_generator_set_force_level(generators[3], -1, true);
            A_bridge_silence=true;
            B_bridge_silence=false;
        }
        output_en = !output_en;
    } 
}
void hbridge_init(uint32_t deadtime_ticks)
{
   // TIMER Definition
    ESP_LOGI(TAG, "Create timer and operator");
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT, // PLL Clock 160 MHz
        .resolution_hz = 10000000, //10 MHz Prescaler, 0.1us/ticks
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP, //just upwards
        .period_ticks = 2500, // 2500 ticks for 1 cycle, 10 MHz/ 2500= 4 KHz Timer
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
        ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparators[i], 1250));  //50% Duty ratio
    }
    

    // -----Generator-----//
    ESP_LOGI(TAG, "Create generators");
    const int gen_gpios[4] = {HBRIDGE_GPIO_A1,HBRIDGE_GPIO_B1,HBRIDGE_GPIO_A2,HBRIDGE_GPIO_B2}; 
    for (int i=0;i<4;i++){
        mcpwm_generator_config_t gen_config = {.gen_gpio_num = gen_gpios[i]};
        //gens 0 and 1, operator 0 and 2 and 3, operator 1
        int oper_idx = i/2;
        ESP_ERROR_CHECK(mcpwm_new_generator(operators[oper_idx], &gen_config, &generators[i]));
    }
    
    // ====== Generator Action  ====== //
    for (int i=0; i<2; i++){
    ESP_LOGI(TAG, "Set generator action on timer and compare event");
    int gen_idx = i * 2;
    //Timer Event
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generators[gen_idx],
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, 
        MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    //Comparator Event
    ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_compare_event(generators[gen_idx],
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, 
        comparators[i], MCPWM_GEN_ACTION_LOW))); 
    //deadtime config
    ESP_LOGI(TAG, "Setup deadtime");
    mcpwm_dead_time_config_t dt_config = {
        .posedge_delay_ticks = deadtime_ticks,
        .negedge_delay_ticks = 0
    };
    ESP_ERROR_CHECK(mcpwm_generator_set_dead_time(generators[gen_idx],generators[gen_idx], &dt_config));   

    dt_config = (mcpwm_dead_time_config_t) {
      .posedge_delay_ticks = 0,
      .negedge_delay_ticks = deadtime_ticks,
      .flags.invert_output = true,
    };
    //Deadtime only can be assigned one posedge or negedge for both PWM on the same operator.
    //Generator 0 controls Generator 1 so we don't need to define gen. 1
    ESP_ERROR_CHECK(mcpwm_generator_set_dead_time(generators[gen_idx],generators[gen_idx + 1], &dt_config));  
    }
    
    // --- 100 Hz BURST ---
    //Pointer to interruption
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &burst_callback,
        .name = "tens_burst"
    };
    esp_timer_handle_t burst_timer;
    //Timer creation
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &burst_timer));
    
    // 100 Hz -> T = 5000us
    ESP_ERROR_CHECK(esp_timer_start_periodic(burst_timer, 5000));

    ESP_LOGI(TAG, "Enable and start timer");
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));
}
void hbridge_stop(void){
    mcpwm_generator_set_force_level(generators[0], 0, true);
    mcpwm_generator_set_force_level(generators[1], 0, true);
    mcpwm_generator_set_force_level(generators[2], 0, true);
    mcpwm_generator_set_force_level(generators[3], 0, true);
    mcpwm_timer_start_stop(timer, MCPWM_TIMER_STOP_EMPTY);
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