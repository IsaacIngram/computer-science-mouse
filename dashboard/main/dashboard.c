#include <stdio.h>
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_now.h"

#define TRAP_1_CONNECTED_PIN 27
#define TRAP_1_HAMMER_STATUS_PIN 33
#define BUILT_IN_LED_PIN 2

void configure_pins() {
    gpio_config_t led_conf = {
            .intr_type = GPIO_INTR_DISABLE,
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = (1ULL << TRAP_1_CONNECTED_PIN) | (1ULL << TRAP_1_HAMMER_STATUS_PIN),
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE
    };
    gpio_config(&led_conf);
}


void app_main(void)
{
    configure_pins();
    while(1) {
        gpio_set_level(TRAP_1_CONNECTED_PIN, 1);
        gpio_set_level(TRAP_1_HAMMER_STATUS_PIN, 1);
    }
}
