#include <driver/gpio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_sleep.h"

// Pin definitions
#define HAMMER_SENSOR_PIN 23
#define LED_PIN 2

#define STATUS_UPDATE_DELAY_US 900000000


uint8_t peer_mac[ESP_NOW_ETH_ALEN] = {0x94, 0xB5, 0x55, 0x8E, 0x2A, 0x20};

static const char* TAG = "Trap";


/**
 * Configure pins. This should be called after every wake up from deep sleep.
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
    ESP_LOGD(TAG, "Configured GPIO");
}


/**
 * Check if the hammer is down.
 * @return True if the hammer is down, false otherwise.
 */
bool is_hammer_down() {
    return gpio_get_level(HAMMER_SENSOR_PIN);
}


/**
 * Initialize NVS. This should be called after every wake up from deep sleep.
 */
void init_nvs() {
    esp_err_t ret;
    ret = nvs_flash_init();
    while (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    ESP_LOGD(TAG, "Flash initialized");
}


/**
 * Initialize Wifi. This should be called after every wake up from deep sleep.
 */
void init_wifi() {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&cfg) != ESP_OK || esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK || esp_wifi_start() != ESP_OK || esp_now_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Wifi or ESP-NOW.");
    } else {
        ESP_LOGD(TAG, "Wifi and ESP-NOW initialized");
    }
}


void enter_deep_sleep() {
    // Disable Wifi
    esp_wifi_stop();
    // De-init flash
    nvs_flash_deinit();
    // Set wake up conditions
    esp_sleep_enable_timer_wakeup(STATUS_UPDATE_DELAY_US); // Wake on timer
    // Wake on hammer going down, if hammer is not already down.
    if (!is_hammer_down()) {
        esp_sleep_enable_ext0_wakeup(HAMMER_SENSOR_PIN, 1);
    }
    ESP_LOGD(TAG, "Entering deep sleep");
    esp_deep_sleep_start();
}


/**
 * Initialize everything that needs to be reinitialized after waking up from
 * deep sleep. This includes GPIO, NVS, WiFi, and ESP-NOW.
 */
void init_all_after_wake() {
    ESP_LOGD(TAG, "Waking from sleep");
    // Configure GPIO pins
    configure_pins();

    // Initialize NVS
    init_nvs();

    // Initialize Wifi
    init_wifi();
}


/**
 * Send the current status over ESP-NOW.
 * @param status
 */
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
    ESP_LOGD(TAG, "Sent status over ESP-NOW");
}


/**
 * Main task to send the status over ESP-NOW. Runs forever, sending status and
 * then entering deep sleep. It wakes from deep sleep and sends a status update
 * after the hammer switches to the "down" position or a 15 minute timer
 * expires.
 * @param pvParameters
 */
_Noreturn void send_status_task(void *pvParameters) {
    while(1) {
        if (is_hammer_down()) {
            gpio_set_level(LED_PIN, 1);
            send_status_esp_now(true);
        } else {
            gpio_set_level(LED_PIN, 0);
            send_status_esp_now(false);
        }
        enter_deep_sleep();
        // After waking up:
        init_all_after_wake();
    }
}


void app_main(void)
{
    // Initialize everything
    init_all_after_wake();

    // Start status tasks
    xTaskCreate(&send_status_task, "send_status_task", 2048, NULL, 5, NULL);
    ESP_LOGD(TAG, "Created send_status_task task");
}
