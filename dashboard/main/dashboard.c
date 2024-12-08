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


void esp_now_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int data_len) {
    if(data == NULL || data_len == 0) {
        // Empty data
        return;
    }
    if(data_len == sizeof(bool)) {
        bool hammer_down = *(bool *)data;
        if(hammer_down) {
            gpio_set_level(TRAP_1_HAMMER_STATUS_PIN, 1);
        } else {
            gpio_set_level(TRAP_1_HAMMER_STATUS_PIN, 0);
        }
    }
}


void init_esp_now() {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_wifi_init(&(wifi_init_config_t){});
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    esp_now_init();
    esp_now_register_recv_cb(esp_now_recv_cb);
}


void app_main(void)
{
    init_esp_now();
    configure_pins();
    esp_now_register_recv_cb(esp_now_recv_cb);
}
