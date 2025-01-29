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
#include "esp_spiffs.h"

#define TRAP_1_CONNECTED_PIN 27
#define TRAP_1_HAMMER_STATUS_PIN 33
#define TRAP_2_CONNECTED_PIN 0
#define TRAP_2_HAMMER_STATUS_PIN 0
#define BUILT_IN_LED_PIN 2

#define CONNECTION_TIMEOUT_MICRO_S 1000000

int64_t trap_1_last_comm_micro_s = 0;

const char* TAG = "Dashboard";
char* cert_content = NULL;
const char* twilio_cert_file_path = "/spiffs/twilio_cert.pem";

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

typedef struct {
    char *message;
    char *recipient_number;
} SendSmsTaskParams;


void twilio_send_sms(void *pvParameters) {

    SendSmsTaskParams *params = (SendSmsTaskParams*)pvParameters;

    char *message = params->message;
    char *recipient_number = params->recipient_number;

    // Create twilio URL
    char twilio_url[200];
    snprintf(
            twilio_url,
            sizeof(twilio_url),
            "%s%s%s",
            "https://api.twilio.com/2010-04-01/Accounts/",
            TWILIO_ACCOUNT_SID,
            "/Messages"
            );

    // Create data packet
    char post_data[200];
    snprintf(
            post_data,
            sizeof(post_data),
            "%s%s%s%s%s%s",
            "To=",
            recipient_number,
            "&From=",
            SENDER_NUMBER,
            "&Body=",
            message
            );

    // Configure HTTP client
    esp_http_client_config_t config = {
            .url = twilio_url,
            .method = HTTP_METHOD_POST,
            .auth_type = HTTP_AUTH_TYPE_BASIC,
            .cert_pem = cert_content
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    esp_http_client_set_username(client, TWILIO_ACCOUNT_SID);
    esp_http_client_set_password(client, TWILIO_AUTH_TOKEN);

    // Send the message
    ESP_LOGI(TAG, "post=%s", post_data);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_err_t err = esp_http_client_perform(client);

    // Check for errors
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code == 201) {
            ESP_LOGI(TAG, "Twilio message sent successfully");
        } else {
            ESP_LOGI(TAG, "Twilio message send failure");
        }
    } else {
        ESP_LOGI(TAG, "Twilio message failed to send (ESP issue)");
    }

    // Cleanup
    esp_http_client_cleanup(client);
    free(params);
    free(cert_content);
    vTaskDelete(NULL);
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


void read_cert_from_file(const char* file_path) {
    FILE *file = fopen(file_path, "r");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", file_path);
    }

    fseek(file, 0, SEEK_END);
    long fsize = ftell(file);
    fseek(file, 0, SEEK_SET);

    cert_content = malloc(fsize + 1);
    if (cert_content == NULL) {
        ESP_LOGE(TAG, "Memory allocation failed for certificate");
        fclose(file);
    }

    fread(cert_content, 1, fsize, file);
    cert_content[fsize] = '\0';  // Null terminate the string

    fclose(file);
}


esp_err_t init_spiffs() {
    esp_vfs_spiffs_conf_t conf = {
            .base_path = "/spiffs",
            .partition_label = NULL,
            .max_files = 5,
            .format_if_mount_failed = true
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS filesystem (esp_err_t %d)", ret);
    } else {
        ESP_LOGI(TAG, "SPIFFS filesystem mounted");
    }
    return ESP_OK;
}


int retry_num = 0;
static void wifi_event_handler(void * event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_id == WIFI_EVENT_STA_START) {
        printf("Wifi connecting...\n");
    } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
        printf("Wifi connected\n");
        retry_num = 0;
        gpio_set_level(BUILT_IN_LED_PIN, 1);

    } else if(event_id == WIFI_EVENT_STA_DISCONNECTED) {
        printf("Wifi lost connection\n");
        esp_wifi_connect();
        printf("Trying to reconnect... (%d)\n", retry_num);
        // Flash LED based on the retry number
        if(retry_num % 2 == 0) {
            gpio_set_level(BUILT_IN_LED_PIN, 0);
        } else {
            gpio_set_level(BUILT_IN_LED_PIN, 1);
        }
        retry_num++;
    } else if (event_id == IP_EVENT_STA_GOT_IP) {
        printf("Wifi got IP\n");
        SendSmsTaskParams *params = malloc(sizeof(SendSmsTaskParams));
        params->message = "Hello, world!";
        params->recipient_number = RECEIVER_NUMBER;
        xTaskCreate(twilio_send_sms, "Twilio Send SMS", 4096, (void*)params, 5, NULL);
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

    // Initialize SPIFFS
    ret = init_spiffs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS initialization failed");
        return;
    }

    // Read certificate from file
    read_cert_from_file(twilio_cert_file_path);
    if (cert_content == NULL) {
        ESP_LOGE(TAG, "Failed to read certificate from file");
        return;
    }

    // Connect to Wifi
    wifi_connection(WIFI_SSID, WIFI_PASS);

    // Initialize ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());

    // Register callback for receiving ESP-NOW data
    ESP_ERROR_CHECK(esp_now_register_recv_cb(esp_now_recv_cb));

    // Start connection watchdog
    //xTaskCreate(&connection_watchdog_task, "connection_watchdog_task", 2048, NULL, 5, NULL);
    ESP_LOGD(TAG, "Created connection_watchdog_task task");


}
