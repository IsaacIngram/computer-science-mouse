///////////////////////////////////////////////////////////////////////////////
//
// File: dashboard.c
//
// Author: Isaac Ingram
//
// Purpose: Control the LED dashboard for multiple mousetraps. Create a mesh
// network with all traps via ESP-NOW.
//
///////////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"

#define TRAP_1_CONNECTED_PIN 27
#define TRAP_1_HAMMER_STATUS_PIN 33
#define TRAP_2_CONNECTED_PIN 0
#define TRAP_2_HAMMER_STATUS_PIN 0
#define BUILT_IN_LED_PIN 2

#define CONNECTION_TIMEOUT_MICRO_S 1000000

int64_t trap_1_last_comm_micro_s = 0;

const char* TAG = "Dashboard";


/**
 * Configure GPIO pins
 */
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


/**
 * Callback for when a message is received over ESP-NOW
 * @param info
 * @param data
 * @param data_len
 */
void esp_now_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int data_len) {
    if(data == NULL || data_len == 0) {
        // Empty data
        return;
    }
    if(data_len == sizeof(bool)) {
        gpio_set_level(TRAP_1_CONNECTED_PIN, 1);
        bool hammer_down = *(bool *)data;
        gpio_set_level(TRAP_1_HAMMER_STATUS_PIN, hammer_down ? 1 : 0);
        trap_1_last_comm_micro_s = esp_timer_get_time();
    }
}


/**
 * Task for connection watchdog. Note that this never returns.
 * @param pvParameters
 */
_Noreturn void connection_watchdog_task(void *pvParameters) {
    while(1) {
        if(esp_timer_get_time() >= trap_1_last_comm_micro_s + CONNECTION_TIMEOUT_MICRO_S) {
            gpio_set_level(TRAP_1_CONNECTED_PIN, 0);
        }
    }
}


/**
 * Main function
 */
void app_main(void)
{
    configure_pins();    // Set up GPIO
    ESP_LOGD(TAG, "GPIO Initialized");

    // Initialize NVS
    esp_err_t ret;
    ret = nvs_flash_init();
    while (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    ESP_LOGD(TAG, "Flash initialized");

    // Initialize Wi-Fi
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGD(TAG, "WiFi initialized");

    // Initialize ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());

    // Register callback for receiving ESP-NOW data
    ESP_ERROR_CHECK(esp_now_register_recv_cb(esp_now_recv_cb));

    // Start connection watchdog
    xTaskCreate(&connection_watchdog_task, "connection_watchdog_task", 2048, NULL, 5, NULL);
    ESP_LOGD(TAG, "Created connection_watchdog_task task");
}
