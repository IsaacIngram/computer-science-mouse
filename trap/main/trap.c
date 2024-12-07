#include <stdio.h>
#include <driver/gpio.h>
#include "freertos/FreeRTOS.h"

// Pin definitions
#define HAMMER_SENSOR_PIN 23
#define LED_PIN 2

#define HAMMER_POLL_DELAY_MS 500

/**
 * Configure pins
 */
void configure_pins() {
    // Hammer sensor
    gpio_config_t hammer_sensor_conf = {
            .intr_type = GPIO_INTR_DISABLE,
            .mode = GPIO_MODE_INPUT,
            .pin_bit_mask = 1ULL << HAMMER_SENSOR_PIN,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE
    };
    gpio_config(&hammer_sensor_conf);
    // LED pin
    gpio_config_t led_pin_conf = {
            .intr_type = GPIO_INTR_DISABLE,
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << LED_PIN,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .pull_up_en = GPIO_PULLUP_DISABLE
    };
    gpio_config(&led_pin_conf);
}

/**
 * Check if the hammer is down
 * @return
 */
bool is_hammer_down() {
    return gpio_get_level(HAMMER_SENSOR_PIN);
}

/**
 * Task to repeatedly check the hammer status and set LEDs accordingly
 * @param pvParameters
 */
_Noreturn void check_hammer_task(void *pvParameters) {
    while(1) {
        if (is_hammer_down()) {
            gpio_set_level(LED_PIN, 1);
        } else {
            gpio_set_level(LED_PIN, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(HAMMER_POLL_DELAY_MS));
    }
}


void app_main(void)
{
    // Configure all GPIO pins
    configure_pins();

    // Start hammer task
    xTaskCreate(&check_hammer_task, "check_hammer_task", 2048, NULL, 5, NULL);
}
