
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// skd includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs.h"
#include "nvs_flash.h"
// app includes
#include "info.h"
#include "config.h"
#include "network.h"
#include "io.h"

#define TAG "main.c"

static led_status_t g_led_status;

void common_init()
{
    // used by many components.
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
}

void app_main()
{

    common_init();
    system_info();
    memory_info(TAG);
    setup_gpio();

    if (is_wifi_configured() && get_button_status() == UNPRESSED)
    {
        g_led_status = LED_FAST_BLINK;
        start_WifiSTA(&g_led_status);
    }
    else
    {
        g_led_status = LED_SLOW_BLINK;
        start_softAPWebServer();
    }

    while (1)
    {
        update_led_status(g_led_status);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
