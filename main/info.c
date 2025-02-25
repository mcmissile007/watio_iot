#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "info.h"
#include "config.h"

#define TAG "info.c"

void system_info()
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "This is ESP8266 chip with %d CPU cores, WiFi, ", chip_info.cores);
    ESP_LOGI(TAG, "silicon revision %d, ", chip_info.revision);
    ESP_LOGI(TAG, "%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
             (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
}

void memory_info(const char *msg)
{
    UBaseType_t uxHighWaterMark;
    uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI(TAG, "%s,taskuxHighWaterMark: %d\n", msg, uxHighWaterMark);
    // The value returned is the high water mark in words
    // on a 32 bit machine a return value of 1 would indicate that 4 bytes of stack were unused
    uint32_t free_heap_size = 0, min_free_heap_size = 0;
    free_heap_size = esp_get_free_heap_size();
    min_free_heap_size = esp_get_minimum_free_heap_size();
    ESP_LOGI(TAG, "\n %s,free heap size = %d \t  min_free_heap_size = %d \n", msg, free_heap_size, min_free_heap_size);
    // minimum free heap size that was ever seen during program execution
}

int set_config_str(const char *namespace, const char *key, const char *value)
{
    nvs_handle_t nvs_handle_conf;
    esp_err_t retval;
    retval = nvs_open(namespace, NVS_READWRITE, &nvs_handle_conf);
    if (retval != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_open:%s", esp_err_to_name(retval));
        return ESP_FAIL;
    }
    retval = nvs_set_str(nvs_handle_conf, key, value);

    if (retval != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_set_str error:%s", esp_err_to_name(retval));
        return ESP_FAIL;
    }
    return ESP_OK;
}
int commit_info(const char *namespace)
{
    nvs_handle_t nvs_handle_conf;
    esp_err_t retval;
    retval = nvs_open(namespace, NVS_READWRITE, &nvs_handle_conf);
    if (retval != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_open:%s", esp_err_to_name(retval));
        return ESP_FAIL;
    }
    retval = nvs_commit(nvs_handle_conf);
    if (retval != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_commit:%s", esp_err_to_name(retval));
        return ESP_FAIL;
    }
    return ESP_OK;
}

char *get_config_str(const char *namespace, const char *key)
{

    size_t required_size;
    nvs_handle_t nvs_handle_conf;
    esp_err_t retval;

    char *value = NULL;

    retval = nvs_open(namespace, NVS_READONLY, &nvs_handle_conf);
    if (retval != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_open:%s", esp_err_to_name(retval));
        return NULL;
    }

    retval = nvs_get_str(nvs_handle_conf, key, NULL, &required_size);

    if (retval != ESP_OK)
    {
        ESP_LOGE(TAG, "first nvs_get_str:%s", esp_err_to_name(retval));
        return NULL;
    }
    // required_size include '\0'
    value = (char *)malloc(required_size);
    memset(value, 0, required_size);

    retval = nvs_get_str(nvs_handle_conf, key, value, &required_size);

    if (retval != ESP_OK)
    {
        ESP_LOGE(TAG, "second nvs_get_str:%s", esp_err_to_name(retval));
        return NULL;
    }

    value[required_size - 1] = '\0'; // really not necessary with memset

    ESP_LOGD(TAG, "value len: %d\n", required_size);
    ESP_LOGD(TAG, "value: %s\n", value);
    return value;
}

bool is_wifi_configured()
{
    char *wifi_ssid = NULL;
    char *wifi_pass = NULL;
    bool retval = false;
    wifi_ssid = (char *)get_config_str(CONFIG_NAMESPACE, CONFIG_WIFI_SSID_KEY);
    wifi_pass = (char *)get_config_str(CONFIG_NAMESPACE, CONFIG_WIFI_PASS_KEY);
    if (wifi_ssid && wifi_pass)
    {
        ESP_LOGI(TAG, "wifi configured -> ssid:%s password:%s", wifi_ssid, wifi_pass);
        retval = true;
    }
    else
    {
        ESP_LOGI(TAG, "wifi not configured.");
        retval = false;
    }
    free(wifi_pass);
    free(wifi_ssid);
    return retval;
}