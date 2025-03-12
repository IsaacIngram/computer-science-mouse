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
#include <string.h>
#include <stdlib.h>
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "secrets.h"

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
            .pin_bit_mask = 1ULL << BUILT_IN_LED_PIN,
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
    // TODO implementing parsing ESP-NOW data
}


/**
 * Task for connection watchdog. Note that this never returns.
 * @param pvParameters
 */
_Noreturn void connection_watchdog_task(void *pvParameters) {
    while(1) {
        // TODO implement connection watchdog task
    }
}


int retry_num = 0;
static void wifi_event_handler(void * event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_id == WIFI_EVENT_STA_START) {
        ESP_LOGD(TAG, "Wifi connecting...");
    } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGD(TAG, "Wifi connected");
        retry_num = 0;
        gpio_set_level(BUILT_IN_LED_PIN, 1);
    } else if(event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGD(TAG, "Wifi lost connection.");
        esp_wifi_connect();
        ESP_LOGD(TAG, "Trying to reconnect... (%d)", retry_num);
        // Flash LED based on the retry number
        if(retry_num % 2 == 0) {
            gpio_set_level(BUILT_IN_LED_PIN, 0);
        } else {
            gpio_set_level(BUILT_IN_LED_PIN, 1);
        }
        retry_num++;
    } else if (event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGD(TAG, "Wifi got IP. Ready to send messages!");
    }
}


/**
 * Connect to Wifi
 */
void wifi_connection(const char* wifi_ssid, const char* wifi_pass) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_cfg_default = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_cfg_default);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    wifi_config_t wifi_cfg = {
            .sta.ssid = "",
            .sta.password = "",
    };
    strcpy((char*)wifi_cfg.sta.ssid, wifi_ssid);
    strcpy((char*)wifi_cfg.sta.password, wifi_pass);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_cfg);
    esp_wifi_start();
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_connect();
    ESP_LOGD(TAG, "WiFi initialized");
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

    // Connect to Wifi
    wifi_connection(WIFI_SSID, WIFI_PASS);

    // Initialize ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());

    // Register callback for receiving ESP-NOW data
    ESP_ERROR_CHECK(esp_now_register_recv_cb(esp_now_recv_cb));

    // Start connection watchdog
    xTaskCreate(&connection_watchdog_task, "connection_watchdog_task", 2048, NULL, 5, NULL);
    ESP_LOGD(TAG, "Created connection_watchdog_task task");


}
