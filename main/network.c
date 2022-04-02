#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "freertos/event_groups.h"
#include "esp_http_server.h"

#include "network.h"
#include "info.h"
#include "config.h"
#include "http_server.h"

#define TAG "network.c"

#define WIFI_AP_START_BIT BIT0
#define WIFI_AP_STOP_BIT BIT1

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

void config_wifi_softap_mode()
{
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = SOFT_AP_SSID,
            .password = SOFT_AP_PASS,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK},
    };

    // https://stackoverflow.com/questions/66554351/esp8266-12f-wifi-soft-ap-config-authmode-failed
    if (strlen(SOFT_AP_PASS) < 8)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
        ESP_LOGI(TAG, " pw-lenght under 8 charcters. Set WIFI_AUTH_OPEN");
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
}

void config_ADAPTER_IF_AP()
{
    ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
    tcpip_adapter_ip_info_t ipAddressInfo;
    memset(&ipAddressInfo, 0, sizeof(ipAddressInfo));
    IP4_ADDR(
        &ipAddressInfo.ip,
        SOFT_AP_IP_ADDRESS_1,
        SOFT_AP_IP_ADDRESS_2,
        SOFT_AP_IP_ADDRESS_3,
        SOFT_AP_IP_ADDRESS_4);
    IP4_ADDR(
        &ipAddressInfo.gw,
        SOFT_AP_GW_ADDRESS_1,
        SOFT_AP_GW_ADDRESS_2,
        SOFT_AP_GW_ADDRESS_3,
        SOFT_AP_GW_ADDRESS_4);
    IP4_ADDR(
        &ipAddressInfo.netmask,
        SOFT_AP_NM_ADDRESS_1,
        SOFT_AP_NM_ADDRESS_2,
        SOFT_AP_NM_ADDRESS_3,
        SOFT_AP_NM_ADDRESS_4);

    ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &ipAddressInfo));
    ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));
}

static void softAP_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{

    ESP_LOGI(TAG, "event softAP:%d", event_id);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START)
    {
        ESP_LOGI(TAG, "WIFI_EVENT_AP_START:%d", event_id);
        xEventGroupSetBits(s_wifi_event_group, WIFI_AP_START_BIT);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STOP)
    {
        ESP_LOGI(TAG, "WIFI_EVENT_AP_STOP:%d", event_id);
        xEventGroupSetBits(s_wifi_event_group, WIFI_AP_STOP_BIT);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        ESP_LOGI(TAG, "WIFI_EVENT_AP_STACONNECTED:%d", event_id);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        ESP_LOGI(TAG, "WIFI_EVENT_AP_STADISCONNECTED:%d", event_id);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_AP_STAIPASSIGNED)
    {
        ESP_LOGI(TAG, "IP_EVENT_AP_STAIPASSIGNED:%d", event_id);
        wifi_sta_list_t station_list;
        esp_wifi_ap_get_sta_list(&station_list);
        wifi_sta_info_t *stations = (wifi_sta_info_t *)station_list.sta;
        for (int i = 0; i < station_list.num; ++i)
        {
            ip4_addr_t addr;
            dhcp_search_ip_on_mac(stations[i].mac, &addr);
            ESP_LOGI(TAG, "station ip:%s", ip4addr_ntoa(&addr));
        }
    }
    else
    {
        ESP_LOGI(TAG, "EVENT REGISTER BUT NOT HANLDE:%d", event_id);
    }
}

static void register_wifi_events()
{
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_START, &softAP_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STOP, &softAP_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, &softAP_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &softAP_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &softAP_event_handler, NULL));
}
static void unregister_wifi_events()
{
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_AP_START, &softAP_event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_AP_STOP, &softAP_event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, &softAP_event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &softAP_event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &softAP_event_handler));
}

static void softAPWebServer_task(void *pvParameters)
{

    memory_info("init softAPWebServer_task");
    // a special type of loop used for system events (Wi-Fi events, for example).
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // register events to handles
    register_wifi_events();

    // the required RAM is automatically allocated from the FreeRTOS heap
    s_wifi_event_group = xEventGroupCreate();
    if (!s_wifi_event_group)
    {
        esp_restart();
    }

    config_ADAPTER_IF_AP();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    config_wifi_softap_mode();
    ESP_ERROR_CHECK(esp_wifi_start());

    for (;;)
    {

        /* Waiting until either the softAP is established (WIFI_AP_START_BIT) or stopped (WIFI_AP_STOP_BIT).
         The bits are set by softAP_event_handler() (see above) */
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                               WIFI_AP_START_BIT | WIFI_AP_STOP_BIT,
                                               pdTRUE,  // the bits set in the event group are not altered when the call to xEventGroupWaitBits() returns.
                                               pdFALSE, // will return when any of the bits
                                               portMAX_DELAY);

        if (bits & WIFI_AP_START_BIT)
        {
            ESP_LOGI(TAG, "started softAP with SSID:%s password:%s",
                     SOFT_AP_SSID, SOFT_AP_PASS);
            start_webserver();
            memory_info("after start_webserver softAPWebServer_task");
        }
        else if (bits & WIFI_AP_STOP_BIT)
        {
            ESP_LOGI(TAG, "Stopped softAP");
            stop_webserver();
            memory_info("after stop_webserver softAPWebServer_task");
        }
        else
        {
            ESP_LOGE(TAG, "UNEXPECTED EVENT");
        }
    }

    unregister_wifi_events();
    vEventGroupDelete(s_wifi_event_group);
    vTaskDelete(NULL);
}

void start_softApWebServer()
{
    memory_info(TAG);
    xTaskCreate(&softAPWebServer_task, "softAPWebServer_task", 4096, NULL, 5, NULL);
}