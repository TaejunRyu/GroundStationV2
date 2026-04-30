#include "motor_controller.h"
#include "esp_log.h"

static const char* TAG = "MOTOR";

// Singleton 템플릿 특수화
MotorDriver* MotorDriver::_instance = nullptr;
void* MotorDriver::_mutex = nullptr;

MotorDriver::MotorDriver() : _timer(nullptr) {}

MotorDriver::~MotorDriver() {
    stopAll();
    // 정리 로직 추가 가능
}

esp_err_t MotorDriver::init(int m1_pin, int m2_pin, int m3_pin, int m4_pin) {
    if (_instance != nullptr) {
        return ESP_OK;
    }
    _instance = new MotorDriver();
    return _instance->init(m1_pin, m2_pin, m3_pin, m4_pin);
}
    int pins[4] = {m1_pin, m2_pin, m3_pin, m4_pin};

    // 1. MCPWM 타이머 설정 (50Hz - 표준 변속기 기준, OneShot은 더 높게 설정 가능)
    mcpwm_timer_config_t timer_conf = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000, // 1MHz (1us 해상도)
        .period_ticks = 20000,    // 20ms (50Hz)
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_conf, &_timer));

    for (int i = 0; i < 4; i++) {
        // 2. 오퍼레이터 설정
        mcpwm_operator_config_t oper_conf = {.group_id = 0};
        ESP_ERROR_CHECK(mcpwm_new_operator(&oper_conf, &_operators[i]));
        ESP_ERROR_CHECK(mcpwm_operator_connect_timer(_operators[i], _timer));

        // 3. 비교기(Comparator) 설정
        mcpwm_comparator_config_t cmpr_conf = {.flags = {.update_cmp_on_tez = true}};
        ESP_ERROR_CHECK(mcpwm_new_comparator(_operators[i], &cmpr_conf, &_comparators[i]));
        mcpwm_comparator_set_compare_value(_comparators[i], MIN_PULSE);

        // 4. 제너레이터 설정 (GPIO 연결)
        mcpwm_generator_config_t gen_conf = {.gen_gpio_num = pins[i]};
        ESP_ERROR_CHECK(mcpwm_new_generator(_operators[i], &gen_conf, &_generators[i]));

        // PWM 파형 로직: 0에서 High, 비교값에서 Low
        ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(_generators[i], 
            MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
        ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(_generators[i], 
            MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, _comparators[i], MCPWM_GEN_ACTION_LOW)));
    }

    ESP_ERROR_CHECK(mcpwm_timer_enable(_timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(_timer, MCPWM_TIMER_START_NO_STOP));

    return ESP_OK;
}

void MotorDriver::setOutputs(float m1, float m2, float m3, float m4) {
    float outputs[4] = {m1, m2, m3, m4};
    for (int i = 0; i < 4; i++) {
        // 0.0 ~ 1.0 입력을 1000us ~ 2000us로 변환
        uint32_t pulse = MIN_PULSE + (uint32_t)(outputs[i] * (MAX_PULSE - MIN_PULSE));
        if (pulse > MAX_PULSE) pulse = MAX_PULSE;
        if (pulse < MIN_PULSE) pulse = MIN_PULSE;
        mcpwm_comparator_set_compare_value(_comparators[i], pulse);
    }
}

void MotorDriver::stopAll() {
    setOutputs(0, 0, 0, 0);
}

void MotorDriver::deinit() {
    if (_instance != nullptr) {
        delete _instance;
        _instance = nullptr;
    }
}
