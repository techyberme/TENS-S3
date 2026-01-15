#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "hbridge_driver.h"
#include "driver/mcpwm_prelude.h"

static const char *TAG = "H-Bridge"; 
static mcpwm_timer_handle_t timer = NULL;  //Base de tiempo
// static mcpwm_oper_handle_t oper = NULL;  //Bloque lógico
// static mcpwm_cmpr_handle_t cmpr = NULL;  //Comparador
// static mcpwm_gen_handle_t gen_a = NULL;
// static mcpwm_gen_handle_t gen_b = NULL;
void hbridge_init(uint32_t deadtime_ticks)
{
   // TIMER Definition
    ESP_LOGI(TAG, "Create timer and operator");
    mcpwm_timer_handle_t timer = NULL;
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT, // PLL Clock 160 MHz
        .resolution_hz = 10000000, //10 MHz Prescaler, 0.1us/ticks
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP, //just upwards
        .period_ticks = 2500, // 2500 ticks for 1 cycle, 10 MHz/ 2500= 4 KHz Timer
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    // ----- OPERATOR, Definition of channel ----- 
    ESP_LOGI(TAG, "Create operators");
    mcpwm_oper_handle_t operators = NULL;
    mcpwm_operator_config_t operator_config = {
        .group_id = 0,
    };
    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &operators));
    ESP_LOGI(TAG, "Connect operators to the same timer");
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(operators, timer));


    // -----Comparator-----//
    ESP_LOGI(TAG, "Create comparators");
    mcpwm_cmpr_handle_t comparators = NULL;
    mcpwm_comparator_config_t compare_config = {
        .flags.update_cmp_on_tez = true,
    };
    ESP_ERROR_CHECK(mcpwm_new_comparator(operators, &compare_config, &comparators));
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparators, 1250));  //50% Duty ratio

    

    // -----Generator-----//
    ESP_LOGI(TAG, "Create generators");
    mcpwm_gen_handle_t generators[2];
    mcpwm_generator_config_t gen_config = {};
    const int gen_gpios[2] = {HBRIDGE_GPIO_A,HBRIDGE_GPIO_B}; //recommended pins 
    for (int i=0;i<=1;i++){
        gen_config.gen_gpio_num = gen_gpios[i];
        ESP_ERROR_CHECK(mcpwm_new_generator(operators, &gen_config, &generators[i]));
    }

    // ====== Generator Action  ====== //
    ESP_LOGI(TAG, "Set generator action on timer and compare event");
    // PWM Start with HIGH State when Timer is 0 and LOW State when Comparators value is equal Timer, For MCPWM_TIMER_COUNT_MODE_UP
    //First generator
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generators[0],MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_compare_event(generators[0],MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparators, MCPWM_GEN_ACTION_LOW))); //el pin se pone a cero
    
    
    //Deadtime only can be assign one posedge or negedge for both PWM on the same operator.
    ESP_LOGI(TAG, "Setup deadtime");
    mcpwm_dead_time_config_t dt_config = {
        .posedge_delay_ticks = deadtime_ticks,
        .negedge_delay_ticks = 0
    };
    ESP_ERROR_CHECK(mcpwm_generator_set_dead_time(generators[0],generators[0], &dt_config));
    dt_config = (mcpwm_dead_time_config_t) {
      .posedge_delay_ticks = 0,
      .negedge_delay_ticks = deadtime_ticks,
      .flags.invert_output = true,
    };
    ESP_ERROR_CHECK(mcpwm_generator_set_dead_time(generators[0],generators[1], &dt_config));  //generator 0 controls generator 1 so we don't need to define gen. 1
    ESP_LOGI(TAG, "Enable and start timer");
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));
}
void hbridge_stop(void){
    mcpwm_timer_start_stop(timer, MCPWM_TIMER_STOP_EMPTY);
}
     
