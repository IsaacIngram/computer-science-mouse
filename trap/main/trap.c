#include <stdio.h>
#include <driver/gpio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_now.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

// Pin definitions
#define HAMMER_SENSOR_PIN 23
#define LED_PIN 2

#define HAMMER_POLL_DELAY_MS 500

uint8_t peer_mac[ESP_NOW_ETH_ALEN] = {0x94, 0xB5, 0x55, 0x8E, 0x2A, 0x20};

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
            .pull_down_en = GPIO_PULLDOWN_ENABLE
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


void send_status_esp_now(bool status) {
    esp_now_peer_info_t peer_info = {
            .channel = 0,
            .ifidx = ESP_IF_WIFI_STA,
            .encrypt = false,
    };
    memcpy(peer_info.peer_addr, peer_mac, ESP_NOW_ETH_ALEN);

    // Add peer if not already added
    if (!esp_now_is_peer_exist(peer_mac)) {
        esp_now_add_peer(&peer_info);
    }
    esp_now_send(peer_mac, (uint8_t*)&status, sizeof(status));
    printf("Sent ESP-NOW message.\n");
}


/**
 * Task to repeatedly check the hammer status and set LEDs accordingly
 * @param pvParameters
 */
_Noreturn void check_hammer_task(void *pvParameters) {
    while(1) {
        if (is_hammer_down()) {
            gpio_set_level(LED_PIN, 1);
            send_status_esp_now(true);
        } else {
            gpio_set_level(LED_PIN, 0);
            send_status_esp_now(false);
        }
    }
}

void init_esp_now() {
    esp_err_t ret;

    // Initialize NVS
    ret = nvs_flash_init();
    while (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    printf("Flash initialized");

    // Initialize Wi-Fi
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    printf("WiFi initialized\n");

    // Initialize ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());
}


void app_main(void)
{
    // Configure all GPIO pins
    configure_pins();

    init_esp_now();

    // Start hammer task
    xTaskCreate(&check_hammer_task, "check_hammer_task", 2048, NULL, 5, NULL);
}
